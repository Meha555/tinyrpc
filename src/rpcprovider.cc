#include "rpcprovider.h"
#include "common.h"
#include "rpcconfig.h"
#include "tinyrpcheader.pb.h"
#include "zookeeperutil.h"
#include <glog/logging.h>
#include <iostream>

using namespace meha;

RpcProvider::RpcProvider(const std::string &package)
{
    ZkClient zkclient;
    zkclient.Start();
    std::string toplevel = "/" + package;
    zkclient.CreateNode(toplevel.c_str(), "", ZkClient::CreateMode::Persistent);
    LOG(INFO) << "zk create " << toplevel << " as toplevel node";
}

/// 这个函数用于注册服务对象和其对应的 RPC 方法，以便服务端处理客户端的请求。
/// 参数类型设置为 google::protobuf::Service，是因为所有由 protobuf 生成的服务类
/// 都继承自 google::protobuf::Service，这样我们可以通过基类指针指向子类对象，实现动态多态。
void RpcProvider::RegisterService(google::protobuf::Service *service)
{
    // 服务端需要知道对方想要调用的具体服务对象和方法，所以服务端需要维护一张表来存储方法名和服务对象之间的关系
    // 这些信息会保存在一个数据结构（ServiceInfo）中。
    ServiceInfo service_info;

    // GetDescriptor() 方法会返回 protobuf 生成的服务类的描述信息（ServiceDescriptor）。
    const google::protobuf::ServiceDescriptor *psd = service->GetDescriptor();

    // 通过 ServiceDescriptor，我们可以获取该服务类中定义的方法列表，并进行相应的注册和管理。

    // 获取服务的名字
    std::string service_name = psd->name();
    // 获取服务端对象service的方法数量
    int method_count = psd->method_count();

    // 打印service_name
    LOG(INFO) << "service_name=" << service_name;

    for (int i = 0; i < method_count; ++i) {
        // 获取服务中的方法描述
        const google::protobuf::MethodDescriptor *pmd = psd->method(i);
        std::string method_name = pmd->name();
        LOG(INFO) << "method_name=" << method_name;
        service_info.method_map.emplace(method_name, pmd);
    }
    service_info.service = service;
    m_service_map.emplace(service_name, service_info);
}

void RpcProvider::Run()
{
    // 读取配置文件rpcserver的信息
    std::string ip = RpcConfig::Instance().Lookup("rpcserver_ip").value_or("127.0.0.1");
    std::string port = RpcConfig::Instance().Lookup("rpcserver_port").value_or("8000");

    // 使用muduo网络库，创建address对象
    muduo::net::InetAddress address(ip, std::atoi(port.c_str()));

    // 创建tcpserver对象
    muduo::net::TcpServer server(&m_event_loop, address, "KrpcProvider");

    // 绑定连接回调和消息回调，分离了网络连接业务和消息处理业务
    server.setConnectionCallback(std::bind(&RpcProvider::onConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server.setThreadNum(4);

    // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client客户端可以在zk上发现服务
    ZkClient zkclient;
    zkclient.Start();
    // service_name为永久节点，method_name为临时节点
    for (auto &sp : m_service_map) {
        // service_name 在zk中的目录下是"/meha/service_name"
        std::string service_path = "/meha/" + sp.first;
        zkclient.CreateNode(service_path, "", ZkClient::CreateMode::Persistent);
        for (auto &mp : sp.second.method_map) {
            std::string method_path = service_path + "/" + mp.first;
            std::string method_path_data = ip + ":" + port;
            zkclient.CreateNode(method_path, method_path_data, ZkClient::CreateMode::Ephemeral);
        }
    }
    // rpc服务端准备启动，打印信息
    LOG(INFO) << "RpcProvider start service at ip:" << ip << " port:" << port;
    // 启动网络服务
    server.start();
    m_event_loop.loop();
    LOG(INFO) << "RpcProvider stop service at ip:" << ip << " port:" << port;
}

void RpcProvider::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected()) { // 如果连接关闭则断开连接即可。
        conn->shutdown();
    }
}

// 在框架内部,RpcProvider和Rpcconsumer之间协商好通信使用的的protobuf的数据类型。
// 已建立连接用户的读写事件回调 如果远程有一共rpc服务的调用请求，那么OnMessage方法就会响应。
// 一般的主要格式是header_size(4个字节)+header_str+arg_str 一般来说header_str是服务对象和服务对象中的方法，arg服务器对象方法设置的参数。

