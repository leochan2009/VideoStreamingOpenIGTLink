cmake_minimum_required(VERSION 2.8)
project( VideoStreamServer )

get_filename_component(PARENT_DIR ${CMAKE${PARENT_DIR}_CURRENT_BINARY_DIR} PATH)
set(CMAKE_PREFIX_PATH	"${PARENT_DIR}/OpenIGTLink-build")	
find_package(OpenIGTLink REQUIRED)
include(${OpenIGTLink_USE_FILE})
include_directories(${OpenIGTLink_INCLUDE_DIRS})
link_directories(${OpenIGTLink_LIBRARY_DIRS})
LINK_DIRECTORIES("${PARENT_DIR}/OpenH264")
include_directories("${PARENT_DIR}/OpenH264/codec")
include_directories("${PARENT_DIR}/OpenH264/codec/common/inc")
include_directories("${PARENT_DIR}/OpenH264/codec/console/common/inc")
include_directories("${PARENT_DIR}/OpenH264/codec/console/enc/src")
include_directories("${PARENT_DIR}/OpenH264/codec/api/svc")
include_directories("${PARENT_DIR}/OpenH264/codec/encoder/core/inc")
include_directories("${PARENT_DIR}/OpenH264/codec/processing/interface")
include_directories("${PARENT_DIR}/OpenH264/test")
include_directories("${CMAKE_SOURCE_DIR}/VideoStreamOpenIGTLink")


add_executable( VideoStreamServer VideoStreamServer.cxx)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  FIND_LIBRARY( OpenH264Lib OpenH264.lib
    ${PARENT_DIR}/OpenH264
  )
else(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  FIND_LIBRARY( OpenH264Lib libOpenH264.a
    ${PARENT_DIR}/OpenH264
  )
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

target_link_libraries( VideoStreamServer OpenIGTLink ${OpenH264Lib} VideoStreamOpenIGTLink)
