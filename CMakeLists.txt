cmake_minimum_required(VERSION 3.24)
project(mymuduo)

#mymuduo最终编译成so动态库,设置动态库的路径,放在根目录的lib文件夹下
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
#设置调试信息 启动c++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC")

#定义参于编译的源代码文件
aux_source_directory(. SRC_LIST)
#b编译动态库
add_library(mymuduo SHARED ${SRC_LIST})