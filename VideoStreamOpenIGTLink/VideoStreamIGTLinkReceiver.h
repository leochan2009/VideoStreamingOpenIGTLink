/*=========================================================================
 
 Program:   OpenIGTLink
 Language:  C++
 
 Copyright (c) Insight Software Consortium. All rights reserved.
 
 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notices for more information.
 
 =========================================================================*/
#if defined(_WIN32) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
// need link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

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
#include "igtlUDPClientSocket.h"
#include "igtlMessageRTPWrapper.h"
#include "igtlConditionVariable.h"
#include "igtlTimeStamp.h"
#include "read_config.h"
#include "H264Decoder.h"
#include "../EvaluationTool.h"

class VideoStreamIGTLinkReceiver
{
public:
  VideoStreamIGTLinkReceiver(char *argv[]);
  ~VideoStreamIGTLinkReceiver(){};
  
  int ProcessVideoStream(uint8_t* bitStream);
  
  bool InitializeClient();
  
  void SendStopMessage();
  
  int ParseConfigForClient();
  
  std::string deviceName;
  
  int transportMethod;
  
  ISVCDecoder*  pSVCDecoder;
  
  SDecodingParam decParam;
  
  unsigned char* decodedFrame;
  
  char* kpOuputFileName;
  
  char* pOptionFileName;
  
  igtl::MutexLock::Pointer glock;
  
  igtl::ClientSocket::Pointer socket;
  
  igtl::UDPClientSocket::Pointer UDPSocket;
  
  uint8_t * videoMessageBuffer;
  
  int   interval;
  
  bool  useCompress;
  
  CReadConfig cRdCfg;
  
  int TCPServerPort;
  
  int UDPClientPort;
  
  char * TCPServerIPAddress;
  
  igtl::MessageRTPWrapper::Pointer rtpWrapper;
  
  char codecType[IGTL_VIDEO_CODEC_NAME_SIZE];
  
  int argc;
  
  std::string augments;
  
  void SetWidth(int iWidth);
  
  void SetHeight(int iHeight);
  
  void SetStreamLength(int iStreamLength);
  
  void SetDecodedFrame();
  
  int Width;
  
  int Height;
  
  int StreamLength;
  
  int RunOnTCPSocket();
  
  int RunOnUDPSocket();
  
  H264Decode* H264DecodeInstance;
  
  int YUV420ToRGBConversion(uint8_t *RGBFrame, uint8_t * YUV420Frame, int iHeight, int iWidth);
  
  bool flipAtX;
  
  igtl::TimeStamp::Pointer ReceiverTimerDecodeThread;
  
  igtl::TimeStamp::Pointer ReceiverTimerPaketThread;
  
  EvaluationTool* evalToolPaketThread;
  
  EvaluationTool* evalToolDecodeThread;

};
