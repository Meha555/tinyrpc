#include "rpcprovider.h"
#include "common.h"
#include "rpcconfig.h"
#include "tinyrpcheader.pb.h"
#include "zookeeperutil.h"
#include <glog/logging.h>

using namespace meha;

RpcProvider::RpcProvider(const std::string &package)
{
    ZkClient zkclient;
    zkclient.Start();
    std::string toplevel = "/" + package;
    zkclient.CreateNode(toplevel.c_str(), "", ZkClient::CreateMode::Persistent);
    LOG(INFO) << "zk create " << toplevel << " as toplevel node";
}

void RpcProvider::RegisterService(std::unique_ptr<google::protobuf::Service> service)
{
    // 服务端需要知道对方想要调用的具体服务对象和方法，从而让Protobuf RPC框架能调用对应的本地方法。
    // 所以服务端需要维护一张表来存储方法名和服务对象之间的关系，这里使用一个数据结构（ServiceInfo）来保存。
    ServiceInfo service_info;

    // 通过 ServiceDescriptor，我们可以获取该服务类中定义的方法列表，并进行相应的注册和管理。
    const google::protobuf::ServiceDescriptor *psd = service->GetDescriptor();
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
    service_info.service = std::move(service);
    m_rwlock.WriteLock();
    m_service_map.emplace(service_name, std::move(service_info));
    m_rwlock.Unlock();
}

