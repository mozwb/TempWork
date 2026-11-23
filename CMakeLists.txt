cmake_minimum_required(VERSION 3.20)
project(WEB LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(MODULES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Modules")



add_executable(ChatWeb
web/main.cpp
server/room.cpp
server/router.cpp
)

target_include_directories(ChatWeb PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/server"
    "${CMAKE_CURRENT_SOURCE_DIR}/log"
)

# 平台宏只在 Windows 下定义，Linux 上不要（否则会去 include <windows.h>）
if(WIN32)
    target_compile_definitions(ChatWeb PRIVATE WINDOWS)
endif()

add_subdirectory("${MODULES_ROOT}" "${CMAKE_BINARY_DIR}/ModulesBuild")
target_link_libraries(ChatWeb PRIVATE Network IO Timer)
target_include_directories(ChatWeb PRIVATE
    "${MODULES_ROOT}/include"
    "${MODULES_ROOT}/src"
)
# 编译完成后，自动把Memory.dll复制到ChatWeb.exe同目录
add_custom_command(TARGET ChatWeb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Memory>
        $<TARGET_FILE_DIR:ChatWeb>
)