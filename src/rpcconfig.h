#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace meha
{

class RpcConfig
{
public:
    static RpcConfig &Instance();
    // 解析命令行参数
    static void InitEnv(int argc, char **argv);
    // 加载配置文件
    void LoadConfigFile(const char *config_file);
    // 查找key对应的value
    std::optional<std::string> Lookup(const std::string &key);

private:
    // 去掉字符串前后的空格
    void Trim(std::string &read_buf);

    std::unordered_map<std::string, std::string> m_config_map;
};

}