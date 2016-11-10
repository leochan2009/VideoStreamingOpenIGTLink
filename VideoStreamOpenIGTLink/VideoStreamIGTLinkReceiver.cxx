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

VideoStreamIGTLinkReceiver::VideoStreamIGTLinkReceiver()
{
  this->hostname = "localhost";
  this->port     = 18944;
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
  this->Height = 0;
  this->Width = 0;
  this->flipAtX = true;
  this->H264DecodeInstance = new H264Decode();
}

int VideoStreamIGTLinkReceiver::Run()
{
  //------------------------------------------------------------
  // Establish Connection
  
  int r = socket->ConnectToServer(hostname.c_str(), port);
  
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
  startVideoMsg->SetDeviceName("Video Client");
  startVideoMsg->SetCodecType(codecType);
  startVideoMsg->SetTimeInterval(interval);
  startVideoMsg->SetUseCompress(useCompress);
  startVideoMsg->Pack();
  socket->Send(startVideoMsg->GetPackPointer(), startVideoMsg->GetPackSize());
  std::string outputFileName = "outputDecodedVideo.yuv";
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
    if (strcmp(headerMsg->GetDeviceName(), "Video") == 0)
    {
      //------------------------------------------------------------
      // Allocate Video Message Class
      
      igtl::VideoMessage::Pointer videoMsg;
      videoMsg = igtl::VideoMessage::New();
      videoMsg->SetMessageHeader(headerMsg);
      videoMsg->AllocatePack(headerMsg->GetBodySizeToRead());
      
      // Receive body from the socket
      socket->Receive(videoMsg->GetPackBodyPointer(), videoMsg->GetPackBodySize());
      
      // Deserialize the transform data
      // If you want to skip CRC check, call Unpack() without argument.
      int c = videoMsg->Unpack();
      
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
      if (this->ProcessVideoStream(this->videoMessageBuffer)== 0)
      {
        this->SendStopMessage();
        break;
      }
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
  stopVideoMsg->SetDeviceName("TDataClient");
  stopVideoMsg->Pack();
  socket->Send(stopVideoMsg->GetPackPointer(), stopVideoMsg->GetPackSize());
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

