find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)

add_subdirectory(monitor)
add_subdirectory(user_ebpf)
add_subdirectory(proto)
