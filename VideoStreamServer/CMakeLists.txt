cmake_minimum_required(VERSION 2.8)
project( VideoStreamServer )

set(CMAKE_PREFIX_PATH	"${CMAKE_BINARY_DIR}/OpenIGTLink-build")	
find_package(OpenIGTLink REQUIRED)
include(${OpenIGTLink_USE_FILE})
include_directories(${OpenIGTLink_INCLUDE_DIRS})
link_directories(${OpenIGTLink_LIBRARY_DIRS})
LINK_DIRECTORIES("${CMAKE_BINARY_DIR}/OpenH264")
include_directories("${CMAKE_BINARY_DIR}/OpenH264/codec")
include_directories("${CMAKE_BINARY_DIR}/OpenH264/codec/common/inc")
include_directories("${CMAKE_BINARY_DIR}/OpenH264/codec/console/common/inc")
include_directories("${CMAKE_BINARY_DIR}/OpenH264/codec/api/svc")
include_directories("${CMAKE_BINARY_DIR}/OpenH264/codec/encoder/core/inc")
include_directories("${CMAKE_BINARY_DIR}/OpenH264/codec/processing/interface")
include_directories("${CMAKE_BINARY_DIR}/OpenH264/test")
include_directories("${CMAKE_SOURCE_DIR}/VideoStreamOpenIGTLink")


add_executable( VideoStreamServer VideoStreamServer.cxx)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  FIND_LIBRARY( VideoStreamOpenIGTLinkLib VideoStreamOpenIGTLink.lib
    ${CMAKE_BINARY_DIR}/VideoStreamOpenIGTLink/Release
    ${CMAKE_BINARY_DIR}/VideoStreamOpenIGTLink/Debug
  )
  FIND_LIBRARY( OpenH264Lib OpenH264.lib
    ${CMAKE_BINARY_DIR}/OpenH264
  )
else(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  FIND_LIBRARY( VideoStreamOpenIGTLinkLib libVideoStreamOpenIGTLink.a
    ${CMAKE_BINARY_DIR}/VideoStreamOpenIGTLink/Release
    ${CMAKE_BINARY_DIR}/VideoStreamOpenIGTLink/Debug
  )
  FIND_LIBRARY( OpenH264Lib libOpenH264.a
    ${CMAKE_BINARY_DIR}/OpenH264
  )
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

target_link_libraries( VideoStreamServer OpenIGTLink ${OpenH264Lib} ${VideoStreamOpenIGTLinkLib})
