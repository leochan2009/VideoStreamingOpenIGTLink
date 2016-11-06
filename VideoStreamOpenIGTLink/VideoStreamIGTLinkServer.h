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

#define MaximuAugumentNum 30

typedef struct LayerpEncCtx_s {
  int32_t       iDLayerQp;
  SSliceArgument  sSliceArgument;
} SLayerPEncCtx;

typedef struct tagFilesSet {
  std::string strBsFile;
  std::string strSeqFile;    // frame File to read
  std::string strLayerCfgFile[MAX_DEPENDENCY_LAYER];
  char   sRecFileName[MAX_DEPENDENCY_LAYER][MAX_FNAME_LEN];
  uint32_t uiFrameToBeCoded;
} SFilesSet;


class ISVCEncoder;
//void* ThreadFunction(void* ptr);
class VideoStreamIGTLinkServer
{
public:
  VideoStreamIGTLinkServer(int argc, char *argv[]);
  ~VideoStreamIGTLinkServer(){};
  
  int StartServer(int portNum);
  
  bool InitializeEncoder();
  
  void SetInputFramePointer(uint8_t* picPointer);
  
  int encodeSingleFrame();
  
  int SendVideoData(igtl::Socket::Pointer& socket, igtl::VideoMessage::Pointer& videoMsg);
  
  void SendIGTLinkMessage();
  
  void* ThreadFunctionEncodeFile(void);
  
  void* ThreadFunctionServer(void);
  
  ISVCEncoder*  pSVCEncoder;
  
  int   nloop;
  
  igtl::SimpleMutexLock* glock;
  
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

  bool waitSTTCommand;
  
  bool InitializationDone;

private:
  SEncParamExt sSvcParam;
  SFilesSet fs;
  // for configuration file
  CReadConfig cRdCfg;
  SFrameBSInfo sFbi;
  SSourcePicture* pSrcPic;
  
  igtl::ConditionVariable::Pointer conditionVar;
  
};