#pragma once

// 此类是继承自google::protobuf::RpcChannel
// 目的是为了给客户端进行方法调用的时候，统一接收的

#include "zookeeperutil.h"
#include <google/protobuf/service.h>

namespace meha
{

class RpcChannel : public google::protobuf::RpcChannel
{
public:
    explicit RpcChannel();
    virtual ~RpcChannel();

    /**
     * @brief 客户端通过Stub对象调用RPC方法的回调。
     * 所有通过stub代理对象调用的rpc方法，都在调用这里，做RPC方法调用的参数序列化和网络传输
     * header_size + header_str(service_name method_name args_size) + args_str
     * @param method：指向 MethodDescriptor 对象的指针，其中包含 RPC 方法的名称和其他信息。
     * @param controller: 指向 RpcController 对象的指针，用于控制 RPC 的行为和获取RPC方法执行的状态。
     * @param request: 指向 HelloRequest 对象的指针，其中包含 RPC 方法的请求参数。
     * @param response: 指向 HelloReply 对象的指针，应在其中设置 RPC 方法的响应结果。
     * @param done: 指向 Closure 对象的指针，表示 RPC 方法完成后应执行的操作。
     */
    void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override;

private:
    /**
     * @brief 查询注册中心中服务的注册表项信息
     * @param zkclient
     * @param service_name 服务名
     * @param method_name 方法名
     * @return std::optional<std::pair<std::string, uint16_t>> IP和端口号
     */
    std::optional<std::pair<std::string, uint16_t>> QueryServiceHost(ZkClient *zkclient, std::string service_name, std::string method_name);
    /**
     * @brief 连接到RpcProvider
     * @param ip
     * @param port
     * @return true
     * @return false
     */
    bool ConnectTo(const std::string &ip, uint16_t port);

    int m_clientfd = -1; // 存放客户端套接字
    std::string m_service_name;
    std::string m_method_name;
    std::string m_ip;
    uint16_t m_port;
};
}