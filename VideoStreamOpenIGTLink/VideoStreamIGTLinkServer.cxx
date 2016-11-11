/*=========================================================================

  Program:   VideoStreamIGTLinkServer
  Language:  C++

  Copyright (c) Insight Software Consortium. All rights reserved.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "VideoStreamIGTLinkServer.h"
#include "welsencUtil.cpp"

typedef  void* (VideoStreamIGTLinkServer::*Thread2Ptr)(void*);
typedef  void* (*PthreadPtr)(void*);

void UpdateHashFromFrame (SFrameBSInfo& info, SHA1Context* ctx) {
 for (int i = 0; i < info.iLayerNum; ++i) {
 const SLayerBSInfo& layerInfo = info.sLayerInfo[i];
 int layerSize = 0;
 for (int j = 0; j < layerInfo.iNalCount; ++j) {
 layerSize += layerInfo.pNalLengthInByte[j];
 }
 SHA1Input (ctx, layerInfo.pBsBuf, layerSize);
 }
}

VideoStreamIGTLinkServer::VideoStreamIGTLinkServer(int argc, char *argv[])
{
  this->pSVCEncoder = NULL;
  memset (&sFbi, 0, sizeof (SFrameBSInfo));
  pSrcPic = new SSourcePicture;
  int iRet = 0;
  
#ifdef _MSC_VER
  _setmode (_fileno (stdin), _O_BINARY);  /* thanks to Marcoss Morais <morais at dee.ufcg.edu.br> */
  _setmode (_fileno (stdout), _O_BINARY);
  
  // remove the LOCK_TO_SINGLE_CORE micro, user need to enable it with manual
  // LockToSingleCore();
#endif
  
  /* Control-C handler */
  signal (SIGINT, SigIntHandler);
  
  this->serverConnected = false;
  this->argc = argc;
  this->augments = std::string(argv[1]);
  this->waitSTTCommand = true;
  this->InitializationDone = false;
  this->serverPortNumber = -1;
  this->conditionVar = igtl::ConditionVariable::New();
  this->glock = igtl::SimpleMutexLock::New();
}

int VideoStreamIGTLinkServer::StartServer ()
{
  if (this->InitializationDone)
  {
    Thread2Ptr   t = &VideoStreamIGTLinkServer::ThreadFunctionServer;// to avoid the use of static class pointer. http://www.scsc.no/blog/2010/09-03-creating-pthreads-in-c++-using-pointers-to-member-functions.html
    PthreadPtr   p = *(PthreadPtr*)&t;
    igtl::MultiThreader::Pointer threader = igtl::MultiThreader::New();
    threader->SpawnThread((igtl::ThreadFunctionType)p, this);
    this->glock->Lock();
    while(!this->serverConnected)
    {
      this->conditionVar->Wait(this->glock);
    }
    this->glock->Unlock();
    return true;
  }
  else
  {
    return false;
  }
}

int VideoStreamIGTLinkServer::ParseConfigForServer()
{
  string strTag[4];
  int iRet = 1;
  
  while (!cRdCfg.EndOfFile()) {
    long iRd = cRdCfg.ReadLine (&strTag[0]);
    if (iRd > 0) {
      if (strTag[0].empty())
        continue;
      if (strTag[0].compare ("ServerPortNumber") == 0) {
        this->serverPortNumber = atoi (strTag[1].c_str());
        if(this->serverPortNumber<0 || this->serverPortNumber>65535)
        {
          fprintf (stderr, "Invalid parameter for server port number should between 0 and 65525.");
          iRet = 1;
          break;
        }
        else
        {
          iRet = 0;
          break;
        }
      }
    }
  }
  return iRet;
}

