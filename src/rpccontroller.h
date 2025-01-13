#pragma once

#include <google/protobuf/service.h>
#include <string>

namespace meha
{

/**
 * @brief 用于描述RPC调用的控制器
 * 主要作用是跟踪RPC方法调用的状态、错误信息并提供控制功能(如取消调用)
 */
class RpcController : public google::protobuf::RpcController
{
public:
    explicit RpcController();
    void Reset() override;
    bool Failed() const override;
    std::string ErrorText() const override;
    void SetFailed(const std::string &reason) override;

    // TODO 目前未实现具体的功能
    void StartCancel() override;
    bool IsCanceled() const override;
    void NotifyOnCancel(google::protobuf::Closure *callback) override;

private:
    bool m_failed; // RPC方法执行过程中的状态
    bool m_canceled;
    std::string m_errText; // RPC方法执行过程中的错误信息
};

}