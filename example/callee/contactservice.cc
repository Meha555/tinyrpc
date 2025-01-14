#include "contact.pb.h"
#include "rpcchannel.h"
#include "rpcconfig.h"
#include "rpccontroller.h"
#include "rpcprovider.h"
#include "user.pb.h"
#include <glog/logging.h>
#include <string>
#include <vector>

static std::map<uint32_t, std::vector<std::string>> g_contact_list = {
    {1, {"zhangsan", "lisi", "wangwu"}},
    {2, {"zhaosi", "zhangliu", "zhangqi"}},
    {3, {"zhangwu", "zhangliu", "zhangqi"}},
};

class ContactService : public example::ContactServiceRpc
{
public:
    void GetContactList(::google::protobuf::RpcController *controller,
                        const ::example::GetContactListRequest *request,
                        ::example::GetContactListResponse *response,
                        ::google::protobuf::Closure *done) override
    {
        uint32_t uid = request->uid();
        auto contact_list = GetContactList(uid);
        if (contact_list.empty()) {
            response->mutable_result()->set_errcode(1);
            response->mutable_result()->set_errmsg("No contact found");
        } else {
            response->mutable_result()->set_errcode(0);
            response->mutable_result()->set_errmsg(std::format("{} contacts found", contact_list.size()));
        }
        for (std::string &name : contact_list) {
            *(response->add_contacts()) = name;
        }
        done->Run();
    }

protected:
    std::vector<std::string> GetContactList(uint32_t uid)
    {
        LOG(WARNING) << "doing local service: GetFriendsList";
        // 调用别的微服务
        example::UserServiceRpc_Stub stub(new meha::RpcChannel);
        example::HasUserRequest req;
        req.set_uid(uid);
        example::HasUserResponse rsp;
        meha::RpcController controller;
        stub.HasUser(&controller, &req, &rsp, nullptr);
        if (controller.Failed()) {
            LOG(ERROR) << controller.ErrorText();
            return {};
        } else {
            return rsp.hasuser() ? g_contact_list[uid] : std::vector<std::string>();
        }
    }
};

int main(int argc, char **argv)
{
    using namespace meha;
    RpcConfig::InitEnv(argc, argv);

    RpcProvider provider("meha");
    provider.RegisterService(new ContactService());

    provider.Run();
    return 0;
}