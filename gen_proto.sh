#!/bin/bash

generate_proto() {
    local proto_dir=$1
    local output_dir=$2

    if [ ! -d "$proto_dir" ]; then
        echo "Error: proto directory $proto_dir does not exist."
        return 1
    fi

    if [ ! -d "$output_dir" ]; then
        mkdir -p $output_dir
    fi

    for proto_file in $proto_dir/*.proto; do
        protoc -I $proto_dir --cpp_out=$output_dir $(basename $proto_file)
    done
}

clean_generated_files() {
    local output_dir=$1

    if [ -d "$output_dir" ]; then
        rm -rf $output_dir
    fi
}

# 添加命令行参数解析
if [ "$1" == "clean" ]; then
    clean_generated_files src/gen
else
    generate_proto src/proto src/gen
fi
