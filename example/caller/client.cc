#include "contact.pb.h"
#include "echo.pb.h"
#include "rpcchannel.h"
#include "rpcconfig.h"
#include "rpccontroller.h"
#include "user.pb.h"
#include <glog/logging.h>

using namespace meha;

RpcController controller;

void test_client_call_service()
{
    LOG(WARNING) << "========= " << __PRETTY_FUNCTION__ << " =========";

    example::EchoService_Stub echo_stub(new RpcChannel, ::google::protobuf::Service::ChannelOwnership::STUB_OWNS_CHANNEL);
    example::EchoRequest req;
    req.set_message("HelloWorld!");
    example::EchoResponse rsp;
    echo_stub.Echo(&controller, &req, &rsp, nullptr);
    if (controller.Failed()) {
        LOG(ERROR) << controller.ErrorText();
        exit(EXIT_FAILURE);
    }
    LOG(INFO) << "echo: " << rsp.message();
}

void test_service_call_another_service()
{
    LOG(WARNING) << "========= " << __PRETTY_FUNCTION__ << " =========";
    auto* channel = new RpcChannel();
    example::UserService_Stub user_stub(channel);

    example::LoginRequest req;
    req.set_name("zhangsan");
    req.set_passwd("123456");
    example::LoginResponse rsp;
    user_stub.Login(&controller, &req, &rsp, nullptr);
    if (controller.Failed()) {
        LOG(ERROR) << controller.ErrorText();
        exit(EXIT_FAILURE);
    }

    example::ContactService_Stub contact_stub(channel);

    example::GetContactListRequest req2;
    req2.set_uid(rsp.uid());
    example::GetContactListResponse rsp2;
    contact_stub.GetContactList(&controller, &req2, &rsp2, nullptr);
    if (controller.Failed()) {
        LOG(ERROR) << controller.ErrorText();
        exit(EXIT_FAILURE);
    }

    if (rsp2.contacts_size() == 0) {
        LOG(WARNING) << "no contacts: " << rsp2.result().errmsg();
        exit(EXIT_SUCCESS);
    }

    LOG(INFO) << "user zhangsan 's contacts:";
    for (auto &contact : rsp2.contacts()) {
        LOG(INFO) << "contact :" << contact;
    }
    delete channel;
}

int main(int argc, char **argv)
{
    RpcConfig::ParseCmd(argc, argv);
    test_client_call_service();
    test_service_call_another_service();
    return 0;
}