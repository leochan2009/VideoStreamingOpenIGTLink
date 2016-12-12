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

void* ThreadFunctionServer(void* ptr);

void* ThreadFunctionUDPServer(void* ptr);

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

VideoStreamIGTLinkServer::VideoStreamIGTLinkServer(char *argv[])
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
  this->augments = std::string(argv[1]);
  this->waitSTTCommand = true;
  this->InitializationDone = false;
  this->serverPortNumber = -1;
  this->deviceName = "";
  this->serverSocket = igtl::ServerSocket::New();
  this->serverUDPSocket = igtl::UDPServerSocket::New();
  this->socket = igtl::Socket::New();;
  this->conditionVar = igtl::ConditionVariable::New();
  this->glock = igtl::SimpleMutexLock::New();
  this->rtpWrapper = igtl::MessageRTPWrapper::New();
  this->ServerTimer = igtl::TimeStamp::New();
  this->evalTool = new EvaluationTool();
}

int VideoStreamIGTLinkServer::StartServer ()
{
  if (this->InitializationDone)
  {
    // Initialize the evaluation file
    std::string fileName = std::string(this->deviceName);
    if (transportMethod == 0)
    {
      fileName.append("-UseTCP");
    }
    else
    {
      fileName.append("-UseUDP");
    }
    if (useCompress == 0)
    {
      fileName.append("-Original-");
    }
    else
    {
      fileName.append("-Compressed-");
    }
    fileName.append("Server-");
    ServerTimer->GetTime();
    char buffertemp[64];
    sprintf(buffertemp, "%llu", ServerTimer->GetTimeStampUint64());
    fileName.append(buffertemp);
    this->evalTool->filename = fileName;
    std::string headLine = "NAL-Unit Fragment-Number Before-Encoding After-Encoding Sending-Paket";
    this->evalTool->AddAnElementToLine(headLine);
    this->evalTool->WriteCurrentLineToFile();
    //----------------------------
    
    if(this->transportMethod==VideoStreamIGTLinkServer::UseTCP)
    {
      igtl::MultiThreader::Pointer threader = igtl::MultiThreader::New();
      threader->SpawnThread((igtl::ThreadFunctionType)&ThreadFunctionServer, this);
      this->glock->Lock();
      while(!this->serverConnected)
      {
        this->conditionVar->Wait(this->glock);
      }
      this->glock->Unlock();
      return true;
    }
    else if (this->transportMethod == VideoStreamIGTLinkServer::UseUDP)
    {
      igtl::MultiThreader::Pointer threader = igtl::MultiThreader::New();
      threader->SpawnThread((igtl::ThreadFunctionType)&ThreadFunctionUDPServer, this);
      this->glock->Lock();
      while(!this->serverConnected)
      {
        this->conditionVar->Wait(this->glock);
      }
      this->glock->Unlock();
      return true;
    }
  }
  else
  {
    return false;
  }
  return false;
}

int VideoStreamIGTLinkServer::ParseConfigForServer()
{
  int iRet = 1;
  int arttributNum = 0;
  std::string strTag[4];
  while (!cRdCfg.EndOfFile()) {
    strTag->clear();
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
          arttributNum ++;
        }
        else
        {
          iRet = 0;
        }
      }
      if (strTag[0].compare ("ClientIPAddress") == 0) {
        this->clientIPAddress = new char[IP4AddressStrLen];
        memcpy(this->clientIPAddress, strTag[1].c_str(), IP4AddressStrLen);
        if(inet_addr(this->clientIPAddress))
        {
          iRet = 0;
        }
        else
        {
          fprintf (stderr, "Invalid parameter for IP address");
          iRet = 1;
          arttributNum ++;
        }
      }
      if (strTag[0].compare ("ClientPortNumber") == 0) {
        this->clientPortNumber = atoi (strTag[1].c_str());
        if(this->clientPortNumber<0 || this->clientPortNumber>65535)
        {
          fprintf (stderr, "Invalid parameter for server port number should between 0 and 65525.");
          iRet = 1;
          arttributNum ++;
        }
        else
        {
          iRet = 0;
        }
          
      }
      if (strTag[0].compare ("DeviceName") == 0)
      {
        this->deviceName =strTag[1].c_str();
        arttributNum ++;
      }
      if (strTag[0].compare ("TransportMethod") == 0)
      {
        this->transportMethod = atoi(strTag[1].c_str());
        arttributNum ++;
      }
      if (strTag[0].compare ("UseCompress") == 0)
      {
        this->useCompress = atoi(strTag[1].c_str());
        arttributNum ++;
      }
    }
    if (arttributNum ==20) // only parse the first 20 arttribute
    {
      break;
    }
  }
  if (this->transportMethod == 1 )
  {
    if (this->clientIPAddress && this->clientPortNumber)
    {
      this->serverUDPSocket->AddClient(this->clientIPAddress, this->clientPortNumber, 0);
      this->interval = 100;
    }
    else
    {
      iRet = 1;
    }
  }
  return iRet;
}

