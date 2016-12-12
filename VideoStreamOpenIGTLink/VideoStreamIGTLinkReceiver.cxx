/*=========================================================================
 
 Program:   VideoStreamIGTLinkReceiver 
 Modified based on the OpenH264/codec/console/dec/src/h264dec.cpp
 Language:  C++
 
 Copyright (c) Insight Software Consortium. All rights reserved.
 
 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notices for more information.
 
 =========================================================================*/

#include "VideoStreamIGTLinkReceiver.h"

struct ReadSocketAndPush
{
  igtl::MessageRTPWrapper::Pointer wrapper;
  igtl::UDPClientSocket::Pointer clientSocket;
  VideoStreamIGTLinkReceiver* receiver;
};

struct Wrapper
{
  igtl::MessageRTPWrapper::Pointer wrapper;
  VideoStreamIGTLinkReceiver* receiver;
};


void* ThreadFunctionUnWrap(void* ptr)
{
  // Get thread information
  igtl::MultiThreader::ThreadInfo* info =
  static_cast<igtl::MultiThreader::ThreadInfo*>(ptr);
  const char *deviceType = "Video";
  Wrapper parentObj = *(static_cast<Wrapper*>(info->UserData));
  const char * videoDeviceName = parentObj.receiver->deviceName.c_str();
  while(1)
  {
    parentObj.wrapper->UnWrapPaketWithTypeAndName(deviceType, videoDeviceName);
    igtl::Sleep(5);
  }
}

void WriteTimeInfo(unsigned char * UDPPaket, int totMsgLen, VideoStreamIGTLinkReceiver* receiver)
{
  igtl_uint16 fragmentField;
  igtl_uint32 messageID;
  int extendedHeaderLength = sizeof(igtl_extended_header);
  memcpy(&fragmentField, (void*)(UDPPaket + RTP_HEADER_LENGTH+IGTL_HEADER_SIZE+extendedHeaderLength-2),2);
  memcpy(&messageID, (void*)(UDPPaket + RTP_HEADER_LENGTH+IGTL_HEADER_SIZE+extendedHeaderLength-6),4);
  if(igtl_is_little_endian())
  {
    fragmentField = BYTE_SWAP_INT16(fragmentField);
    messageID = BYTE_SWAP_INT32(messageID);
  }
  
  char buffertemp[64];
  sprintf(buffertemp, "%lu", messageID);
  receiver->evalToolPaketThread->AddAnElementToLine(std::string(buffertemp));
  if(fragmentField==0X0000) // fragment doesn't exist
  {
    sprintf(buffertemp, "%d", -1);
    receiver->evalToolPaketThread->AddAnElementToLine(std::string(buffertemp));
  }
  else if(fragmentField==0X8000)// To do, fix the issue when later fragment arrives earlier than the beginning fragment
  {
    sprintf(buffertemp, "%d", 0);
    receiver->evalToolPaketThread->AddAnElementToLine(std::string(buffertemp));
  }
  else if(fragmentField>0XE000)// To do, fix the issue when later fragment arrives earlier than the beginning fragment
  {
    sprintf(buffertemp, "%d", fragmentField - 0XE000 + 1);
    receiver->evalToolPaketThread->AddAnElementToLine(std::string(buffertemp));
  }
  else if(fragmentField>0X8000 && fragmentField<0XE000)// To do, fix the issue when later fragment arrives earlier than the beginning fragment
  {
    sprintf(buffertemp, "%d", fragmentField - 0X8000);
    receiver->evalToolPaketThread->AddAnElementToLine(std::string(buffertemp));
  }
  else
  {
    sprintf(buffertemp, "%d", -2);
    receiver->evalToolPaketThread->AddAnElementToLine(std::string(buffertemp));

  }
  receiver->ReceiverTimerPaketThread->GetTime();
  sprintf(buffertemp, "%llu", receiver->ReceiverTimerPaketThread->GetTimeStampUint64());
  receiver->evalToolPaketThread->AddAnElementToLine(std::string(buffertemp));
  receiver->evalToolPaketThread->WriteCurrentLineToFile();
}


