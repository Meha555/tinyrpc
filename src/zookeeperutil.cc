#include "zookeeperutil.h"
#include "rpcconfig.h"
#include <glog/logging.h>
#include <semaphore>

using namespace meha;

// 全局的watcher观察器，zkserver给zkclient的通知——有服务上线
void ZkClient::WatchCallback(zhandle_t *zh, int type, int status, const char *path, void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT) { // 回调消息类型和会话相关的消息类型
        if (status == ZOO_CONNECTED_STATE) { // zkclient和zkserver连接成功
            auto *psem = (std::binary_semaphore *)zoo_get_context(zh);
            psem->release();
        }
    }
}

ZkClient::ZkClient()
    : m_zhandle(nullptr)
{
}

ZkClient::~ZkClient()
{
    if (m_zhandle != nullptr) {
        zookeeper_close(m_zhandle);
    }
}

void ZkClient::Start(uint64_t recv_timeout_ms)
{
    std::string ip = RpcConfig::Instance().Lookup("zookeeper_ip").value_or("127.0.0.1"); // 获取zookeeper服务端的ip
    std::string port = RpcConfig::Instance().Lookup("zookeeper_port").value_or("2181"); // 获取zoo keeper服务端的port

    std::string host_str = ip + ":" + port;

    auto *psem = new std::binary_semaphore(0);

    // 使用zookeeper_init初始化一个zk对象，异步建立rpcserver和zkclient之间的连接
    m_zhandle = zookeeper_init(host_str.c_str(), WatchCallback, recv_timeout_ms, nullptr, psem, 0);
    if (nullptr == m_zhandle) { // 这个返回值不代表连接成功或者不成功
        LOG(ERROR) << "zookeeper_init error: " << google::StrError(errno);
        exit(EXIT_FAILURE);
    }

    psem->acquire();
    LOG(INFO) << "zookeeper_init success";
}

void ZkClient::CreateNode(const char *path, const char *data, int datalen, CreateMode mode)
{
    // 创建znode节点，可以选择永久性节点还是临时节点。
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if (flag == ZNONODE) { // 表示节点不存在
        // 创建指定的path的znode节点
        flag = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, mode, path_buffer, bufferlen);
        if (flag == ZOK) {
            LOG(INFO) << "znode create success... path:" << path;
            return;
        } else {
            LOG(ERROR) << "znode create success... path:" << path;
            exit(EXIT_FAILURE);
        }
    }
    LOG(INFO) << "znode already exists... path:" << path;
}

std::optional<std::string> ZkClient::GetNodeData(const std::string &path)
{
    char buf[64];
    int bufferlen = sizeof(buf);
    int flag = zoo_get(m_zhandle, path.c_str(), 0, buf, &bufferlen, nullptr);
    if (flag != ZOK) {
        LOG(ERROR) << "zoo_get error";
        return std::nullopt;
    } else {
        return buf;
    }
}
