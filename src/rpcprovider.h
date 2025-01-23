#pragma once

#include "google/protobuf/service.h"
#include <google/protobuf/descriptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <string>
#include <unordered_map>
#include "rwlock.h"

namespace meha
{

/**
 * @brief 管理当前进程中发布服务和移除服务的类
 * @details 作为TCP服务器，监听客户端（即caller）发送的数据，并调用服务方法来实现远程调用
 */
class RpcProvider
{
public:
    explicit RpcProvider(const std::string &package);
    ~RpcProvider();

    /**
     * @brief 发布一个RPC服务
     * 这个函数用于注册服务对象和其对应的 RPC 方法，以便服务端处理客户端的请求。
     * 参数类型设置为 google::protobuf::Service，是因为所有由 protobuf 生成的服务类
     * 都继承自 google::protobuf::Service，这样我们可以通过基类指针指向子类对象，实现动态多态。
     * @param service 
     */
    void RegisterService(std::unique_ptr<google::protobuf::Service> service);
    // 移除一个RPC服务
    void UnregisterService(const std::string &service_name);
    // 启动RPC服务节点，开始提供RPC服务
    void Run();

private:
    /**
     * @brief 新的socket连接回调
     */
    void onConnection(const muduo::net::TcpConnectionPtr &conn);
    /**
     * @brief 已建立连接的客户端调用的读写事件回调
     * 需要反序列化解析远端发送的rpc请求数据，然后调用服务方法，并将返回值序列化发送回客户端
     * @details 在框架内部，Callee和Caller之间协商好通信使用的的protobuf的数据类型。
     * 格式是header_size(4个字节)+header_str+arg_size+arg_str，是二进制字节流，
     * 其中header_str是服务对象和服务对象中的方法，arg_size是要传入服务对象方法的参数长度，
     * 然后就会接着arg_str，即Caller在Stub上设置的服务对象方法的传入参数。
     * @param conn
     * @param buffer
     * @param receive_time
     */
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp receive_time);
    /**
     * @brief RPCClosure的回调操作，用于序列化rpc的响应和网络发送
     */
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
    std::unordered_map<std::string, ServiceInfo> m_service_map; // 保存在该Provider上注册的所有服务对象和其服务方法
    muduo::net::EventLoop m_event_loop;
    RWLock m_rwlock;
};

}