void RpcProvider::onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp receive_time)
{
    UNUSED(receive_time);
    LOG(INFO) << "call onMessage";
    // 网络上接收远程rpc调用请求的字符流
    std::string recv_buf = buffer->retrieveAllAsString();

    // 使用porotbuf的CodeInputStream反序列化rpc请求
    google::protobuf::io::ArrayInputStream raw_input(recv_buf.data(), recv_buf.size());
    google::protobuf::io::CodedInputStream coded_input(&raw_input);

    // 1. 获取RpcHeader字符串
    uint32_t header_size{};
    coded_input.ReadVarint32(&header_size); // 解析header_size（固定4B即32bits）
    // 根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str;
    tinyrpc::RpcHeader krpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size{};
    // 设置读取限制
    google::protobuf::io::CodedInputStream::Limit msg_limit = coded_input.PushLimit(header_size);
    coded_input.ReadString(&rpc_header_str, header_size);
    // 恢复之前的限制，以便安全地继续读取其他数据
    coded_input.PopLimit(msg_limit);
    if (krpcHeader.ParseFromString(rpc_header_str)) {
        // 2. 反序列化出RpcHeader结构体各成员
        service_name = krpcHeader.service_name();
        method_name = krpcHeader.method_name();
        args_size = krpcHeader.args_size();
    } else {
        LOG(ERROR) << "krpcHeader parse error";
        return;
    }
    std::string args_str; // rpc参数
    // 直接读取args_size长度的字符串数据
    if (!coded_input.ReadString(&args_str, args_size)) {
        LOG(ERROR) << "read args error";
        return;
    }
    // 打印调试信息
    // LOG(INFO) << "============================================";
    // LOG(INFO) << "header_size: " << header_size;
    // LOG(INFO) << "rpc_header_str: " << rpc_header_str;
    // LOG(INFO) << "service_name: " << service_name;
    // LOG(INFO) << "method_name: " << method_name;
    // LOG(INFO) << "args_str: " << args_str;
    // LOG(INFO) << "============================================";

    // 获取service对象和method对象
    auto it = m_service_map.find(service_name);
    if (it == m_service_map.end()) {
        LOG(WARNING) << service_name << "is not exist!";
        return;
    }
    auto mit = it->second.method_map.find(method_name);
    if (mit == it->second.method_map.end()) {
        LOG(WARNING) << service_name << "." << method_name << "is not exist!";
        return;
    }

    // 此时说明服务和方法都在，可以执行了

    google::protobuf::Service *service = it->second.service; // 获取服务对象
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取方法对象

    // 生成rpc方法调用请求的request和响应的response参数。Login函数需要这两个参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New(); // 通过 GetRequestPrototype，可以根据方法描述符动态获取对应的请求消息类型，并New()实例化该类型的对象【这样我就不用手动多态创建了】
    if (!request->ParseFromString(args_str)) {
        LOG(ERROR) << service_name << "." << method_name << "parse error!";
        return;
    }

    google::protobuf::Message *response = service->GetResponsePrototype(method).New(); // 同理获取请求消息对象

    // 给下面的mehod方法的调用绑定一个Closure的回调函数
    /***
    template <typename Class, typename Arg1, typename Arg2>
    inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2), Arg1 arg1, Arg2 arg2) { // void (Class::*)(Arg1, Arg2) 成员函数指针类型
        return new internal::MethodClosure2<Class, Arg1, Arg2> (object, method, true, arg1, arg2);
    }
    ***/
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr &, google::protobuf::Message *>(this, &RpcProvider::sendRpcResponse, conn, response);

    // 使用protobuf框架，调用当前rpc节点上发布的方法，并注册sendRpcResponse回调
    service->CallMethod(method, nullptr, request, response, done); // request,response是method方法(如login)的参数。done是执行完method方法后会执行的回调函数。
}

void RpcProvider::sendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str)) {
        // 序列化成功，通过网络把rpc方法执行的结果返回给rpc的调用方（执行结果在response里，序列化到response_str）
        conn->send(response_str);
    } else {
        LOG(ERROR) << "serialize error!";
    }
    // conn->shutdown(); // TODO 模拟http短链接，由rpcprovider主动断开连接
}

RpcProvider::~RpcProvider()
{
    m_event_loop.quit();
}