void* VideoStreamIGTLinkServer::ThreadFunctionServer(void* ptr)
{
  //------------------------------------------------------------
  // Get thread information
  igtl::MultiThreader::ThreadInfo* info =
    static_cast<igtl::MultiThreader::ThreadInfo*>(ptr);

  VideoStreamIGTLinkServer* parentObj = static_cast<VideoStreamIGTLinkServer*>(info->UserData);

  if (parentObj->serverSocket.IsNotNull())
  {
    parentObj->serverSocket->CloseSocket();
  }
  parentObj->serverSocket = NULL;
  parentObj->serverSocket = igtl::ServerSocket::New();
  
  int r = parentObj->serverSocket->CreateServer(parentObj->serverPortNumber);
  
  if (r < 0)
  {
    std::cerr << "Cannot create a server socket." << std::endl;
    exit(0);
  }
  
  igtl::MultiThreader::Pointer threader = igtl::MultiThreader::New();
  parentObj->socket = igtl::Socket::New();
  
  while (1)
  {
    //------------------------------------------------------------
    // Waiting for Connection
    parentObj->serverConnected     = false;
    parentObj->socket = parentObj->serverSocket->WaitForConnection(1000);
    
    if (parentObj->socket.IsNotNull()) // if client connected
    {
      std::cerr << "A client is connected." << std::endl;
      igtl::MessageHeader::Pointer headerMsg;
      headerMsg = igtl::MessageHeader::New();
      if (!parentObj->waitSTTCommand)
      {
        // Create a message buffer to receive header
        parentObj->interval = 100;
        parentObj->useCompress = true;
        strncpy(parentObj->codecName, "H264", IGTL_VIDEO_CODEC_NAME_SIZE);
        parentObj->InitializationDone = false;
        parentObj->serverConnected     = true;
        parentObj->conditionVar->Signal();
        while (parentObj->serverConnected)
        {
          headerMsg->InitPack();
          int rs = parentObj->socket->Receive(headerMsg->GetPackPointer(), headerMsg->GetPackSize());
          if (rs == 0)
          {
            std::cerr << "Disconnecting the client." << std::endl;
            break;
          }
          else
          {
            igtl::Sleep(10);
          }
        }
      }
      else if (parentObj->waitSTTCommand)
      {
        //------------------------------------------------------------
        // loop
        for (;;)
        {
          // Initialize receive buffer
          headerMsg->InitPack();
          
          // Receive generic header from the socket
          int rs = parentObj->socket->Receive(headerMsg->GetPackPointer(), headerMsg->GetPackSize());
          if (rs == 0)
          {
            std::cerr << "Disconnecting the client." << std::endl;
            break;
          }
          if (rs != headerMsg->GetPackSize())
          {
            continue;
          }
          
          // Deserialize the header
          headerMsg->Unpack();
          
          // Check data type and receive data body
          if (strcmp(headerMsg->GetDeviceType(), "STP_VIDEO") == 0)
          {
            parentObj->socket->Skip(headerMsg->GetBodySizeToRead(), 0);
            std::cerr << "Received a STP_VIDEO message." << std::endl;
            std::cerr << "Disconnecting the client." << std::endl;
            parentObj->InitializationDone = false;
            parentObj->serverConnected = false;
            parentObj->glock->Lock();
            if (parentObj->socket.IsNotNull())
            {
              parentObj->socket->CloseSocket();
            }
            parentObj->glock->Unlock();
            parentObj->socket = NULL;  // VERY IMPORTANT. Completely remove the instance.
            break;
          }
          else if (strcmp(headerMsg->GetDeviceType(), "STT_VIDEO") == 0)
          {
            std::cerr << "Received a STT_VIDEO message." << std::endl;
            
            igtl::StartVideoDataMessage::Pointer startVideoMsg;
            startVideoMsg = igtl::StartVideoDataMessage::New();
            startVideoMsg->SetMessageHeader(headerMsg);
            startVideoMsg->AllocatePack();
            
            parentObj->socket->Receive(startVideoMsg->GetPackBodyPointer(), startVideoMsg->GetPackBodySize());
            int c = startVideoMsg->Unpack(1);
            if (c & igtl::MessageHeader::UNPACK_BODY && strcmp(startVideoMsg->GetCodecType().c_str(), "H264")) // if CRC check is OK
            {
              parentObj->interval = startVideoMsg->GetTimeInterval();
              parentObj->useCompress = startVideoMsg->GetUseCompress();
              strncpy(parentObj->codecName, startVideoMsg->GetCodecType().c_str(), IGTL_VIDEO_CODEC_NAME_SIZE);
              parentObj->serverConnected     = true;
              parentObj->conditionVar->Signal();
            }
          }
          else
          {
            std::cerr << "Receiving : " << headerMsg->GetDeviceType() << std::endl;
            parentObj->socket->Skip(headerMsg->GetBodySizeToRead(), 0);
          }
        }
      }
    }
  }
  
  //------------------------------------------------------------
  // Close connection (The example code never reaches to this section ...)
  parentObj->serverSocket->CloseSocket();
  parentObj->glock->Lock();
  if (parentObj->socket.IsNotNull())
  {
    parentObj->socket->CloseSocket();
  }
  parentObj->glock->Unlock();
  parentObj->socket = NULL;  // VERY IMPORTANT. Completely remove the instance.
  parentObj->serverSocket = NULL;
}