void* ThreadFunctionReadSocket(void* ptr)
{
  // Get thread information
  igtl::MultiThreader::ThreadInfo* info =
  static_cast<igtl::MultiThreader::ThreadInfo*>(ptr);
  
  ReadSocketAndPush parentObj = *(static_cast<ReadSocketAndPush*>(info->UserData));
  unsigned char UDPPaket[RTP_PAYLOAD_LENGTH+RTP_HEADER_LENGTH];
  while(1)
  {
    int totMsgLen = parentObj.clientSocket->ReadSocket(UDPPaket, RTP_PAYLOAD_LENGTH+RTP_HEADER_LENGTH);
    
    WriteTimeInfo(UDPPaket, totMsgLen, parentObj.receiver);
    if (totMsgLen>0)
    {
      parentObj.wrapper->PushDataIntoPaketBuffer(UDPPaket, totMsgLen);
    }
  }
}

VideoStreamIGTLinkReceiver::VideoStreamIGTLinkReceiver(char *argv[])
{
  this->deviceName = "";
  this->augments     = argv[1];
  this->rtpWrapper = igtl::MessageRTPWrapper::New();
  strncpy(this->codecType, "H264",4);
  this->useCompress      = true;
  
  this->interval = 10; //10ms
  
  WelsCreateDecoder (&this->pSVCDecoder);
  memset (&this->decParam, 0, sizeof (SDecodingParam));
  this->decParam.uiTargetDqLayer = UCHAR_MAX;
  this->decParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
  this->decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
  this->pSVCDecoder->Initialize (&decParam);
  this->kpOuputFileName = (char*)"decodedOutput.yuv";
  this->pOptionFileName = NULL;
  this->videoMessageBuffer=NULL;
  this->decodedFrame=NULL;
  socket = igtl::ClientSocket::New();
  UDPSocket = igtl::UDPClientSocket::New();
  this->Height = 0;
  this->Width = 0;
  this->flipAtX = true;
  this->H264DecodeInstance = new H264Decode();
  ReceiverTimerPaketThread = igtl::TimeStamp::New();
  ReceiverTimerDecodeThread = igtl::TimeStamp::New();
  this->evalToolPaketThread = new EvaluationTool();
  this->evalToolDecodeThread = new EvaluationTool();

}

