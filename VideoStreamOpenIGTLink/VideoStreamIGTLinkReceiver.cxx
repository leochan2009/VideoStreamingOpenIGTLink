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
  this->pOptionFileName = (char*)"";
  socket = igtl::ClientSocket::New();
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
  int loop = 0;
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
      this->ProcessVideoStream(socket, headerMsg);
      loop++;
      if (loop>10)
      {
        this->SendStopMessage();
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

int VideoStreamIGTLinkReceiver::ProcessVideoStream(igtl::ClientSocket::Pointer& socket, igtl::MessageHeader::Pointer& header)
{
  std::cerr << "Receiving Video data type." << std::endl;
  
  //------------------------------------------------------------
  // Allocate Video Message Class
  
  igtl::VideoMessage::Pointer videoMsg;
  videoMsg = igtl::VideoMessage::New();
  videoMsg->SetMessageHeader(header);
  videoMsg->AllocatePack(header->GetBodySizeToRead());
  
  // Receive body from the socket
  socket->Receive(videoMsg->GetPackBodyPointer(), videoMsg->GetPackBodySize());
  
  // Deserialize the transform data
  // If you want to skip CRC check, call Unpack() without argument.
  videoMsg->Unpack();
  
  if (igtl::MessageHeader::UNPACK_BODY)
  {
    int32_t iWidth = videoMsg->GetWidth(), iHeight = videoMsg->GetHeight(), streamLength = videoMsg->GetPackBodySize()- IGTL_VIDEO_HEADER_SIZE;
    if (useCompress)
    {
      this->decodedFrame[0] = NULL;
      this->decodedFrame[1] = NULL;
      this->decodedFrame[2] = NULL;
      H264DecodeInstance(this->pSVCDecoder, videoMsg->GetPackFragmentPointer(2), this->decodedFrame, kpOuputFileName, iWidth, iHeight, streamLength, pOptionFileName);
      if (this->decodedFrame[0])
      {
        return 1;
      }
      return 0;
    }
    else
    {
      std::cerr << "No using compression, data size in byte is: " << iWidth*iHeight*3/2  <<std::endl;
      FILE* pYuvFile    = NULL;
      pYuvFile = fopen (kpOuputFileName, "ab");
      unsigned char* pData[3];
      int iStride[2] = {iWidth, iWidth/2};
      pData[0] = videoMsg->GetPackFragmentPointer(2);
      pData[1] = pData[0] + iWidth * iHeight;
      pData[2] = pData[1] + iWidth * iHeight/4;
      Write2File (pYuvFile, pData, iStride, iWidth, iHeight);
      fclose (pYuvFile);
      pYuvFile = NULL;
      return 1;
    }
  }
  return 0;
}
