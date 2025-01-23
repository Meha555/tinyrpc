#include "rpcconfig.h"
#include "memory"
#include <cstdlib>
#include <glog/logging.h>
#include <iostream>
#include <unistd.h>

using namespace meha;

RpcConfig &RpcConfig::Instance()
{
    static RpcConfig config;
    return config;
}

void RpcConfig::ParseCmd(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "format:" << argv[0] << "-i <configfile>" << std::endl;
        exit(EXIT_FAILURE);
    }
    int o;
    std::string config_file;
    while (-1 != (o = getopt(argc, argv, "i:"))) { // 如果argv里面有i的话就代表指定了配置文件
        switch (o) {
        case 'i':
            config_file = optarg; // 将i后的配置文件的路径赋给config_file
            break;
        case '?': // 这个问号代表我们不喜欢出现其他参数比如上面除i以外的
            std::cerr << "format:" << argv[0] << "-i <configfile>" << std::endl;
            exit(EXIT_FAILURE);
            break;
        case ':': // 如果出现了-i，但是没有参数。
            std::cerr << "format:" << argv[0] << "-i <configfile>" << std::endl;
            exit(EXIT_FAILURE);
            break;
        default:
            break;
        }
    }
    RpcConfig::Instance().LoadConfigFile(config_file.c_str());

    google::InitGoogleLogging(argv[0]);
    FLAGS_colorlogtostderr = true; // 启用彩色日志
    FLAGS_logtostderr = true; // 默认输出标准错误
}

struct FileDeleter
{
    void operator()(FILE *fp) const
    {
        if (fp) {
            std::fclose(fp);
        }
    }
};

void RpcConfig::LoadConfigFile(const char *config_file)
{
    // TODO 有空这里改造成用std::ofstream
    std::unique_ptr<FILE, FileDeleter> pf(fopen(config_file, "r"));
    if (pf == nullptr) {
        LOG(ERROR) << "open config file error!";
        exit(EXIT_FAILURE);
    }
    char buf[1024];
    while (fgets(buf, 1024, pf.get()) != nullptr) {
        std::string read_buf(buf);
        Trim(read_buf); // 去掉字符串前后的空格
        if (read_buf[0] == '#' || read_buf.empty())
            continue;
        int index = read_buf.find('=');
        if (index == -1)
            continue;
        std::string key = read_buf.substr(0, index);
        Trim(key); // 去掉key前后的空格
        int endindex = read_buf.find('\n', index); // 找到行尾
        std::string value = read_buf.substr(index + 1, endindex - index - 1); // 找到value，-1目的是排除\n
        Trim(value); // 去掉value前后的空格
        m_config_map.insert({key, value});
    }
}

std::optional<std::string> RpcConfig::Lookup(const std::string &key)
{
    auto it = m_config_map.find(key);
    if (it == m_config_map.end()) {
        return std::nullopt;
    }
    return it->second;
}

void RpcConfig::Trim(std::string &read_buf)
{
    int index = read_buf.find_first_not_of(' '); // 去掉字符串前的空格
    if (index != -1) {
        read_buf = read_buf.substr(index, read_buf.size() - index);
    }
    index = read_buf.find_last_not_of(' '); // 去掉字符串后的空格
    if (index != -1) {
        read_buf = read_buf.substr(0, index + 1);
    }
}