bool VideoStreamIGTLinkServer::CompareHash (const unsigned char* digest, const char* hashStr) {
  char hashStrCmp[SHA_DIGEST_LENGTH * 2 + 1];
  for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
    sprintf (&hashStrCmp[i * 2], "%.2x", digest[i]);
  }
  hashStrCmp[SHA_DIGEST_LENGTH * 2] = '\0';
  if (hashStr == hashStrCmp)
  {
    return true;
  }
  return false;
}

bool VideoStreamIGTLinkServer::InitializeEncoderAndServer()
{
  //------------------------------------------------------------
  int iRet = 0;
  iRet = CreateSVCEncHandle(&(this->pSVCEncoder));
  if (this->pSVCEncoder == NULL)
    this->serverConnected = false;
  
  int iTotalFrameMax;
  
  // Preparing encoding process
  
  // Inactive with sink with output file handler
  FILE* pFpBs = NULL;
#if defined(COMPARE_DATA)
  //For getting the golden file handle
  FILE* fpGolden = NULL;
#endif
#if defined ( STICK_STREAM_SIZE )
  FILE* fTrackStream = fopen ("coding_size.stream", "wb");
#endif
  this->pSVCEncoder->GetDefaultParams (&sSvcParam);
  memset (&fs.sRecFileName[0][0], 0, sizeof (fs.sRecFileName));
  
  FillSpecificParameters (sSvcParam);
  if (pSrcPic == NULL) {
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }
  //fill default pSrcPic
  pSrcPic->iColorFormat = videoFormatI420;
  pSrcPic->uiTimeStamp = 0;
  
  // if configure file exit, reading configure file firstly
  cRdCfg.Openf (this->augments.c_str());// to do get the first augments from this->augments.
  if (cRdCfg.ExistFile())
  {
    iRet = ParseConfig (cRdCfg, pSrcPic, sSvcParam, fs);
    if (iRet) {
      fprintf (stderr, "parse svc parameter config file failed.\n");
      iRet = 1;
      goto INSIDE_MEM_FREE;
    }
    cRdCfg.~CReadConfig();
    cRdCfg.Openf (this->augments.c_str());// reset the file read pointer to the beginning.
    iRet = ParseConfigForServer();
    if (iRet) {
      fprintf (stderr, "parse server parameter config file failed.\n");
      iRet = 1;
      goto INSIDE_MEM_FREE;
    }
  }
  else
  {
    fprintf (stderr, "Specified file: %s not exist, maybe invalid path or parameter settting.\n",
             cRdCfg.GetFileName().c_str());
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }
  iTotalFrameMax = (int32_t)fs.uiFrameToBeCoded;
  this->pSVCEncoder->SetOption (ENCODER_OPTION_TRACE_LEVEL, &g_LevelSetting);
  //finish reading the configurations
  uint32_t iSourceWidth, iSourceHeight, kiPicResSize;
  iSourceWidth = pSrcPic->iPicWidth;
  iSourceHeight = pSrcPic->iPicHeight;
  kiPicResSize = iSourceWidth * iSourceHeight * 3 >> 1;
  
  //update pSrcPic
  pSrcPic->iStride[0] = iSourceWidth;
  pSrcPic->iStride[1] = pSrcPic->iStride[2] = pSrcPic->iStride[0] >> 1;
  
  //update sSvcParam
  sSvcParam.iPicWidth = 0;
  sSvcParam.iPicHeight = 0;
  for (int iLayer = 0; iLayer < sSvcParam.iSpatialLayerNum; iLayer++) {
    SSpatialLayerConfig* pDLayer = &sSvcParam.sSpatialLayers[iLayer];
    sSvcParam.iPicWidth = WELS_MAX (sSvcParam.iPicWidth, pDLayer->iVideoWidth);
    sSvcParam.iPicHeight = WELS_MAX (sSvcParam.iPicHeight, pDLayer->iVideoHeight);
  }
  //if target output resolution is not set, use the source size
  sSvcParam.iPicWidth = (!sSvcParam.iPicWidth) ? iSourceWidth : sSvcParam.iPicWidth;
  sSvcParam.iPicHeight = (!sSvcParam.iPicHeight) ? iSourceHeight : sSvcParam.iPicHeight;
  
  //  sSvcParam.bSimulcastAVC = true;
  if (cmResultSuccess != this->pSVCEncoder->InitializeExt (&sSvcParam)) { // SVC encoder initialization
    fprintf (stderr, "SVC encoder Initialize failed\n");
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }
  for (int iLayer = 0; iLayer < MAX_DEPENDENCY_LAYER; iLayer++) {
    if (fs.sRecFileName[iLayer][0] != 0) {
      SDumpLayer sDumpLayer;
      sDumpLayer.iLayer = iLayer;
      sDumpLayer.pFileName = fs.sRecFileName[iLayer];
      if (cmResultSuccess != this->pSVCEncoder->SetOption (ENCODER_OPTION_DUMP_FILE, &sDumpLayer)) {
        fprintf (stderr, "SetOption ENCODER_OPTION_DUMP_FILE failed!\n");
        iRet = 1;
        goto INSIDE_MEM_FREE;
      }
    }
  }
  // Inactive with sink with output file handler
  if (fs.strBsFile.length() > 0) {
    pFpBs = fopen (fs.strBsFile.c_str(), "wb");
    if (pFpBs == NULL) {
      fprintf (stderr, "Can not open file (%s) to write bitstream!\n", fs.strBsFile.c_str());
      iRet = 1;
      goto INSIDE_MEM_FREE;
    }
  }
  
#if defined(COMPARE_DATA)
  //For getting the golden file handle
  if ((fpGolden = fopen (argv[3], "rb")) == NULL) {
    fprintf (stderr, "Unable to open golden sequence file, check corresponding path!\n");
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }
#endif
  this->InitializationDone = true;
  return true;
INSIDE_MEM_FREE:
  if (pFpBs) {
    fclose (pFpBs);
    pFpBs = NULL;
  }
#if defined (STICK_STREAM_SIZE)
  if (fTrackStream) {
    fclose (fTrackStream);
    fTrackStream = NULL;
  }
#endif
#if defined (COMPARE_DATA)
  if (fpGolden) {
    fclose (fpGolden);
    fpGolden = NULL;
  }
#endif
  if (pSrcPic) {
    delete pSrcPic;
    pSrcPic = NULL;
  }
  this->serverConnected = false;
  this->InitializationDone = false;
  return false;
}

