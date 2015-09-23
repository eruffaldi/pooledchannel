add_definitions(--std=c++11)
find_package(boost REQUIRED COMPONENTS interprocess)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(../include)
include_directories(../include)
add_executable(ipcpooledchanneltest ipcpooledchanneltest.cpp)

add_executable(pooledchanneltest pooledchanneltest.cpp)