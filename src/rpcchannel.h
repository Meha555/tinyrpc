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
     * @param method
     * @param controller
     * @param request
     * @param response
     * @param done
     */
    void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override; // override可以验证是否是虚函数

private:
    /**
     * @brief 查询注册中心中服务的注册表项信息
     * @param zkclient
     * @param service_name
     * @param method_name
     * @return std::optional<std::pair<std::string, uint16_t>>
     */
    std::optional<std::pair<std::string, uint16_t>> QueryServiceHost(ZkClient *zkclient, std::string service_name, std::string method_name);
    /**
     * @brief 连接到RPC方法的服务提供者
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