void VideoStreamIGTLinkServer::SendIGTLinkMessage()
{
  igtl::VideoMessage::Pointer videoMsg;
  videoMsg = igtl::VideoMessage::New();
  videoMsg->SetDeviceName("Video");
  int frameSize = pSrcPic->iPicWidth* pSrcPic->iPicHeight * 3 >> 1;
  if (this->useCompress)
  {
    int frameSize = 0;
    int iLayer = 0;
    if (sFbi.iFrameSizeInBytes > 0)
    {
      videoMsg->SetBitStreamSize(sFbi.iFrameSizeInBytes);
      videoMsg->AllocateScalars();
      videoMsg->SetScalarType(videoMsg->TYPE_UINT8);
      videoMsg->SetEndian(igtl_is_little_endian()==true?2:1); //little endian is 2 big endian is 1
      videoMsg->SetWidth(pSrcPic->iPicWidth);
      videoMsg->SetHeight(pSrcPic->iPicHeight);
      while (iLayer < sFbi.iLayerNum) {
        SLayerBSInfo* pLayerBsInfo = &sFbi.sLayerInfo[iLayer];
        if (pLayerBsInfo != NULL) {
          int iLayerSize = 0;
          int iNalIdx = pLayerBsInfo->iNalCount - 1;
          do {
            iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
            -- iNalIdx;
          } while (iNalIdx >= 0);
          frameSize += iLayerSize;
          for (int i = 0; i < iLayerSize ; i++)
          {
            videoMsg->GetPackFragmentPointer(2)[frameSize-iLayerSize+i] = pLayerBsInfo->pBsBuf[i];
          }
        }
        ++ iLayer;
      }
      videoMsg->Pack();
    }
  }
  else
  {
    videoMsg->SetBitStreamSize(frameSize);
    videoMsg->AllocateScalars();
    videoMsg->SetScalarType(videoMsg->TYPE_UINT8);
    videoMsg->SetEndian(igtl_is_little_endian()==true?2:1); //little endian is 2 big endian is 1
    videoMsg->SetWidth(pSrcPic->iPicWidth);
    videoMsg->SetHeight(pSrcPic->iPicHeight);
    videoMsg->m_Frame= pSrcPic->pData[0];
    videoMsg->Pack();
  }
  this->glock->Lock();
  if(this->socket)
  {
    for (int i = 0; i < videoMsg->GetNumberOfPackFragments(); i ++)
    {
      this->socket->Send(videoMsg->GetPackFragmentPointer(i), videoMsg->GetPackFragmentSize(i));
    }
  }
  this->glock->Unlock();
  igtl::Sleep(this->interval);
}

