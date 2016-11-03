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
  int iRet = 0;
  
#ifdef _MSC_VER
  _setmode (_fileno (stdin), _O_BINARY);  /* thanks to Marcoss Morais <morais at dee.ufcg.edu.br> */
  _setmode (_fileno (stdout), _O_BINARY);
  
  // remove the LOCK_TO_SINGLE_CORE micro, user need to enable it with manual
  // LockToSingleCore();
#endif
  
  /* Control-C handler */
  signal (SIGINT, SigIntHandler);
  
  
  iRet = CreateSVCEncHandle(&(this->pSVCEncoder));
  if (iRet) {
    this->stop = true;
  }
  this->argc = argc;
  this->augments = std::string(argv[1]);
}

int VideoStreamIGTLinkServer::Run()
{
  int iRet;
  if (argc < 2) {
    return 0;
  } else {
    if (strstr (this->augments.c_str(), ".cfg")) { // check configuration type (like .cfg?)
      if (argc == 2) {
        this->bConfigFile = true;
        iRet = StartServer();
        if (iRet > 0)
          return 0;
      }
    }
  }
  this->stop = true;
  return 1;
}

int VideoStreamIGTLinkServer::StartServer ()
{
  if (this->serverSocket.IsNotNull())
  {
    this->serverSocket->CloseSocket();
  }
  this->serverSocket = NULL;
  this->serverSocket = igtl::ServerSocket::New();
  int port     = 18944; //atoi(this->augments[1]);
  int r = serverSocket->CreateServer(port);

  if (r < 0)
  {
  std::cerr << "Cannot create a server socket." << std::endl;
  exit(0);
  }
  
  igtl::MultiThreader::Pointer threader = igtl::MultiThreader::New();
  this->glock = igtl::MutexLock::New();
  this->socket = igtl::Socket::New();

  while (1)
  {
    //------------------------------------------------------------
    // Waiting for Connection
    int threadID = -1;
    socket = serverSocket->WaitForConnection(1000);
    
    if (socket.IsNotNull()) // if client connected
    {
      std::cerr << "A client is connected." << std::endl;
      igtl::MessageHeader::Pointer headerMsg;
      headerMsg = igtl::MessageHeader::New();
      if (!this->waitSTTCommand)
      {
        // Create a message buffer to receive header
        this->interval = 100;
        this->useCompress = true;
        strncpy(this->codecName, "H264", IGTL_VIDEO_CODEC_NAME_SIZE);
        this->glock    = glock;
        this->socket   = socket;
        this->stop     = 0;
        threadID    = threader->SpawnThread((igtl::ThreadFunctionType)(&ThreadFunction), this);
        while (!this->stop)
        {
          igtl::Sleep(10);
        }
        break;
      }
      else if (this->waitSTTCommand)
      {
        //------------------------------------------------------------
        // loop
        for (;;)
        {
          // Initialize receive buffer
          headerMsg->InitPack();
          
          // Receive generic header from the socket
          int rs = socket->Receive(headerMsg->GetPackPointer(), headerMsg->GetPackSize());
          if (rs == 0)
          {
            if (threadID >= 0)
            {
              this->stop = 1;
              threader->TerminateThread(threadID);
              threadID = -1;
            }
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
            socket->Skip(headerMsg->GetBodySizeToRead(), 0);
            std::cerr << "Received a STP_VIDEO message." << std::endl;
            if (threadID >= 0)
            {
              this->stop  = 1;
              threader->TerminateThread(threadID);
              threadID = -1;
              std::cerr << "Disconnecting the client." << std::endl;
              this->socket->CloseSocket();
              this->socket = NULL;  // VERY IMPORTANT. Completely remove the instance.
              
            }
            break;
          }
          else if (strcmp(headerMsg->GetDeviceType(), "STT_VIDEO") == 0)
          {
            std::cerr << "Received a STT_VIDEO message." << std::endl;
            
            igtl::StartVideoDataMessage::Pointer startVideoMsg;
            startVideoMsg = igtl::StartVideoDataMessage::New();
            startVideoMsg->SetMessageHeader(headerMsg);
            startVideoMsg->AllocatePack();
            
            socket->Receive(startVideoMsg->GetPackBodyPointer(), startVideoMsg->GetPackBodySize());
            int c = startVideoMsg->Unpack(1);
            if (c & igtl::MessageHeader::UNPACK_BODY && strcmp(startVideoMsg->GetCodecType().c_str(), "H264")) // if CRC check is OK
            {
              this->interval = startVideoMsg->GetTimeInterval();
              this->useCompress = startVideoMsg->GetUseCompress();
              strncpy(this->codecName, startVideoMsg->GetCodecType().c_str(), IGTL_VIDEO_CODEC_NAME_SIZE);
              this->glock    = glock;
              this->socket   = socket;
              this->stop     = 0;
              threadID    = threader->SpawnThread((igtl::ThreadFunctionType)(&ThreadFunction), this);
            }
          }
          else
          {
            std::cerr << "Receiving : " << headerMsg->GetDeviceType() << std::endl;
            this->socket->Skip(headerMsg->GetBodySizeToRead(), 0);
          }
        }
      }
    }
  }
  
  //------------------------------------------------------------
  // Close connection (The example code never reaches to this section ...)
  serverSocket->CloseSocket();
  this->glock->Lock();
  if (this->socket.IsNotNull())
  {
    this->socket->CloseSocket();
  }
  this->glock->Unlock();
  this->socket = NULL;  // VERY IMPORTANT. Completely remove the instance.
  this->serverSocket = NULL;
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


void SendIGTLinkMessage(VideoStreamIGTLinkServer * td, SSourcePicture* pic, SFrameBSInfo* info)
{
  igtl::VideoMessage::Pointer videoMsg;
  videoMsg = igtl::VideoMessage::New();
  videoMsg->SetDeviceName("Video");
  int frameSize = pic->iPicWidth* pic->iPicHeight * 3 >> 1;
  if (td->useCompress)
  {
    int frameSize = 0;
    int iLayer = 0;
    if (info->iFrameSizeInBytes > 0)
    {
      videoMsg->SetBitStreamSize(info->iFrameSizeInBytes);
      videoMsg->AllocateScalars();
      videoMsg->SetScalarType(videoMsg->TYPE_UINT8);
      videoMsg->SetEndian(igtl_is_little_endian()==true?2:1); //little endian is 2 big endian is 1
      videoMsg->SetWidth(pic->iPicWidth);
      videoMsg->SetHeight(pic->iPicHeight);
      while (iLayer < info->iLayerNum) {
        SLayerBSInfo* pLayerBsInfo = &info->sLayerInfo[iLayer];
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
    videoMsg->SetWidth(pic->iPicWidth);
    videoMsg->SetHeight(pic->iPicHeight);
    videoMsg->m_Frame= pic->pData[0];
    videoMsg->Pack();
  }
  td->glock->Lock();
  for (int i = 0; i < videoMsg->GetNumberOfPackFragments(); i ++)
  {
    td->socket->Send(videoMsg->GetPackFragmentPointer(i), videoMsg->GetPackFragmentSize(i));
  }
  td->glock->Unlock();
  igtl::Sleep(td->interval);
}

void* ThreadFunction(void* ptr)
{
  
  //------------------------------------------------------------
  // Get thread information
  igtl::MultiThreader::ThreadInfo* info =
  static_cast<igtl::MultiThreader::ThreadInfo*>(ptr);
  
  //int id      = info->ThreadID;
  //int nThread = info->NumberOfThreads;
  VideoStreamIGTLinkServer* td = static_cast<VideoStreamIGTLinkServer*>(info->UserData);

  int iRet = 0;
  
  if (td->pSVCEncoder == NULL)
    td->stop = true;
  
  SFrameBSInfo sFbi;
  SEncParamExt sSvcParam;
  int64_t iStart = 0, iTotal = 0;
  
  // Preparing encoding process
  FILE* pFileYUV = NULL;
  int32_t iActualFrameEncodedCount = 0;
  int32_t iFrameIdx = 0;
  int32_t iTotalFrameMax = -1;
  uint8_t* pYUV = NULL;
  SSourcePicture* pSrcPic = NULL;
  uint32_t iSourceWidth, iSourceHeight, kiPicResSize;
  // Inactive with sink with output file handler
  FILE* pFpBs = NULL;
#if defined(COMPARE_DATA)
  //For getting the golden file handle
  FILE* fpGolden = NULL;
#endif
#if defined ( STICK_STREAM_SIZE )
  FILE* fTrackStream = fopen ("coding_size.stream", "wb");
#endif
  SFilesSet fs;
  // for configuration file
  CReadConfig cRdCfg;
  int iParsedNum = 1;
  
  memset (&sFbi, 0, sizeof (SFrameBSInfo));
  td->pSVCEncoder->GetDefaultParams (&sSvcParam);
  memset (&fs.sRecFileName[0][0], 0, sizeof (fs.sRecFileName));
  
  FillSpecificParameters (sSvcParam);
  pSrcPic = new SSourcePicture;
  if (pSrcPic == NULL) {
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }
  //fill default pSrcPic
  pSrcPic->iColorFormat = videoFormatI420;
  pSrcPic->uiTimeStamp = 0;
  
  // if configure file exit, reading configure file firstly
  if (td->bConfigFile) {
    iParsedNum = 2;
    cRdCfg.Openf (td->augments.c_str());// to do get the first augments from td->augments.
    if (!cRdCfg.ExistFile()) {
      fprintf (stderr, "Specified file: %s not exist, maybe invalid path or parameter settting.\n",
               cRdCfg.GetFileName().c_str());
      iRet = 1;
      goto INSIDE_MEM_FREE;
    }
    iRet = ParseConfig (cRdCfg, pSrcPic, sSvcParam, fs);
    if (iRet) {
      fprintf (stderr, "parse svc parameter config file failed.\n");
      iRet = 1;
      goto INSIDE_MEM_FREE;
    }
  }
  /*if (ParseCommandLine (td->argc - iParsedNum, td->augments + iParsedNum, pSrcPic, sSvcParam, fs) != 0) {
    printf ("parse pCommand line failed\n");
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }*/
  td->pSVCEncoder->SetOption (ENCODER_OPTION_TRACE_LEVEL, &g_LevelSetting);
  //finish reading the configurations
  iSourceWidth = pSrcPic->iPicWidth;
  iSourceHeight = pSrcPic->iPicHeight;
  kiPicResSize = iSourceWidth * iSourceHeight * 3 >> 1;
  
  pYUV = new uint8_t [kiPicResSize];
  if (pYUV == NULL) {
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }
  
  //update pSrcPic
  pSrcPic->iStride[0] = iSourceWidth;
  pSrcPic->iStride[1] = pSrcPic->iStride[2] = pSrcPic->iStride[0] >> 1;
  
  pSrcPic->pData[0] = pYUV;
  pSrcPic->pData[1] = pSrcPic->pData[0] + (iSourceWidth * iSourceHeight);
  pSrcPic->pData[2] = pSrcPic->pData[1] + (iSourceWidth * iSourceHeight >> 2);
  
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
  
  iTotalFrameMax = (int32_t)fs.uiFrameToBeCoded;
  //  sSvcParam.bSimulcastAVC = true;
  if (cmResultSuccess != td->pSVCEncoder->InitializeExt (&sSvcParam)) { // SVC encoder initialization
    fprintf (stderr, "SVC encoder Initialize failed\n");
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }
  for (int iLayer = 0; iLayer < MAX_DEPENDENCY_LAYER; iLayer++) {
    if (fs.sRecFileName[iLayer][0] != 0) {
      SDumpLayer sDumpLayer;
      sDumpLayer.iLayer = iLayer;
      sDumpLayer.pFileName = fs.sRecFileName[iLayer];
      if (cmResultSuccess != td->pSVCEncoder->SetOption (ENCODER_OPTION_DUMP_FILE, &sDumpLayer)) {
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
    iRet = 1;
    goto INSIDE_MEM_FREE;
  }
  
  iFrameIdx = 0;

  while (iFrameIdx < iTotalFrameMax && (((int32_t)fs.uiFrameToBeCoded <= 0)
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
    pSrcPic->uiTimeStamp = WELS_ROUND (iFrameIdx * (1000 / sSvcParam.fMaxFrameRate));
    int iEncFrames = td->pSVCEncoder->EncodeFrame (pSrcPic, &sFbi);
    iTotal += WelsTime() - iStart;
    ++ iFrameIdx;
    if (videoFrameTypeSkip == sFbi.eFrameType) {
      continue;
    }
    
    if (iEncFrames == cmResultSuccess) {
      SendIGTLinkMessage(td, pSrcPic, &sFbi);
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
  // Destruction memory introduced in this routine
  if (pFileYUV != NULL) {
    fclose (pFileYUV);
    pFileYUV = NULL;
  }
  if (pYUV) {
    delete[] pYUV;
    pYUV = NULL;
  }
  if (pSrcPic) {
    delete pSrcPic;
    pSrcPic = NULL;
  }
  td->stop = true;
  return NULL;
}



