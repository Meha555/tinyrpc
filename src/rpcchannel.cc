#include "rpcchannel.h"
#include "tinyrpcheader.pb.h"
#include "zookeeperutil.h"
#include <arpa/inet.h>
#include <cerrno>
#include <format>
#include <glog/logging.h>

using namespace meha;

void RpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                            ::google::protobuf::RpcController *controller,
                            const ::google::protobuf::Message *request,
                            ::google::protobuf::Message *response,
                            ::google::protobuf::Closure *done)
{
    // 如果没有和服务提供者连接过，说明还不知道服务提供者的ip:port
    if (-1 == m_clientfd) {
        // 获取服务对象和方法名
        const google::protobuf::ServiceDescriptor *sd = method->service();
        m_service_name = sd->name();
        m_method_name = method->name();
        // rpc调用方也就是客户端想要调用服务器上服务对象提供的方法，需要查询zk上该服务所在的host信息。
        ZkClient zkCli;
        zkCli.Start(); // start返回就代表成功连接上zk服务器了
        auto host_data = QueryServiceHost(&zkCli, m_service_name, m_method_name);
        if (!host_data) {
            controller->SetFailed(std::format("query service {}/{} data error!", m_service_name, m_method_name));
            LOG(ERROR) << "query service " << m_service_name << " method " << m_method_name << " error";
            return;
        }
        std::tie(m_ip, m_port) = *host_data;
        LOG(INFO) << "RpcProvider data: " << m_ip << ":" << m_port;
        auto rt = ConnectTo(m_ip, m_port);
        if (!rt) {
            controller->SetFailed("connect to server error");
            LOG(ERROR) << "connect to server error";
            return;
        } else {
            LOG(INFO) << "connect to server success";
        }
    }

    // 获取参数的序列化字符串长度args_size
    std::string args_str;
    if (!request->SerializeToString(&args_str)) {
        controller->SetFailed("serialize request fail");
        LOG(ERROR) << "serialize request fail";
        return;
    }
    // 定义rpc的报文header
    tinyrpc::RpcHeader krpcheader;
    krpcheader.set_service_name(m_service_name);
    krpcheader.set_method_name(m_method_name);
    krpcheader.set_args_size(args_str.size());

    std::string header_str;
    if (!krpcheader.SerializeToString(&header_str)) {
        controller->SetFailed("serialize rpc header error!");
        LOG(ERROR) << "serialize rpc header error!";
        return;
    }
    std::string send_rpc_str;
    {
        google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
        google::protobuf::io::CodedOutputStream coded_output(&string_output);
        coded_output.WriteVarint32(static_cast<uint32_t>(header_str.size()));
        coded_output.WriteString(header_str);
    }
    send_rpc_str += args_str;
    //  打印调试信息
    // LOG(INFO) << "============================================";
    // LOG(INFO) << "header_size: " << header_str.size();
    // LOG(INFO) << "rpc_header_str: " << header_str;
    // LOG(INFO) << "service_name: " << m_service_name;
    // LOG(INFO) << "method_name: " << m_method_name;
    // LOG(INFO) << "args_str: " << args_str;
    // LOG(INFO) << "============================================";

    // 发送rpc的请求
    if (-1 == send(m_clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0)) {
        close(m_clientfd);
        char errtxt[512] = {};
        LOG(ERROR) << "send request error: " << strerror_r(errno, errtxt, sizeof(errtxt));
        controller->SetFailed(std::format("send request error: {}", errtxt));
        return;
    }
    // 接收rpc请求的响应值 // TODO 这里改成支持异步api
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(m_clientfd, recv_buf, sizeof(recv_buf), 0))) {
        char errtxt[512] = {};
        LOG(ERROR) << "recv retval error" << strerror_r(errno, errtxt, sizeof(errtxt));
        controller->SetFailed(std::format("recv retval error: {}", errtxt));
        return;
    }
    // 反序列化rpc调用响应数据
    if (!response->ParseFromArray(recv_buf, recv_size)) { // TODO 为什么这里用ParseFromArray
        close(m_clientfd);
        char errtxt[512] = {};
        LOG(INFO) << "parse retval error" << strerror_r(errno, errtxt, sizeof(errtxt));
        controller->SetFailed(std::format("parse retval error: {}", errtxt));
        return;
    }
}

bool RpcChannel::ConnectTo(const std::string &ip, uint16_t port)
{
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd) {
        LOG(ERROR) << "socket create error: " << google::StrError(errno);
        return false;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    if (-1 == connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
        close(clientfd);
        LOG(ERROR) << "connect server " << ip << ":" << port << " errror: " << google::StrError(errno);
        return false;
    }
    m_clientfd = clientfd;
    return true;
}

std::optional<std::pair<std::string, uint16_t>> RpcChannel::QueryServiceHost(ZkClient *zkclient, std::string service_name, std::string method_name)
{
    std::string method_path = "/meha/" + service_name + "/" + method_name;
    LOG(INFO) << "method_path: " << method_path;
    auto host_data = zkclient->GetNodeData(method_path);
    if (!host_data) {
        LOG(ERROR) << method_path + " is not exist!";
        return std::nullopt;
    }
    int idx = (*host_data).find(":");
    if (idx == -1) {
        LOG(ERROR) << method_path + " address is invalid!";
        return std::nullopt;
    }
    std::string ip = (*host_data).substr(0, idx);
    uint16_t port = std::atoi((*host_data).substr(idx + 1, (*host_data).size() - idx).c_str());
    return std::make_pair(ip, port);
}

RpcChannel::RpcChannel()
    : m_clientfd(-1)
{
}

RpcChannel::~RpcChannel()
{
    if (-1 != m_clientfd) {
        close(m_clientfd);
    }
}