void RpcProvider::UnregisterService(const std::string &service_name)
{
    // RpcProvider维护的是本进程的服务注册表，所以直接删掉本进程所注册的服务表项就可以
    m_rwlock.WriteLock();
    m_service_map.erase(service_name);
    LOG(WARNING) << "service_name=" << service_name << " unregistered";
    m_rwlock.Unlock();
    // 不用到zookeeper上删除节点，因为创建的是临时节点，本进程服务下线时会断开连接，从而自动删除
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

    // 设置muduo库的线程数量（这里是1个IO线程处理链接，3个工作线程处理业务）
    server.setThreadNum(4);

    // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client可以在zk上发现服务
    // NOTE 这里没有区分注册多个相同服务的情况（比如同一个服务部署在多台机器上，这种情况可能是要修改zk的节点名）
    ZkClient zkclient;
    zkclient.Start();
    m_rwlock.ReadLock();
    // service_name为永久节点(因为可能很多个该服务的实例），method_name为临时节点(因为每个method节点对应一个该服务的实例)
    for (auto &sp : m_service_map) {
        // service_name 在zk中的目录下是"/meha/service_name"
        std::string service_path = "/meha/" + sp.first;
        zkclient.CreateNode(service_path, "", ZkClient::CreateMode::Persistent);
        for (auto &mp : sp.second.method_map) {
            std::string method_path = service_path + "/" + mp.first;
            std::string method_path_data = ip + ":" + port;
            zkclient.CreateNode(method_path, method_path_data, ZkClient::CreateMode::Ephemeral
                                , std::bind(&RpcProvider::UnregisterService, this, std::placeholders::_1));
        }
    }
    m_rwlock.Unlock();
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

void RpcProvider::onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp receive_time)
{
    /* 1. 网络上接收的远程rpc调用请求的字符流    Login args  */
    /* 2. 对收到的字符流反序列化  */
    /* 3. 从反序列化的字符流中解析出service_name 和 method_name 和 args_str参数 */
	/* 4. m_serviceMap是一个哈希表，我们之前将服务对象和方法对象保存在这个表里面，
          根据service_name和method_name可以从m_serviceMap中找到服务对象service和方法对象描述符method。
       5. 将args_str解码填充至request对象中
    	  之后request对象就可以这样使用了：request.name() = "zhangsan"  |  request.pwd() = "123456"
    */
    UNUSED(receive_time);
    LOG(INFO) << "call onMessage";
    // 网络上接收远程rpc调用请求的字节流
    std::string recv_buf = buffer->retrieveAllAsString();

    // 使用porotbuf的CodeInputStream反序列化rpc请求
    google::protobuf::io::ArrayInputStream raw_input(recv_buf.data(), recv_buf.size());
    google::protobuf::io::CodedInputStream coded_input(&raw_input);

    // 获取RpcHeader字符串
    uint32_t header_size{};
    coded_input.ReadVarint32(&header_size); // 解析header_size（固定4B即32bits）
    // 根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str;
    tinyrpc::RpcHeader header;
    std::string service_name;
    std::string method_name;
    uint32_t args_size{};
    // 设置读取限制，读出RpcHeader
    google::protobuf::io::CodedInputStream::Limit msg_limit = coded_input.PushLimit(header_size);
    coded_input.ReadString(&rpc_header_str, header_size);
    // 恢复之前的限制，以便安全地继续读取其他数据
    coded_input.PopLimit(msg_limit);
    // 将读到的RpcHeader数据填充至对象中方便使用
    if (header.ParseFromString(rpc_header_str)) {
        // 2. 反序列化出RpcHeader结构体各成员
        service_name = header.service_name();
        method_name = header.method_name();
        args_size = header.args_size();
    } else {
        LOG(ERROR) << "header parse error";
        return;
    }
    std::string args_str; // rpc参数
    // 直接读取args_size长度的字节payload
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
    m_rwlock.ReadLock();
    auto sit = m_service_map.find(service_name);
    if (sit == m_service_map.end()) {
        LOG(WARNING) << service_name << " is not exist!";
        m_rwlock.Unlock();
        return;
    }
    m_rwlock.Unlock();

    auto mit = sit->second.method_map.find(method_name);
    if (mit == sit->second.method_map.end()) {
        LOG(WARNING) << service_name << "." << method_name << " is not exist!";
        return;
    }

    // 此时说明服务和方法都在，可以执行了
    // 我们要通过Protobuf RPC框架来调用本地的服务方法实现，所以要先准备一些需要的对象

    google::protobuf::Service * const service = sit->second.service.get(); // 获取服务对象
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取方法对象

    // 生成rpc方法调用请求的request和响应的response参数。本地的RPC回调需要这两个参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New(); // 通过 GetRequestPrototype，可以根据方法描述符动态获取对应的请求消息类型，并New()实例化该类型的对象【这样我就不用手动多态创建了】
    if (!request->ParseFromString(args_str)) {
        LOG(ERROR) << service_name << "." << method_name << "parse error!";
        return;
    }

    google::protobuf::Message *response = service->GetResponsePrototype(method).New(); // 同理获取请求消息对象

    // 给下面的mehod方法的调用绑定一个回调函数，当服务的方法调用完成后，这个回调函数会被调用
    /***
    template <typename Class, typename Arg1, typename Arg2>
    inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2), Arg1 arg1, Arg2 arg2) { // void (Class::*)(Arg1, Arg2) 成员函数指针类型
        return new internal::MethodClosure2<Class, Arg1, Arg2> (object, method, true, arg1, arg2);
    }
    ***/
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr &, google::protobuf::Message *>(this, &RpcProvider::sendRpcResponse, conn, response);

    // 使用protobuf框架，调用当前rpc节点上发布的服务方法
    service->CallMethod(method, nullptr, request, response, done); // request,response是method方法(如login)的参数。done是执行完method方法后会执行的回调函数。
}

void RpcProvider::sendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response)
{
    LOG(INFO) << "RPC Call finished, sending response to caller";
    std::string response_str;
    if (response->SerializeToString(&response_str)) {
        // 序列化成功，通过网络把rpc方法执行的结果返回给rpc的调用方（执行结果在response里，序列化到response_str）
        conn->send(response_str);
    } else {
        LOG(ERROR) << "serialize response error!";
    }
    // 模拟http短链接（毕竟是方法调用），由rpcprovider主动断开连接【目前无法使用，因为我在RpcChannel中有一个m_clientFd改不了】
    // TODO 要想改得能用，应该是要把RpcChannel中使用裸的socket API改成使用muduo::TcpConnectionPtr
    // conn->shutdown();
}

RpcProvider::~RpcProvider()
{
    m_event_loop.quit();
}
