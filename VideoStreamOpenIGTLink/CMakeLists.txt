cmake_minimum_required(VERSION 2.8)
project( VideoStreamOpenIGTLink )

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_BINARY_DIR} PATH)
get_filename_component(PARENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PATH)
set(CMAKE_PREFIX_PATH	"${PARENT_DIR}/OpenIGTLink-build")	
find_package(OpenIGTLink REQUIRED)
include(${OpenIGTLink_USE_FILE})
include_directories(${OpenIGTLink_INCLUDE_DIRS})
link_directories(${OpenIGTLink_LIBRARY_DIRS})
#-----------------------------------------------------------------------------
# Help other projects use VideoStreamOpenIGTLink.

# Copy the UseVideoStreamOpenIGTLink.cmake file to the binary tree for backward compatability.
CONFIGURE_FILE(${VideoStreamOpenIGTLink_SOURCE_DIR}/UseVideoStreamOpenIGTLink.cmake.in
${VideoStreamOpenIGTLink_BINARY_DIR}/UseVideoStreamOpenIGTLink.cmake COPYONLY IMMEDIATE)

# Create the VideoStreamOpenIGTLinkConfig.cmake file containing the VideoStreamOpenIGTLink configuration.
INCLUDE (${VideoStreamOpenIGTLink_SOURCE_DIR}/GenerateVideoStreamOpenIGTLinkConfig.cmake)

#Add OpenH264 directly to our build.
#add_subdirectory(${PARENT_DIR}/Testing/OpenH264)
SET(VideoStreamOpenIGTLink_SOURCES ${VideoStreamOpenIGTLink_SOURCE_DIR}/VideoStreamIGTLinkServer.cxx ${VideoStreamOpenIGTLink_SOURCE_DIR}/VideoStreamIGTLinkReceiver.cxx ${PARENT_SOURCE_DIR}/EvaluationTool.cpp ${PARENT_DIR}/OpenH264/test/api/sha1.c H264Decoder.cpp ${PARENT_DIR}/OpenH264/codec/console/common/src/read_config.cpp)
SET(VideoStreamOpenIGTLink_INCLUDES ${VideoStreamOpenIGTLink_SOURCE_DIR}/VideoStreamIGTLinkServer.h ${VideoStreamOpenIGTLink_SOURCE_DIR}/VideoStreamIGTLinkReceiver.h ${PARENT_SOURCE_DIR}/EvaluationTool.h ${PARENT_DIR}/OpenH264/test/sha1.h H264Decoder.h ${PARENT_DIR}/OpenH264/codec/console/common/inc/read_config.h)
message("${OpenIGTLink_INCLUDE_DIRS}")
SET(VideoStreamOpenIGTLink_INCLUDE_DIRS
	${VideoStreamOpenIGTLink_INCLUDE_DIRS}
  ${VideoStreamOpenIGTLink_SOURCE_DIR}
  ${OpenIGTLink_INCLUDE_DIRS}
  "${PARENT_DIR}/OpenH264/codec"
  "${PARENT_DIR}/OpenH264/codec/common/inc"
  "${PARENT_DIR}/OpenH264/codec/console/common/src"
	"${PARENT_DIR}/OpenH264/codec/console/common/inc"
	"${PARENT_DIR}/OpenH264/codec/console/dec/inc"
	"${PARENT_DIR}/OpenH264/codec/console/enc/src"
	"${PARENT_DIR}/OpenH264/codec/api/svc"
	"${PARENT_DIR}/OpenH264/codec/encoder/core/inc"
	"${PARENT_DIR}/OpenH264/codec/processing/interface"
	"${PARENT_DIR}/OpenH264/test"
)
INCLUDE_DIRECTORIES(${VideoStreamOpenIGTLink_INCLUDE_DIRS})

LINK_DIRECTORIES("${PARENT_DIR}/OpenH264")
SET(VideoStreamOpenIGTLink_LIBRARY_PATH ${VideoStreamOpenIGTLink_BINARY_DIR} CACHE INTERNAL "Single output directory for building all libraries.")
SET(VideoStreamOpenIGTLink_INCLUDE_PATH ${VideoStreamOpenIGTLink_INCLUDE_DIRS} CACHE INTERNAL "Include path for project that uses VideoStreamOpenIGTLink.")
add_library( VideoStreamOpenIGTLink ${VideoStreamOpenIGTLink_SOURCES} ${VideoStreamOpenIGTLink_INCLUDES})
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  SET(LINK_OPENIGTLINK_LIBRARY optimized ${PARENT_DIR}/OpenIGTLink-build/bin/Release/OpenIGTLink.lib debug ${PARENT_DIR}/OpenIGTLink-build/bin/Debug/OpenIGTLink.lib)
  SET(LINK_OPENIGTLINK_UTIL_LIBRARY optimized ${PARENT_DIR}/OpenIGTLink-build/bin/Release/igtlutil.lib debug ${PARENT_DIR}/OpenIGTLink-build/bin/Debug/igtlutil.lib)
  target_link_libraries( VideoStreamOpenIGTLink  LINK_OPENIGTLINK_LIBRARY LINK_OPENIGTLINK_UTIL_LIBRARY ${PARENT_DIR}/OpenH264/openH264.lib)
  SET(LINK_VIDEOSTREAMOPENIGTLINK_LIBRARY optimized ${VideoStreamOpenIGTLink_BINARY_DIR}/Release/VideoStreamOpenIGTLink.lib debug ${VideoStreamOpenIGTLink_BINARY_DIR}/Debug/VideoStreamOpenIGTLink.lib)
  SET(VideoStreamOpenIGTLink_LIBRARIES ${LINK_VIDEOSTREAMOPENIGTLINK_LIBRARY} ${LINK_OPENIGTLINK_LIBRARY} ${LINK_OPENIGTLINK_UTIL_LIBRARY} ${PARENT_DIR}/OpenH264/openh264.lib CACHE INTERNAL "a collection of all video streaming libraries.")
else(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  SET(LINK_OPENIGTLINK_LIBRARY optimized ${PARENT_DIR}/OpenIGTLink-build/bin/Release/libOpenIGTLink.a debug ${PARENT_DIR}/OpenIGTLink-build/bin/Debug/libOpenIGTLink.a)
  SET(LINK_OPENIGTLINK_UTIL_LIBRARY optimized ${PARENT_DIR}/OpenIGTLink-build/bin/Release/libigtlutil.a debug ${PARENT_DIR}/OpenIGTLink-build/bin/Debug/libigtlutil.a)
  target_link_libraries( VideoStreamOpenIGTLink ${PARENT_DIR}/OpenIGTLink-build/bin/Debug/libOpenIGTLink.a ${PARENT_DIR}/OpenH264/libopenh264.a)
  SET(LINK_VIDEOSTREAMOPENIGTLINK_LIBRARY optimized ${VideoStreamOpenIGTLink_BINARY_DIR}/Release/libVideoStreamOpenIGTLink.a debug ${VideoStreamOpenIGTLink_BINARY_DIR}/Debug/libVideoStreamOpenIGTLink.a)
  SET(VideoStreamOpenIGTLink_LIBRARIES ${LINK_VIDEOSTREAMOPENIGTLINK_LIBRARY} ${LINK_OPENIGTLINK_LIBRARY} ${LINK_OPENIGTLINK_UTIL_LIBRARY} ${PARENT_DIR}/OpenH264/libopenh264.a CACHE INTERNAL "a collection of all video streaming libraries.")
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
