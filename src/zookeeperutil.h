#pragma once

#include <optional>
#include <string>
#include <zookeeper/zookeeper.h>

namespace meha
{

/**
 * @brief ZooKeeper客户端封装类
    zookeeper_mt：多线程版本
    zookeeper的API客户端程序提供了三个线程
    API调用线程
    网络I/O线程poll
    watcher回调线程
 */
class ZkClient
{
public:
    enum CreateMode {
        Persistent = 0, // ZOO_PERSISTENT,
        Ephemeral = 1, // ZOO_EPHEMERAL,
        EphemeralSequential = 2, // ZOO_PERSISTENT_SEQUENTIAL
        PersistentSequential = 3, // ZOO_EPHEMERAL_SEQUENTIAL,
        Container = 4, // ZOO_CONTAINER
        PersistentWithTTL = 5, // ZOO_PERSISTENT_WITH_TTL
        PersistentSequentialWithTTL = 6, // ZOO_PERSISTENT_SEQUENTIAL_WITH_TTL
    };

    explicit ZkClient();
    ~ZkClient();
    // zkclient启动连接zkserver
    void Start(uint64_t recv_timeout_ms = 10000);
    // 在zkserver中根据指定的path创建一个节点
    void CreateNode(const char *path, const char *data, int datalen, CreateMode mode = Persistent);
    // 根据参数指定的znode节点路径获取znode节点值
    std::optional<std::string> GetNodeData(const std::string &path);

private:
    static void WatchCallback(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

    // Zk的客户端句柄
    zhandle_t *m_zhandle;
};
}