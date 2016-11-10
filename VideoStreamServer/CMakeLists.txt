cmake_minimum_required(VERSION 2.8)
project( VideoStreamServer )

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_BINARY_DIR} PATH)
set(CMAKE_PREFIX_PATH	"${PARENT_DIR}/VideoStreamOpenIGTLink")		
find_package(VideoStreamOpenIGTLink REQUIRED)
include(${VideoStreamOpenIGTLink_USE_FILE})

add_executable( VideoStreamServer VideoStreamServer.cxx)
target_link_libraries( VideoStreamServer ${VideoStreamOpenIGTLink_LIBRARIES})
