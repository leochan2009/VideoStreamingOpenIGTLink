/*=========================================================================
 
 Program:   OpenIGTLink
 Language:  C++
 
 Copyright (c) Insight Software Consortium. All rights reserved.
 
 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notices for more information.
 
 =========================================================================*/

#include <fstream>
#include <climits>
#include <cstring>
#include <stdlib.h>
#include "api/svc/codec_api.h"
#include "api/svc/codec_app_def.h"
#include "sha1.h"
#include "igtlOSUtil.h"
#include "igtlMessageHeader.h"
#include "igtlVideoMessage.h"
#include "igtlServerSocket.h"
#include "igtlMultiThreader.h"

#include "H264Decoder.h"

class VideoStreamIGTLinkReceiver
{
public:
  VideoStreamIGTLinkReceiver();
  ~VideoStreamIGTLinkReceiver(){};
  
  int ReceiveVideoData(igtl::ClientSocket::Pointer& socket, igtl::MessageHeader::Pointer& header);
  
  void SendStopMessage();
  
  ISVCDecoder*  pSVCDecoder;
  
  SDecodingParam decParam;
  
  char* kpOuputFileName;
  
  char* pOptionFileName;
  
  igtl::MutexLock::Pointer glock;
  
  igtl::ClientSocket::Pointer socket;
  
  int   interval;
  
  bool  useCompress;
  
  std::string  hostname;
  
  int port;
  
  char codecType[IGTL_VIDEO_CODEC_NAME_SIZE];
  
  int argc;
  
  std::string augments;
  
  int Run();
};
