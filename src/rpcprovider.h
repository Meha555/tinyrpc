#pragma once

#include "google/protobuf/service.h"
#include <google/protobuf/descriptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <string>
#include <unordered_map>

namespace meha
{

/**
 * @brief 发布服务的类
 */
class RpcProvider
{
public:
    explicit RpcProvider(const std::string &package);
    ~RpcProvider();

    // 发布一个RPC服务
    void RegisterService(std::unique_ptr<google::protobuf::Service> service);
    // 启动RPC服务节点，开始提供RPC服务
    void Run();

private:
    // 新的socket连接回调
    void onConnection(const muduo::net::TcpConnectionPtr &conn);
    /**
     * @brief 已建立连接的客户端调用的读写事件回调
     * 需要反序列化解析远端发送的rpc请求数据，然后调用服务方法，并将返回值序列化发送回客户端
     * @param conn
     * @param buffer
     * @param receive_time
     */
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp receive_time);
    // RPCClosure的回调操作，用于序列化rpc的响应和网络发送
    void sendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response);

    /// @brief 该服务对象需要提交到注册中心的注册表项
    /// @note 由于含有std::unique_ptr，所以该类不能拷贝
    struct ServiceInfo
    {
        // 服务对象
        std::unique_ptr<google::protobuf::Service> service;
        // 服务方法
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> method_map;
    };
    std::unordered_map<std::string, ServiceInfo> m_service_map; // 保存服务对象和服务方法
    muduo::net::EventLoop m_event_loop;
};

}