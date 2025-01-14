file(GLOB Client_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

#获取protobuf生成的.cc
file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../gen/*.pb.cc)

add_executable(client ${Client_SRCS} ${PROTO_SRCS})

target_link_libraries(client PRIVATE tinyrpc_core)

set_target_properties(client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/client)