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
#include "igtlOSUtil.h"
#include "igtlMessageHeader.h"
#include "igtlVideoMessage.h"
#include "igtlServerSocket.h"
#include "igtlMultiThreader.h"
#include "igtlConditionVariable.h"
#include "codec_def.h"
#include "codec_app_def.h"
#include "read_config.h"
#include "wels_const.h"
#include "welsencUtil.h"

class ISVCEncoder;

class VideoStreamIGTLinkServer
{
public:
  VideoStreamIGTLinkServer(int argc, char *argv[]);
  ~VideoStreamIGTLinkServer(){};
  
  /**
   Start the server, this function will be holding the main program for a client connection.
   */
  int StartServer();
  
  /**
   Parse the configuration file to initialize the encoder and server.
   */
  bool InitializeEncoderAndServer();
  
  /**
   Set the input frame pointer , the encoder will read the frame from the pointer
   */
  void SetInputFramePointer(uint8_t* picPointer);
  
  /**
   Encode a frame, for performance issue, before encode the frame, make sure the frame pointer is updated with a new frame.
   Otherwize, the old frame will be encoded.
   */
  int EncodeSingleFrame();
  
  /**
   Pack the encoded frame into a OpenIGTLink message and send the message to a client.
   */
  void SendIGTLinkMessage();
  
  /**
   Set the server to wait for STT command or just send the bitstream when connection is setup.
   */
  void SetWaitSTTCommand(bool needSTTCommand){ waitSTTCommand = needSTTCommand;};
  
  /**
   Get the encoder and server initialization status.
   */
  bool GetInitializationStatus(){return InitializationDone;};
  
  /**
   Get Server connection status, true for connected, false for not connected
   */
  bool GetServerConnectStatus(){return serverConnected;}
  
  /**
   Encode the video stream from a source file
   */
  void* EncodeFile(void);
  
  /**
   Get the type of encoded frame
   */
  int GetVideoFrameType(){return videoFrameType;};
  
  int ParseConfigForServer();
  
  //void* ThreadFunctionServer(void*);
  
  static bool CompareHash (const unsigned char* digest, const char* hashStr);
  
  igtl::SimpleMutexLock* glock;
  
  igtl::Socket::Pointer socket;
  
  igtl::ServerSocket::Pointer serverSocket;
  
  ISVCEncoder*  pSVCEncoder;
  
  SEncParamExt sSvcParam;
  
  SFilesSet fs;
  // for configuration file
  
  CReadConfig cRdCfg;
  
  SFrameBSInfo sFbi;
  
  SSourcePicture* pSrcPic;
  
  igtl::ConditionVariable::Pointer conditionVar;
  
  int videoFrameType;
  
  int   interval;
  
  bool  useCompress;
  
  char  codecName[IGTL_VIDEO_CODEC_NAME_SIZE];
  
  int   serverConnected;
  
  int   serverPortNumber;
  
  int argc;
  
  std::string augments;
    
  bool waitSTTCommand;
  
  std::string deviceName;
  
  bool InitializationDone;
  
};