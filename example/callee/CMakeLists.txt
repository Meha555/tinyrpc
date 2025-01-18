set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/server)

add_executable(userservice userservice.cc ../gen/user.pb.cc ../gen/echo.pb.cc ../gen/resultcode.pb.cc)
target_link_libraries(userservice PRIVATE tinyrpc_core)

add_executable(contactservice contactservice.cc ../gen/contact.pb.cc ../gen/user.pb.cc ../gen/resultcode.pb.cc)
target_link_libraries(contactservice PRIVATE tinyrpc_core)