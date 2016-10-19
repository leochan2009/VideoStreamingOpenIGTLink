/*=========================================================================

  Program:   OpenIGTLink
  Language:  C++

  Copyright (c) Insight Software Consortium. All rights reserved.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include <fstream>
#include <cstring>
#include <stdlib.h>
#include "sha1.h"
#include "igtl_header.h"
#include "igtl_video.h"
#include "welsenc.h"
#include "igtlOSUtil.h"
#include "igtlMessageHeader.h"
#include "igtlVideoMessage.h"
#include "igtlServerSocket.h"
#include "igtlMultiThreader.h"


#define MaximuAugumentNum 30

void* ThreadFunction(void* ptr);

class VideoStreamIGTLinkServer
{
public:
  VideoStreamIGTLinkServer(int argc, char *argv[]);
  ~VideoStreamIGTLinkServer(){};
  
  int StartServer();
  
  int   SendVideoData(igtl::Socket::Pointer& socket, igtl::VideoMessage::Pointer& videoMsg);
  
  ISVCEncoder*  pSVCEncoder;
  
  int   nloop;
  
  igtl::MutexLock::Pointer glock;
  
  igtl::Socket::Pointer socket;
  
  igtl::ServerSocket::Pointer serverSocket;
  
  int   interval;
  
  bool  useCompress;
  
  char  codecName[IGTL_VIDEO_CODEC_NAME_SIZE];
  
  int   stop;

  int argc;
  
  std::string augments;
  
  bool bConfigFile;
  
  int Run();
  
  static bool CompareHash (const unsigned char* digest, const char* hashStr);

  static void UpdateHashFromFrame (SFrameBSInfo& info, SHA1Context* ctx);
};