void* ThreadFunctionServer(void* ptr)
{
  //------------------------------------------------------------
  // Get thread information
  igtl::MultiThreader::ThreadInfo* info =
    static_cast<igtl::MultiThreader::ThreadInfo*>(ptr);

  VideoStreamIGTLinkServer* parentObj = static_cast<VideoStreamIGTLinkServer*>(info->UserData);
  int r = -1;
  if (parentObj->serverSocket.IsNotNull())
  {
    parentObj->serverSocket->CloseSocket();
  }
  r = parentObj->serverSocket->CreateServer(parentObj->serverPortNumber);
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



void* ThreadFunctionUDPServer(void* ptr)
{
  //------------------------------------------------------------
  // Get thread information
  igtl::MultiThreader::ThreadInfo* info =
  static_cast<igtl::MultiThreader::ThreadInfo*>(ptr);
  
  VideoStreamIGTLinkServer* parentObj = static_cast<VideoStreamIGTLinkServer*>(info->UserData);
  int r = -1;
  if (parentObj->serverUDPSocket.IsNotNull())
  {
    parentObj->serverUDPSocket->CloseSocket();
  }
  r = parentObj->serverUDPSocket->CreateUDPServer(parentObj->serverPortNumber);
  if (r < 0)
  {
    std::cerr << "Cannot create a server socket." << std::endl;
    exit(0);
  }
  strncpy(parentObj->codecName, "H264", IGTL_VIDEO_CODEC_NAME_SIZE);
  parentObj->serverConnected     = true;
  parentObj->conditionVar->Signal();
  
  while (1)
  {
    /*igtl::MessageHeader::Pointer headerMsg = igtl::MessageHeader::New();
    headerMsg->InitPack();
    int rs = parentObj->socket->Receive(headerMsg->GetPackPointer(), headerMsg->GetPackSize());
    if (rs == 0)
    {
      std::cerr << "Disconnecting the client." << std::endl;
      break;
    }
    else */
    {
      igtl::Sleep(10);
    }
  }
  
  //------------------------------------------------------------
  // Close connection (The example code never reaches to this section ...)
  parentObj->serverUDPSocket->CloseSocket();
  parentObj->glock->Lock();
  if (parentObj->socket.IsNotNull())
  {
    parentObj->socket->CloseSocket();
  }
  parentObj->glock->Unlock();
  parentObj->socket = NULL;  // VERY IMPORTANT. Completely remove the instance.
  parentObj->serverUDPSocket = NULL;
  return NULL;
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

void VideoStreamIGTLinkServer::SendOriginalData()
{
  if(this->useCompress == 0)
  {
    int kiPicResSize = pSrcPic->iPicWidth*pSrcPic->iPicHeight*3>>1;
    uint8_t* pYUV = new uint8_t[kiPicResSize];
    this->SetInputFramePointer(pYUV);
    bool bCanBeRead = false;
    FILE* pFileYUV = fopen (fs.strSeqFile.c_str(), "rb");
    if (pFileYUV != NULL)
    {
      while (1)
      {
        bCanBeRead = (fread (pYUV, 1, kiPicResSize, pFileYUV) == kiPicResSize);
        if (!bCanBeRead)
          break;
        int frameSize = pSrcPic->iPicWidth* pSrcPic->iPicHeight * 3 >> 1;
        igtl::VideoMessage::Pointer videoMsg;
        videoMsg = igtl::VideoMessage::New();
        videoMsg->SetHeaderVersion(IGTL_HEADER_VERSION_2);
        videoMsg->SetBitStreamSize(frameSize);
        videoMsg->AllocateBuffer();
        videoMsg->SetScalarType(videoMsg->TYPE_UINT8);
        videoMsg->SetEndian(igtl_is_little_endian()==true?2:1); //little endian is 2 big endian is 1
        videoMsg->SetWidth(pSrcPic->iPicWidth);
        videoMsg->SetHeight(pSrcPic->iPicHeight);
        static igtl_uint32 messageID = 0;
        videoMsg->SetMessageID(messageID);
        messageID ++;
        videoMsg->m_Frame= pSrcPic->pData[0];
        ServerTimer->GetTime();
        videoMsg->SetTimeStamp(ServerTimer);
        videoMsg->Pack();
        if(this->transportMethod == UseUDP)
        {
          
          // to do uncompressed data using UDP
          this->glock->Lock();
          if(this->socket)
          {
            rtpWrapper->WrapMessageAndSend(this->serverUDPSocket, (igtlUint8 *)videoMsg->GetPackPointer(), videoMsg->GetPackSize());
          }
          this->glock->Unlock();
          for (int i = 0; i< rtpWrapper->fragmentNumberList.size(); i++)
          {
            char buffertemp[64];
            sprintf(buffertemp, "%lu", messageID);
            evalTool->AddAnElementToLine(std::string(buffertemp));

            sprintf(buffertemp, "%lu", rtpWrapper->fragmentNumberList[i]);
            evalTool->AddAnElementToLine(std::string(buffertemp));
            
            sprintf(buffertemp, "%llu", rtpWrapper->paketSendTimeStampList[i]);
            evalTool->AddAnElementToLine(std::string(buffertemp));
            
            evalTool->WriteCurrentLineToFile();
          }
          igtl::Sleep(500);
        }
        else if(this->transportMethod == UseTCP)
        {
          this->glock->Lock();
          if(this->socket)
          {
            this->serverSocket->Send(videoMsg->GetPackPointer(), videoMsg->GetPackSize());
          }
          this->glock->Unlock();
          char buffertemp[64];
          sprintf(buffertemp, "%lu", messageID);
          evalTool->AddAnElementToLine(std::string(buffertemp));
          sprintf(buffertemp, "%llu", ServerTimer->GetTimeStampUint64());
          evalTool->AddAnElementToLine(std::string(buffertemp));
          evalTool->WriteCurrentLineToFile();
        }
      }
    }
    delete[] pYUV;
    pYUV = NULL;
  }
}

void VideoStreamIGTLinkServer::SendCompressedData()
{
  //int frameSize = pSrcPic->iPicWidth* pSrcPic->iPicHeight * 3 >> 1;
  if(this->transportMethod == UseTCP)
  {
    igtl::VideoMessage::Pointer videoMsg;
    videoMsg = igtl::VideoMessage::New();
    videoMsg->SetHeaderVersion(IGTL_HEADER_VERSION_2);
    videoMsg->SetDeviceName(this->deviceName.c_str());
    ServerTimer->SetTime(this->encodeEndTime);
    videoMsg->SetTimeStamp(ServerTimer);
    static igtl_uint32 messageID = 0;
    int frameSize = 0;
    int iLayer = 0;
    if (sFbi.iFrameSizeInBytes > 0)
    {
      videoMsg->SetBitStreamSize(sFbi.iFrameSizeInBytes);
      videoMsg->AllocateBuffer();
      videoMsg->SetScalarType(videoMsg->TYPE_UINT8);
      videoMsg->SetEndian(igtl_is_little_endian()==true?2:1); //little endian is 2 big endian is 1
      videoMsg->SetWidth(pSrcPic->iPicWidth);
      videoMsg->SetHeight(pSrcPic->iPicHeight);
      videoMsg->SetMessageID(messageID);
      messageID ++;
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
    this->glock->Lock();
    if(this->socket)
    {
      this->socket->Send(videoMsg->GetPackPointer(), videoMsg->GetBufferSize());
    }
    this->glock->Unlock();
    char buffertemp[64];
    sprintf(buffertemp, "%lu", messageID);
    evalTool->AddAnElementToLine(std::string(buffertemp));
    sprintf(buffertemp, "%llu", ServerTimer->GetTimeStampUint64());
    evalTool->AddAnElementToLine(std::string(buffertemp));
    evalTool->WriteCurrentLineToFile();
  }
  else if(this->transportMethod == UseUDP)
  {
    int iLayer = 0;
    if (sFbi.iFrameSizeInBytes > 0)
    {
      
      static igtlUint32 messageID = 0;
      while (iLayer < sFbi.iLayerNum) {
        SLayerBSInfo* pLayerBsInfo = &sFbi.sLayerInfo[iLayer];
        if (pLayerBsInfo != NULL) {
          int iLayerSize = 0;
          for (int iNalIdx = 0 ; iNalIdx < pLayerBsInfo->iNalCount; iNalIdx ++)
          {
            int nalLength = pLayerBsInfo->pNalLengthInByte[iNalIdx];
            igtl::VideoMessage::Pointer videoMsg;
            videoMsg = igtl::VideoMessage::New();
            videoMsg->SetHeaderVersion(IGTL_HEADER_VERSION_2);
            videoMsg->SetMessageID(messageID);
            videoMsg->SetBitStreamSize(nalLength);
            ServerTimer->SetTime(this->encodeEndTime);
            videoMsg->SetTimeStamp(ServerTimer);
            videoMsg->AllocateBuffer();
            videoMsg->SetDeviceName(this->deviceName.c_str());
            videoMsg->SetScalarType(videoMsg->TYPE_UINT8);
            videoMsg->SetEndian(igtl_is_little_endian()==true?2:1); //little endian is 2 big endian is 1
            videoMsg->SetWidth(pSrcPic->iPicWidth);
            videoMsg->SetHeight(pSrcPic->iPicHeight);
            
            
            memcpy(videoMsg->GetPackFragmentPointer(2), &pLayerBsInfo->pBsBuf[iLayerSize], nalLength);
            std::cerr<<nalLength<<std::endl;
            
            videoMsg->Pack();
            this->glock->Lock();
            int status = rtpWrapper->WrapMessageAndSend(this->serverUDPSocket, (igtlUint8 *)videoMsg->GetPackPointer(), videoMsg->GetPackSize());
            this->glock->Unlock();
            for (int i = 0; i< rtpWrapper->fragmentNumberList.size(); i++)
            {
              char buffertemp[64];
              sprintf(buffertemp, "%lu", messageID);
              evalTool->AddAnElementToLine(std::string(buffertemp));
              
              if (rtpWrapper->fragmentNumberList[i] == 0XFFFF)
              {
                sprintf(buffertemp, "%i", -1);
              }
              else
              {
                sprintf(buffertemp, "%lu", rtpWrapper->fragmentNumberList[i]);
              }
              evalTool->AddAnElementToLine(std::string(buffertemp));
              
              sprintf(buffertemp, "%llu", this->encodeStartTime);
              evalTool->AddAnElementToLine(std::string(buffertemp));
              
              sprintf(buffertemp, "%llu", this->encodeEndTime);
              evalTool->AddAnElementToLine(std::string(buffertemp));
              
              sprintf(buffertemp, "%llu", rtpWrapper->paketSendTimeStampList[i]);
              evalTool->AddAnElementToLine(std::string(buffertemp));
              
              evalTool->WriteCurrentLineToFile();
            }
            
            iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
            messageID ++;
          }
        }
        ++ iLayer;
      }
    }
  }
  
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
    this->ServerTimer->GetTime();
    this->encodeStartTime = this->ServerTimer->GetTimeStampUint64();
    int iEncFrames = this->pSVCEncoder->EncodeFrame (pSrcPic, &sFbi);
    this->ServerTimer->GetTime();
    this->encodeEndTime = this->ServerTimer->GetTimeStampUint64();
    iTotal += WelsTime()- iStart;
    ++ iFrameIdx;
    if (sFbi.eFrameType == videoFrameTypeSkip) {
      continue;
    }
    
    if (iEncFrames == cmResultSuccess ) {
      SendCompressedData();
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