int VideoStreamIGTLinkReceiver::RunOnTCPSocket()
{
  //------------------------------------------------------------
  // Establish Connection
  std::string fileName = std::string(this->deviceName);
  fileName.append("-UseTCP");
  if (useCompress == 0)
  {
    fileName.append("-Original-");
  }
  else
  {
    fileName.append("-Compressed-");
  }
  ReceiverTimerDecodeThread->GetTime();
  char buffertemp[64];
  sprintf(buffertemp, "%llu", ReceiverTimerDecodeThread->GetTimeStampUint64());
  this->evalToolDecodeThread->filename = std::string(fileName).append("DecodeThread-").append(buffertemp);
  this->evalToolPaketThread->filename = std::string(fileName).append("PaketThread-").append(buffertemp);
  std::string headLine = "NAL-Unit Before-Decoding After-Decoding";
  this->evalToolDecodeThread->AddAnElementToLine(headLine);
  this->evalToolDecodeThread->WriteCurrentLineToFile();
  headLine = "NAL-Unit Fragment-Number Fragment-Paket-SendingTime";
  this->evalToolPaketThread->AddAnElementToLine(headLine);
  this->evalToolPaketThread->WriteCurrentLineToFile();
  int r = socket->ConnectToServer(TCPServerIPAddress, TCPServerPort);
  
  if (r != 0)
  {
    std::cerr << "Cannot connect to the server." << std::endl;
    exit(0);
  }
  
  //------------------------------------------------------------
  // Ask the server to start pushing tracking data
  std::cerr << "Sending STT_VIDEO message....." << std::endl;
  igtl::StartVideoDataMessage::Pointer startVideoMsg;
  startVideoMsg = igtl::StartVideoDataMessage::New();
  startVideoMsg->AllocatePack();
  startVideoMsg->SetHeaderVersion(IGTL_HEADER_VERSION_2);
  startVideoMsg->SetDeviceName("MacCamera5");
  startVideoMsg->SetCodecType(codecType);
  startVideoMsg->SetTimeInterval(interval);
  startVideoMsg->SetUseCompress(useCompress);
  startVideoMsg->Pack();
  socket->Send(startVideoMsg->GetPackPointer(), startVideoMsg->GetPackSize());
  std::string outputFileName = "outputDecodedVideoTCP.yuv";
  while (1)
  {
    //------------------------------------------------------------
    // Wait for a reply
    igtl::MessageHeader::Pointer headerMsg;
    headerMsg = igtl::MessageHeader::New();
    headerMsg->InitPack();
    int rs = socket->Receive(headerMsg->GetPackPointer(), headerMsg->GetPackSize());
    if (rs == 0)
    {
      std::cerr << "Connection closed." << std::endl;
      socket->CloseSocket();
      exit(0);
    }
    if (rs != headerMsg->GetPackSize())
    {
      std::cerr << "Message size information and actual data size don't match." << std::endl;
      this->SendStopMessage();
      socket->CloseSocket();
      
      exit(0);
    }
    
    headerMsg->Unpack();
    if (strcmp(headerMsg->GetDeviceName(), this->deviceName.c_str()) == 0)
    {
      //------------------------------------------------------------
      // Allocate Video Message Class
      
      igtl::VideoMessage::Pointer videoMsg;
      videoMsg = igtl::VideoMessage::New();
      videoMsg->SetHeaderVersion(IGTL_HEADER_VERSION_2);
      videoMsg->SetMessageHeader(headerMsg);
      videoMsg->AllocateBuffer();
      // Receive body from the socket
      socket->Receive(videoMsg->GetPackBodyPointer(), videoMsg->GetPackBodySize());
      
      // Deserialize the transform data
      // If you want to skip CRC check, call Unpack() without argument.
      int c = videoMsg->Unpack(1);
      
      if (c==0) // if CRC check fails
      {
        // TODO: error handling
        return 0;
      }
      this->SetWidth(videoMsg->GetWidth());
      this->SetHeight(videoMsg->GetHeight());
      int streamLength = videoMsg->GetPackSize()-IGTL_VIDEO_HEADER_SIZE-IGTL_HEADER_SIZE;
      this->SetStreamLength(streamLength);
      if (!(this->videoMessageBuffer==NULL))
      {
        delete[] this->videoMessageBuffer;
      }
      this->videoMessageBuffer = new uint8_t[streamLength];
      memcpy(this->videoMessageBuffer, videoMsg->GetPackFragmentPointer(2), streamLength);
      ReceiverTimerDecodeThread->GetTime();
      char buffertemp[64];
      sprintf(buffertemp, "%lu", videoMsg->GetMessageID());
      evalToolDecodeThread->AddAnElementToLine(std::string(buffertemp));
      sprintf(buffertemp, "%llu", ReceiverTimerDecodeThread->GetTimeStampUint64());
      evalToolDecodeThread->AddAnElementToLine(std::string(buffertemp));
      if (this->ProcessVideoStream(this->videoMessageBuffer)== 0)
      {
        this->SendStopMessage();
        break;
      }
      ReceiverTimerDecodeThread->GetTime();
      sprintf(buffertemp,"%llu", ReceiverTimerDecodeThread->GetTimeStampUint64());
      evalToolDecodeThread->AddAnElementToLine(std::string(buffertemp));
      evalToolDecodeThread->WriteCurrentLineToFile(); // this is actually the decode time of a single NAL unit
    }
    else
    {
      std::cerr << "Receiving : " << headerMsg->GetDeviceType() << std::endl;
      socket->Skip(headerMsg->GetBodySizeToRead(), 0);
    }
  }
  WelsDestroyDecoder(this->pSVCDecoder);
}

void VideoStreamIGTLinkReceiver::SendStopMessage()
{
  //------------------------------------------------------------
  // Ask the server to stop pushing tracking data
  std::cerr << "Sending STP_VIDEO message....." << std::endl;
  igtl::StopVideoMessage::Pointer stopVideoMsg;
  stopVideoMsg = igtl::StopVideoMessage::New();
  stopVideoMsg->SetHeaderVersion(IGTL_HEADER_VERSION_2);
  stopVideoMsg->SetDeviceName("TDataClient");
  stopVideoMsg->Pack();
  socket->Send(stopVideoMsg->GetPackPointer(), stopVideoMsg->GetPackSize());
}

