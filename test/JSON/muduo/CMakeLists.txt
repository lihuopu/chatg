cmake_minimum_required(VERSION 3.10)
project(mm)

# 设置 C++ 标准为 C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 配置编译选项（调试 + 警告）
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -Werror")

# 配置头文件搜索路径
#include_directories(/usr/local/include)  # 替换为实际路径

# 配置库文件搜索路径
#link_directories(/usr/local/include)  # 替换为实际路径

#把.指定路径下的所有源文件名字放入变量名SRC_LIST里面
#aux_source_directory(. SRC_LIST)


#设置可执行文件最终存储路径 ${PROJECT_SOURCE_DIR}-工程根目录，现在的muduo
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 设置源文件
set(SRC_LIST testmuduo.cpp)

# 生成可执行文件,由SRC_LIST变量定义的源文件编译而来
add_executable(testmuduo ${SRC_LIST})

# 链接依赖库
target_link_libraries(testmuduo muduo_net muduo_base pthread)