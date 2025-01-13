import os
import subprocess
import threading
import argparse

# 使用 argparse 模块处理命令行参数
parser = argparse.ArgumentParser(description="Format source code using clang-format.")
parser.add_argument("--verbose", action="store_true", help="Print formatting information.")
args = parser.parse_args()

# 定义要格式化的文件夹列表
dirs = ["src", "example"]

# 指定 clang-format 路径
clang_format_path = "/usr/bin/clang-format"

# 定义文件类型列表
file_suffix_white_list = (".c", ".cpp", ".cc", ".h", ".hpp", ".tpp")
file_suffix_black_list = (".pb.h")

# 定义执行 clang-format 的函数
def run_clang_format(file_list: list[str], verbose: bool =True):
    for file_path in file_list:
        if verbose:
            print(f"正在格式化文件：{file_path}")
        subprocess.call([clang_format_path, "-i", file_path, "--style=file"])
        if verbose:
            print(f"文件格式化完成：{file_path}")

# 遍历文件夹并收集满足要求的文件
def traverse_and_collect(dir: str):
    file_list = []
    for root, dirs, files in os.walk(dir):
        for file in files:
            file_path = os.path.join(root, file)
            if file_path.endswith(file_suffix_white_list) and not file_path.endswith(file_suffix_black_list):
                file_list.append(file_path)
    if file_list:
        run_clang_format(file_list, args.verbose)

# 创建线程用于遍历根文件夹并格式化文件
threads = []
for dir in dirs:
    thread = threading.Thread(target=traverse_and_collect, args=(dir,))
    thread.start()
    threads.append(thread)

# 等待所有线程完成
for thread in threads:
    thread.join()