int VideoStreamIGTLinkReceiver::RunOnUDPSocket()
{
  // Initialize the evaluation file
  std::string fileName = std::string(this->deviceName);
  fileName.append("-UseUDP");
  if (useCompress == 0)
  {
    fileName.append("-Original-");
  }
  else
  {
    fileName.append("-Compressed-");
  }
  ReceiverTimerDecodeThread->GetTime();
  char buffertemp[64];
  sprintf(buffertemp, "%llu", ReceiverTimerDecodeThread->GetTimeStampUint64());
  this->evalToolDecodeThread->filename = std::string(fileName).append("DecodeThread-").append(buffertemp);
  this->evalToolPaketThread->filename = std::string(fileName).append("PaketThread-").append(buffertemp);
  std::string headLine = "NAL-Unit Before-Decoding After-Decoding";
  this->evalToolDecodeThread->AddAnElementToLine(headLine);
  this->evalToolDecodeThread->WriteCurrentLineToFile();
  headLine = "NAL-Unit Fragment-Number Fragment-Paket-SendingTime";
  this->evalToolPaketThread->AddAnElementToLine(headLine);
  this->evalToolPaketThread->WriteCurrentLineToFile();
  //----------------------------
  
  //UDPSocket->JoinNetwork("226.0.0.1", port, 1);
  igtl::ConditionVariable::Pointer conditionVar = igtl::ConditionVariable::New();
  igtl::SimpleMutexLock* glock = igtl::SimpleMutexLock::New();
  //UDPSocket->JoinNetwork("127.0.0.1", port, 0); // join the local network for a client connection
  //std::vector<ReorderBuffer> reorderBufferVec(10, ReorderBuffer();
  //int loop = 0;
  UDPSocket->JoinNetwork("127.0.0.1", UDPClientPort, 0);
  ReadSocketAndPush info;
  info.wrapper = rtpWrapper;
  info.clientSocket = UDPSocket;
  info.receiver = this;
  
  Wrapper infoWrapper;
  infoWrapper.wrapper = rtpWrapper;
  infoWrapper.receiver = this;
  
  igtl::MultiThreader::Pointer threader = igtl::MultiThreader::New();
  threader->SpawnThread((igtl::ThreadFunctionType)&ThreadFunctionReadSocket, &info);
  threader->SpawnThread((igtl::ThreadFunctionType)&ThreadFunctionUnWrap, &infoWrapper);
  while(1)
  {
    if(rtpWrapper->unWrappedMessages.size())// to do: glock this session
    {
      igtl::VideoMessage::Pointer videoMultiPKTMSG = igtl::VideoMessage::New();
      videoMultiPKTMSG->SetHeaderVersion(IGTL_HEADER_VERSION_2);
      glock->Lock();
      std::map<igtl_uint32, igtl::UnWrappedMessage*>::iterator it = rtpWrapper->unWrappedMessages.begin();
      igtlUint8 * message = new igtlUint8[it->second->messageDataLength];
      int MSGLength = it->second->messageDataLength;
      memcpy(message, it->second->messagePackPointer, it->second->messageDataLength);
      rtpWrapper->unWrappedMessages.erase(it);
      //delete it->second;
      glock->Unlock();
      igtl::MessageHeader::Pointer header = igtl::MessageHeader::New();
      header->InitPack();
      memcpy(header->GetPackPointer(), message, IGTL_HEADER_SIZE);
      header->Unpack();
      videoMultiPKTMSG->SetMessageHeader(header);
      videoMultiPKTMSG->AllocateBuffer();
      if (MSGLength == videoMultiPKTMSG->GetPackSize())
      {
        memcpy(videoMultiPKTMSG->GetPackPointer(), message, MSGLength);
        videoMultiPKTMSG->Unpack(1);
        this->SetWidth(videoMultiPKTMSG->GetWidth());
        this->SetHeight(videoMultiPKTMSG->GetHeight());
        int streamLength = videoMultiPKTMSG->GetBitStreamSize();
        this->SetStreamLength(streamLength);
        if (!(this->videoMessageBuffer==NULL))
        {
          delete[] this->videoMessageBuffer;
        }
        this->videoMessageBuffer = new uint8_t[streamLength];
        memcpy(this->videoMessageBuffer, videoMultiPKTMSG->GetPackFragmentPointer(2), streamLength);
        ReceiverTimerDecodeThread->GetTime();
        char buffertemp[64];
        sprintf(buffertemp, "%lu", videoMultiPKTMSG->GetMessageID());
        evalToolDecodeThread->AddAnElementToLine(std::string(buffertemp));
        sprintf(buffertemp, "%llu", ReceiverTimerDecodeThread->GetTimeStampUint64());
        evalToolDecodeThread->AddAnElementToLine(std::string(buffertemp));
        if (this->ProcessVideoStream(this->videoMessageBuffer)== 0) // To do, check if we need to get all the NALs
        {
          break;
        }
        ReceiverTimerDecodeThread->GetTime();
        sprintf(buffertemp, "%llu", ReceiverTimerDecodeThread->GetTimeStampUint64());
        evalToolDecodeThread->AddAnElementToLine(std::string(buffertemp));
        evalToolDecodeThread->WriteCurrentLineToFile(); // this is actually the decode time of a single NAL unit
      }
      delete message;
    }
  }
  return 0;
}

