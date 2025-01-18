# tinyrpc

## 项目概述

Linux 环境下基于 muduo、Protobuf 和 Zookeeper 实现的一个轻量级 RPC 框架。可以把单体架构系统的本地方法调用，重构成基于 TCP 网络通信的 RPC 远程方法调用，实现统一台机器不同进程或者不同机器之间的服务调用。

- 基于 muduo 网络库实现高并发网络通信模块，作为 RPC 同步调用的基础。
- 基于 Protobuf 实现 RPC 方法调用和参数的序列化和反序列化，并根据其提供得 RPC 接口编写 RPC 服务。
- 基于 ZooKeeper 分布式协调服务中间件提供服务注册和服务发现功能。
- 设计了基于 TCP 传输的二进制协议，解决粘包问题，且能够高效传输服务名、方法名以及参数。

## example

### 介绍

在example目录下，分别有callee和caller两个目录，分别对应服务发布端和调用端。

其中callee中，`EchoService` 和 `UserService` 是通过 `RpcProvider` 发布在同一个节点 `127.0.0.1:8000` 上的；而 `ContactService` 是发布在另一个节点 `127.0.0.1:8001` 上的。

两者共同说明了服务是可以分布式部署的，同一个 `RpcProvider` 发布的服务会共用同一个节点。且除了客户端和服务端之间可以调用，服务端之间也可以调用。

### 运行方法

启动zookeeper，可使用docker：`docker run --name zk1 -p 2181:2181 -it zookeeper bash`

进入到example目录下，运行server和client，即可完成服务发布和调用。

```shell
cd server
./userservice -i ../../example/callee/userservice.conf
./contactservice -i ../../example/callee/contactservice.conf
cd client
./client -i ../../example/caller/client.conf
```

![](example/example.png)

## 库准备

1. Muduo 库

Muduo 是一个基于多线程 Epoll 模式的高效网络库，负责数据流的网络通信。
安装教程参考：[Mudo安装](https://blog.csdn.net/QIANGWEIYUAN/article/details/89023980)

2. Zookeeper
Zookeeper 负责服务注册与发现，动态记录服务的 IP 地址及端口号，以便调用端快速找到目标服务。

安装 Zookeeper：
```shell
sudo apt install zookeeperd
```
安装 Zookeeper 开发库：
```shell
sudo apt install libzookeeper-mt-dev
```

3. Protobuf
Protobuf 负责 RPC 方法的注册、数据的序列化与反序列化。

```shell
sudo apt-get install protobuf-compiler libprotobuf-dev
```

4. Glog 日志库的安装

一个简单高效的异步日志库，用于记录框架运行时的调试与错误日志。

```shell
sudo apt-get install libgoogle-glog-dev libgflags-dev
```

## 整体的框架

- **muduo库**：负责数据流的网络通信，采用了多线程epoll模式的IO多路复用，让服务发布端接受服务调用端的连接请求，并由绑定的回调函数处理调用端的函数调用请求。

- **Protobuf**：负责RPC方法的注册，数据的序列化和反序列化，相比于文本存储的XML和JSON来说，Protobuf是二进制存储，且不需要存储额外的信息，效率更高。

- **Zookeeper**：负责分布式环境的服务注册，记录服务所在的IP地址以及端口号，可动态地为调用端提供目标服务所在发布端的IP地址与端口号，方便服务所在IP地址变动的及时更新。

- **TCP沾包问题处理**：定义服务发布端和调用端之间的消息传输格式，记录方法名和参数长度，防止沾包。

## TODO

- [ ] 性能测试
- [ ] 利用muduo库替换rpcchannel::callmethod中的send/recv
- [ ] 实现正确的rpccontroller
