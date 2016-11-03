# Generate the VideoStreamOpenIGTLinkConfig.cmake file in the build tree.  Also configure
# one for installation.  The file tells external projects how to use
# VideoStreamOpenIGTLink.

#-----------------------------------------------------------------------------
# Settings specific to the build tree.

# The "use" file.
SET(VideoStreamOpenIGTLink_USE_FILE ${VideoStreamOpenIGTLink_BINARY_DIR}/UseVideoStreamOpenIGTLink.cmake)

# Library directory.
SET(VideoStreamOpenIGTLink_LIBRARY_DIRS_CONFIG ${VideoStreamOpenIGTLink_LIBRARY_PATH})

# Determine the include directories needed.
SET(VideoStreamOpenIGTLink_INCLUDE_DIRS_CONFIG
  ${VideoStreamOpenIGTLink_INCLUDE_PATH}
)
# Libraries.
SET(VideoStreamOpenIGTLink_LIBRARIES_CONFIG ${VideoStreamOpenIGTLink_LIBRARIES})


#-----------------------------------------------------------------------------
# Configure OpenIGTLinkConfig.cmake for the build tree.
CONFIGURE_FILE(${VideoStreamOpenIGTLink_SOURCE_DIR}/VideoStreamOpenIGTLinkConfig.cmake.in
               ${VideoStreamOpenIGTLink_BINARY_DIR}/VideoStreamOpenIGTLinkConfig.cmake @ONLY IMMEDIATE)
