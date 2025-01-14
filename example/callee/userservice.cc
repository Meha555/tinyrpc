#include "user.pb.h"
#include "rpcconfig.h"
#include "rpcprovider.h"
#include <glog/logging.h>
#include <google/protobuf/service.h>
#include <string>

/**
 * @brief RPC服务UserService提供者
 */
class UserService : public example::UserServiceRpc
{
public: // RPC远端方法回调
    void Login(::google::protobuf::RpcController *controller
    , const ::example::LoginRequest *request
    , ::example::LoginResponse *response
    , ::google::protobuf::Closure *done) override
    {
        // 框架给业务上报了请求参数LoginRequest，应用获取相应数据做本地业务
        std::string name = request->name();
        std::string passwd = request->passwd();

        // 做本地业务
        bool ok = Login(name, passwd);

        // 写回响应（错误码，错误消息，返回值）
        example::ResultCode *code = response->mutable_result();
        if (ok) {
            code->set_errcode(0);
            code->set_errmsg("Login Success");
            response->set_uid(m_name2uid_map[name]);
        } else {
            code->set_errcode(1);
            code->set_errmsg("Login Failed");
            response->set_uid(-1);
        }

        // 执行回调操作（response对象数据的序列化和网络发送（都是由框架完成））
        done->Run();
    }
    void Logout(::google::protobuf::RpcController *controller
    , const ::example::LogoutRequest *request
    , ::example::LogoutResponse* response
    , ::google::protobuf::Closure* done) override
    {
        uint32_t uid = request->uid();
        bool ok = Logout(uid);
        example::ResultCode *code = response->mutable_result();
        if (ok) {
            code->set_errcode(0);
            code->set_errmsg("Login Success");
            response->set_uid(uid);
        } else {
            code->set_errcode(1);
            code->set_errmsg("Login Failed");
            response->set_uid(-1);
        }
        done->Run();
    }
    void IsUserOnline(::google::protobuf::RpcController *controller
    , const ::example::IsUserOnlineRequest *request
    , ::example::IsUserOnlineResponse *response
    , ::google::protobuf::Closure *done) override
    {
        uint32_t uid = request->uid();
        bool ok = IsUserOnline(uid);
        response->set_isonline(ok);
        done->Run();
    }
    void HasUser(::google::protobuf::RpcController *controller
    , const ::example::HasUserRequest *request
    , ::example::HasUserResponse *response
    , ::google::protobuf::Closure *done) override
    {
        uint32_t uid = request->uid();
        bool ok = HasUser(uid);
        response->set_hasuser(ok);
        done->Run();
    }

protected: // RPC方法对应的本地业务方法
    bool Login(std::string name, std::string passwd)
    {
        LOG(INFO) << "doing local service: Login";
        LOG(INFO) << "name:" << name << " passwd:" << passwd;
        static uint32_t uid = 1;
        if (m_user_map.contains(name)) {
            if (m_user_map[name] == passwd) {
                m_online_lists.emplace_back(m_name2uid_map[name]);
                return true;
            }
            return false;
        }
        else {
            m_online_lists.emplace_back(uid);
            m_user_map[name] = passwd;
            m_name2uid_map[name] = uid;
            m_uid2name_map[uid] = name;
            uid++;
        }
        return false;
    }
    bool Logout(uint32_t uid)
    {
        LOG(INFO) << "doing local service: Logout";
        if (m_uid2name_map.contains(uid)) {
            auto it = std::find(m_online_lists.begin(), m_online_lists.end(), uid);
            if (m_online_lists.end() != it) {
                m_online_lists.erase(it);
                return true;
            }
        }
        return false;
    }
    bool IsUserOnline(uint32_t uid) const
    {
        return std::find(m_online_lists.begin(), m_online_lists.end(), uid) != m_online_lists.end();
    }
    bool HasUser(uint32_t uid) const
    {
        return m_uid2name_map.contains(uid);
    }

private:
    std::vector<uint32_t> m_online_lists;
    std::map<uint32_t, std::string> m_uid2name_map;
    std::map<std::string, uint32_t> m_name2uid_map;
    std::map<std::string, std::string> m_user_map;
};

int main(int argc, char **argv)
{
    using namespace meha;
    // 调用框架的初始化操作
    RpcConfig::InitEnv(argc, argv);

    // provider是一个rpc网络服务对象。把UserService对象发布到rpc节点上
    RpcProvider provider("meha");
    provider.RegisterService(new UserService());

    // 启动一个rpc服务发布节点 Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();
    return 0;
}