#include "rpccontroller.h"

using namespace meha;

RpcController::RpcController()
    : m_failed(false)
    , m_canceled(false)
    , m_errText("RPC error occurred: ")
    , m_callback(nullptr)
{
}

void RpcController::Reset()
{
    m_failed = false;
    m_canceled = false;
    m_errText.clear();
}

bool RpcController::Failed() const
{
    return m_failed;
}

std::string RpcController::ErrorText() const
{
    return m_errText;
}

void RpcController::SetFailed(const std::string &reason)
{
    m_failed = true;
    m_errText += reason;
}

void RpcController::StartCancel()
{
    m_canceled = true;
    if (m_callback) {
        m_callback->Run();
    }
    m_canceled = false;
}

bool RpcController::IsCanceled() const
{
    return m_canceled;
}

void RpcController::NotifyOnCancel(google::protobuf::Closure *callback)
{
    if(!m_canceled) {
        return;
    }
    m_callback = callback;
    callback->Run();
}