bool VideoStreamIGTLinkReceiver::InitializeClient()
{
  // if configure file exit, reading configure file firstly
  cRdCfg.Openf (this->augments.c_str());// to do get the first augments from this->augments.
  if (cRdCfg.ExistFile())
  {
    cRdCfg.Openf (this->augments.c_str());// reset the file read pointer to the beginning.
    int iRet = ParseConfigForClient();
    if (iRet == -1) {
      fprintf (stderr, "parse client parameter config file failed.\n");
      return false;
    }
    return true;
  }
  else
  {
    fprintf (stderr, "Specified file: %s not exist, maybe invalid path or parameter settting.\n",
             cRdCfg.GetFileName().c_str());
    return false;
  }
}

int VideoStreamIGTLinkReceiver::ParseConfigForClient()
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
      if (strTag[0].compare ("TCPServerPortNumber") == 0) {
        this->TCPServerPort = atoi (strTag[1].c_str());
        if(this->TCPServerPort<0 || this->TCPServerPort>65535)
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
      if (strTag[0].compare ("TCPServerIPAddress") == 0) {
        this->TCPServerIPAddress = new char[IP4AddressStrLen];
        memcpy(this->TCPServerIPAddress, strTag[1].c_str(), IP4AddressStrLen);
        if(inet_addr(this->TCPServerIPAddress))
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
      if (strTag[0].compare ("UDPClientPortNumber") == 0) {
        this->UDPClientPort = atoi (strTag[1].c_str());
        if(this->UDPClientPort<0 || this->UDPClientPort>65535)
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
    if (arttributNum ==20)
    {
      break;
    }
    
  }
  return iRet;
}

void VideoStreamIGTLinkReceiver::SetWidth(int iWidth)
{
  this->Width = iWidth;
}

void VideoStreamIGTLinkReceiver::SetHeight(int iHeight)
{
  this->Height = iHeight;
}

void VideoStreamIGTLinkReceiver::SetStreamLength(int iStreamLength)
{
  this->StreamLength = iStreamLength;
}

void VideoStreamIGTLinkReceiver::SetDecodedFrame()
{
  if (!(this->decodedFrame == NULL))
  {
    delete[] this->decodedFrame;
  }
  this->decodedFrame = NULL;
  this->decodedFrame = new unsigned char[this->Width*this->Height*3>>1];
}

int VideoStreamIGTLinkReceiver::ProcessVideoStream(uint8_t* bitStream)
{
  //std::cerr << "Receiving Video data type." << std::endl;
  //this->videoMessageBuffer = new uint8_t[StreamLength];
  //memcpy(this->videoMessageBuffer, bitStream, StreamLength);// copy slow down the process, however, the videoMsg is a smart pointer, it gets deleted unpredictable.
  
  if (useCompress)
  {
    this->SetDecodedFrame();
    H264DecodeInstance->DecodeSingleFrame(this->pSVCDecoder, bitStream, this->decodedFrame, kpOuputFileName, Width, Height, StreamLength, pOptionFileName);
    if (this->decodedFrame)
    {
      return 1;
    }
    return 0;
  }
  else
  {
    //std::cerr << "No using compression, data size in byte is: " << Width*Height*3/2  <<std::endl;
    FILE* pYuvFile    = NULL;
    pYuvFile = fopen (kpOuputFileName, "ab");
    unsigned char* pData[3];
    int iStride[2] = {Width, Width/2};
    pData[0] = bitStream;
    pData[1] = pData[0] + Width * Height;
    pData[2] = pData[1] + Width * Height/4;
    H264DecodeInstance->Write2File (pYuvFile, pData, iStride, Width, Height);
    fclose (pYuvFile);
    pYuvFile = NULL;
    return 1;
  }
  return 0;
}


int VideoStreamIGTLinkReceiver::YUV420ToRGBConversion(uint8_t *RGBFrame, uint8_t * YUV420Frame, int iHeight, int iWidth)
{
  int componentLength = iHeight*iWidth;
  const uint8_t *srcY = YUV420Frame;
  const uint8_t *srcU = YUV420Frame+componentLength;
  const uint8_t *srcV = YUV420Frame+componentLength*5/4;
  uint8_t * YUV444 = new uint8_t[componentLength * 3];
  uint8_t *dstY = YUV444;
  uint8_t *dstU = dstY + componentLength;
  uint8_t *dstV = dstU + componentLength;
  
  memcpy(dstY, srcY, componentLength);
  const int halfHeight = iHeight/2;
  const int halfWidth = iWidth/2;
  
#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU,iWidth)
  for (int y = 0; y < halfHeight; y++) {
    for (int x = 0; x < halfWidth; x++) {
      dstU[2 * x + 2 * y*iWidth] = dstU[2 * x + 1 + 2 * y*iWidth] = srcU[x + y*iWidth/2];
      dstV[2 * x + 2 * y*iWidth] = dstV[2 * x + 1 + 2 * y*iWidth] = srcV[x + y*iWidth/2];
    }
    memcpy(&dstU[(2 * y + 1)*iWidth], &dstU[(2 * y)*iWidth], iWidth);
    memcpy(&dstV[(2 * y + 1)*iWidth], &dstV[(2 * y)*iWidth], iWidth);
  }
  
  
  const int yOffset = 16;
  const int cZero = 128;
  int yMult, rvMult, guMult, gvMult, buMult;
  yMult =   76309;
  rvMult = 117489;
  guMult = -13975;
  gvMult = -34925;
  buMult = 138438;
  
  static unsigned char clp_buf[384+256+384];
  static unsigned char *clip_buf = clp_buf+384;
  
  // initialize clipping table
  memset(clp_buf, 0, 384);
  
  for (int i = 0; i < 256; i++) {
    clp_buf[384+i] = i;
  }
  memset(clp_buf+384+256, 255, 384);
  
  
#pragma omp parallel for default(none) shared(dstY,dstU,dstV,RGBFrame,yMult,rvMult,guMult,gvMult,buMult,clip_buf,componentLength)// num_threads(2)
  for (int i = 0; i < componentLength; ++i) {
    const int Y_tmp = ((int)dstY[i] - yOffset) * yMult;
    const int U_tmp = (int)dstU[i] - cZero;
    const int V_tmp = (int)dstV[i] - cZero;
    
    const int R_tmp = (Y_tmp                  + V_tmp * rvMult ) >> 16;//32 to 16 bit conversion by left shifting
    const int G_tmp = (Y_tmp + U_tmp * guMult + V_tmp * gvMult ) >> 16;
    const int B_tmp = (Y_tmp + U_tmp * buMult                  ) >> 16;
    
    if (flipAtX)
    {
      int pos = componentLength-i-Width+(i%Width)*2;
      RGBFrame[3*pos] = clip_buf[R_tmp];
      RGBFrame[3*pos+1] = clip_buf[G_tmp];
      RGBFrame[3*pos+2] = clip_buf[B_tmp];
    }
    else
    {
      RGBFrame[3*i]   = clip_buf[R_tmp];
      RGBFrame[3*i+1] = clip_buf[G_tmp];
      RGBFrame[3*i+2] = clip_buf[B_tmp];
    }
  }
  delete [] YUV444;
  YUV444 = NULL;
  dstY = NULL;
  dstU = NULL;
  dstV = NULL;
  return 1;
}

