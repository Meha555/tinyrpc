#include "contact.pb.h"
#include "rpcchannel.h"
#include "rpcconfig.h"
#include "rpccontroller.h"
#include "user.pb.h"
#include <glog/logging.h>

using namespace meha;

int main(int argc, char **argv)
{
    RpcConfig::InitEnv(argc, argv);
    RpcController controller;

    example::UserServiceRpc_Stub user_stub(new RpcChannel);

    example::LoginRequest req;
    req.set_name("zhangsan");
    req.set_passwd("123456");
    example::LoginResponse rsp;
    user_stub.Login(&controller, &req, &rsp, nullptr);
    if (controller.Failed())
    {
        LOG(ERROR) << controller.ErrorText();
        exit(EXIT_FAILURE);
    }

    example::ContactServiceRpc_Stub contact_stub(new RpcChannel);

    example::GetContactListRequest req2;
    req2.set_uid(rsp.uid());
    example::GetContactListResponse rsp2;
    contact_stub.GetContactList(&controller, &req2, &rsp2, nullptr);
    if (controller.Failed())
    {
        LOG(ERROR) << controller.ErrorText();
        exit(EXIT_FAILURE);
    }

    if (rsp2.contacts_size() == 0) {
        LOG(WARNING) << "no contacts: " << rsp2.result().errmsg();
        exit(EXIT_SUCCESS);
    }

    LOG(INFO) << "user " << "zhangsan" << "'s contacts:";
    for (auto& contact : rsp2.contacts()) {
        LOG(INFO) << "contact :" << contact;
    }

    return 0;
}