void* VideoStreamIGTLinkServer::EncodeFile(void)
{
  if(!this->InitializationDone)
  {
    this->InitializeEncoderAndServer();
  }
  int64_t iStart = 0, iTotal = 0;
  int32_t iActualFrameEncodedCount = 0;
  int32_t iFrameIdx = 0;
  int32_t iTotalFrameMax = -1;
  int kiPicResSize = pSrcPic->iPicWidth*pSrcPic->iPicHeight*3>>1;
  FILE* pFileYUV = NULL;
  bool fileValid = true;
  pFileYUV = fopen (fs.strSeqFile.c_str(), "rb");
  if (pFileYUV != NULL) {
#if defined(_WIN32) || defined(_WIN64)
#if _MSC_VER >= 1400
    if (!_fseeki64 (pFileYUV, 0, SEEK_END)) {
      int64_t i_size = _ftelli64 (pFileYUV);
      _fseeki64 (pFileYUV, 0, SEEK_SET);
      iTotalFrameMax = WELS_MAX ((int32_t) (i_size / kiPicResSize), iTotalFrameMax);
    }
#else
    if (!fseek (pFileYUV, 0, SEEK_END)) {
      int64_t i_size = ftell (pFileYUV);
      fseek (pFileYUV, 0, SEEK_SET);
      iTotalFrameMax = WELS_MAX ((int32_t) (i_size / kiPicResSize), iTotalFrameMax);
    }
#endif
#else
    if (!fseeko (pFileYUV, 0, SEEK_END)) {
      int64_t i_size = ftello (pFileYUV);
      fseeko (pFileYUV, 0, SEEK_SET);
      iTotalFrameMax = WELS_MAX ((int32_t) (i_size / kiPicResSize), iTotalFrameMax);
    }
#endif
  } else {
    fprintf (stderr, "Unable to open source sequence file (%s), check corresponding path!\n",
             fs.strSeqFile.c_str());
    fileValid = false;
  }
  
  uint8_t* pYUV = new uint8_t[kiPicResSize];
  this->SetInputFramePointer(pYUV);
  while (fileValid && iFrameIdx < iTotalFrameMax && (((int32_t)fs.uiFrameToBeCoded <= 0)
                                        || (iFrameIdx < (int32_t)fs.uiFrameToBeCoded))) {
    
#ifdef ONLY_ENC_FRAMES_NUM
    // Only encoded some limited frames here
    if (iActualFrameEncodedCount >= ONLY_ENC_FRAMES_NUM) {
      break;
    }
#endif//ONLY_ENC_FRAMES_NUM
    bool bCanBeRead = false;
    
    bCanBeRead = (fread (pYUV, 1, kiPicResSize, pFileYUV) == kiPicResSize);
    
    if (!bCanBeRead)
      break;
    // To encoder this frame
    iStart = WelsTime();
    this->pSrcPic->uiTimeStamp = WELS_ROUND (iFrameIdx * (1000 / sSvcParam.fMaxFrameRate));
    int iEncFrames = this->pSVCEncoder->EncodeFrame (pSrcPic, &sFbi);
    iTotal += WelsTime() - iStart;
    ++ iFrameIdx;
    if (sFbi.eFrameType == videoFrameTypeSkip) {
      continue;
    }
    
    if (iEncFrames == cmResultSuccess ) {
      SendIGTLinkMessage();
#if defined (STICK_STREAM_SIZE)
      if (fTrackStream) {
        fwrite (&iFrameSize, 1, sizeof (int), fTrackStream);
      }
#endif//STICK_STREAM_SIZE
      ++ iActualFrameEncodedCount; // excluding skipped frame time
    } else {
      fprintf (stderr, "EncodeFrame(), ret: %d, frame index: %d.\n", iEncFrames, iFrameIdx);
    }
  }
  delete[] pYUV;
  pYUV = NULL;
  if (iActualFrameEncodedCount > 0) {
    double dElapsed = iTotal / 1e6;
    printf ("Width:\t\t%d\nHeight:\t\t%d\nFrames:\t\t%d\nencode time:\t%f sec\nFPS:\t\t%f fps\n",
            sSvcParam.iPicWidth, sSvcParam.iPicHeight,
            iActualFrameEncodedCount, dElapsed, (iActualFrameEncodedCount * 1.0) / dElapsed);
#if defined (WINDOWS_PHONE)
    g_fFPS = (iActualFrameEncodedCount * 1.0f) / (float) dElapsed;
    g_dEncoderTime = dElapsed;
    g_iEncodedFrame = iActualFrameEncodedCount;
#endif
  }
  this->serverConnected = false;
  return NULL;
}



int VideoStreamIGTLinkServer::EncodeSingleFrame()
{
  int encodeRet = pSVCEncoder->EncodeFrame(pSrcPic, &sFbi);
  this->videoFrameType = sFbi.eFrameType;
  return encodeRet;
}

void VideoStreamIGTLinkServer::SetInputFramePointer(uint8_t* picPointer)
{
  if (this->InitializationDone == true)
  {
    int iSourceWidth = pSrcPic->iPicWidth;
    int iSourceHeight = pSrcPic->iPicHeight;
    pSrcPic->pData[0] = picPointer;
    pSrcPic->pData[1] = pSrcPic->pData[0] + (iSourceWidth * iSourceHeight);
    pSrcPic->pData[2] = pSrcPic->pData[1] + (iSourceWidth * iSourceHeight >> 2);
  }
}


