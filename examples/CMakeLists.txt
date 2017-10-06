add_definitions(--std=c++11)
find_package(boost REQUIRED COMPONENTS interprocess)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(../include)
include_directories(../include)
add_executable(ipcpooledchanneltestC ipcpooledchanneltestC.cpp)

add_executable(ipcpooledchanneltestS ipcpooledchanneltestS.cpp)


add_executable(pooledchanneltest pooledchanneltest.cpp)

add_executable(termination termination.cpp)

#add_executable(pubsub pubsub.cpp)
#add_executable(dispatcher dispatcher.cpp)