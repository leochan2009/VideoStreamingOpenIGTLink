#include "H264Decoder.h"

H264Decode::H264Decode()
{}

void H264Decode::Write2File (FILE* pFp, unsigned char* pData[3], int iStride[2], int iWidth, int iHeight) {
  int   i;
  unsigned char*  pPtr = NULL;
  
  pPtr = pData[0];
  for (i = 0; i < iHeight; i++) {
    fwrite (pPtr, 1, iWidth, pFp);
    pPtr += iStride[0];
  }
  
  iHeight = iHeight / 2;
  iWidth = iWidth / 2;
  pPtr = pData[1];
  for (i = 0; i < iHeight; i++) {
    fwrite (pPtr, 1, iWidth, pFp);
    pPtr += iStride[1];
  }
  
  pPtr = pData[2];
  for (i = 0; i < iHeight; i++) {
    fwrite (pPtr, 1, iWidth, pFp);
    pPtr += iStride[1];
  }
}


int H264Decode::Process (void* pDst[3], SBufferInfo* pInfo, FILE* pFp) {
  
  int iRet = 0;
  
  if (pFp && pDst[0] && pDst[1] && pDst[2] && pInfo) {
    int iStride[2];
    int iWidth = pInfo->UsrData.sSystemBuffer.iWidth;
    int iHeight = pInfo->UsrData.sSystemBuffer.iHeight;
    iStride[0] = pInfo->UsrData.sSystemBuffer.iStride[0];
    iStride[1] = pInfo->UsrData.sSystemBuffer.iStride[1];
    
    Write2File (pFp, (unsigned char**)pDst, iStride, iWidth, iHeight);
  }
  return iRet;
}

int64_t getCurrentTime()
{
#if defined(_WIN32)
  SYSTEMTIME sysTime = {0};
  GetLocalTime(&sysTime);
  return sysTime.wMilliseconds;
#else
  struct timeval tv_date;
  gettimeofday(&tv_date, NULL);
  return ((int64_t)tv_date.tv_sec * 1000000 + (int64_t)tv_date.tv_usec);
#endif
  
  
}

int32_t iFrameCountTotal = 0;



void H264Decode::ComposeByteSteam(uint8_t** inputData, SBufferInfo bufInfo, uint8_t *outputByteStream,  int iWidth, int iHeight)
{
  int iStride [2] = {bufInfo.UsrData.sSystemBuffer.iStride[0],bufInfo.UsrData.sSystemBuffer.iStride[1]};
#pragma omp parallel for default(none) shared(outputByteStream,inputData, iStride, iHeight, iWidth)
  for (int i = 0; i < iHeight; i++)
  {
    uint8_t* pPtr = inputData[0]+i*iStride[0];
    for (int j = 0; j < iWidth; j++)
    {
      outputByteStream[i*iWidth + j] = pPtr[j];
    }
  }
#pragma omp parallel for default(none) shared(outputByteStream,inputData, iStride, iHeight, iWidth)
  for (int i = 0; i < iHeight/2; i++)
  {
    uint8_t* pPtr = inputData[1]+i*iStride[1];
    for (int j = 0; j < iWidth/2; j++)
    {
      outputByteStream[i*iWidth/2 + j + iHeight*iWidth] = pPtr[j];
    }
  }
#pragma omp parallel for default(none) shared(outputByteStream, inputData, iStride, iHeight, iWidth)
  for (int i = 0; i < iHeight/2; i++)
  {
    uint8_t* pPtr = inputData[2]+i*iStride[1];
    for (int j = 0; j < iWidth/2; j++)
    {
      outputByteStream[i*iWidth/2 + j + iHeight*iWidth*5/4] = pPtr[j];
    }
  }
  
}

void H264Decode::DecodeSingleFrame (ISVCDecoder* pDecoder, unsigned char* kpH264BitStream,uint8_t* outputByteStream, const char* kpOuputFileName,
                         int32_t& iWidth, int32_t& iHeight, int32_t& iStreamSize, const char* pOptionFileName) {
  
  
  unsigned long long uiTimeStamp = 0;
  int64_t iStart = 0, iEnd = 0, iTotal = 0;
  int32_t iSliceSize;
  int32_t iSliceIndex = 0;
  unsigned char* pBuf = NULL;
  unsigned char uiStartCode[4] = {0, 0, 0, 1};
  
  unsigned char* pData[3] = {NULL};
  //unsigned char* pDst[3] = {NULL};
  SBufferInfo sDstBufInfo;
  
  int32_t iBufPos = 0;
  int32_t i = 0;
  int32_t iLastWidth = 0, iLastHeight = 0;
  int32_t iFrameCount = 0;
  int32_t iEndOfStreamFlag = 0;
  //for coverage test purpose
  int32_t iErrorConMethod = (int32_t) ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
  pDecoder->SetOption (DECODER_OPTION_ERROR_CON_IDC, &iErrorConMethod);
  //~end for
  double dElapsed = 0;
  
  if (pDecoder == NULL) return;
  
  
  FILE* pYuvFile    = NULL;
  FILE* pOptionFile = NULL;
  // Lenght input mode support
  /*if (kpOuputFileName) {
    pYuvFile = fopen (kpOuputFileName, "ab");
    if (pYuvFile == NULL) {
      fprintf (stderr, "Can not open yuv file to output result of decoding..\n");
      // any options
      //return; // can let decoder work in quiet mode, no writing any output
    } //else
    //fprintf (stderr, "Sequence output file name: %s..\n", kpOuputFileName);
  } else {
    fprintf (stderr, "Can not find any output file to write..\n");
    // any options
  }
  
  if (pOptionFileName) {
    pOptionFile = fopen (pOptionFileName, "wb");
    if (pOptionFile == NULL) {
      fprintf (stderr, "Can not open optional file for write..\n");
    } else
      fprintf (stderr, "Extra optional file: %s..\n", pOptionFileName);
  }*/
  
  //printf ("------------------------------------------------------\n");
  
  if (iStreamSize <= 0) {
    //fprintf (stderr, "Current Bit Stream File is too small, read error!!!!\n");
    goto label_exit;
  }
  pBuf = new unsigned char[iStreamSize + 5];
  if (pBuf == NULL) {
    //fprintf (stderr, "new buffer failed!\n");
    goto label_exit;
  }
  memcpy (pBuf, kpH264BitStream, iStreamSize);
  memcpy (pBuf + iStreamSize, &uiStartCode[0], 4); //confirmed_safe_unsafe_usage
  
  while (true) {
    if (iBufPos >= iStreamSize) {
      iEndOfStreamFlag = true;
      if (iEndOfStreamFlag)
        pDecoder->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
      break;
    }
    for (i = 0; i < iStreamSize; i++) {
      if ((pBuf[iBufPos + i] == 0 && pBuf[iBufPos + i + 1] == 0 && pBuf[iBufPos + i + 2] == 0 && pBuf[iBufPos + i + 3] == 1
           && i > 0) || (pBuf[iBufPos + i] == 0 && pBuf[iBufPos + i + 1] == 0 && pBuf[iBufPos + i + 2] == 1 && i > 0)) {
        break;
      }
    }
    iSliceSize = i;
    if (iSliceSize < 4) { //too small size, no effective data, ignore
      iBufPos += iSliceSize;
      continue;
    }
    
    iStart = getCurrentTime();
    pData[0] = NULL;
    pData[1] = NULL;
    pData[2] = NULL;
    uiTimeStamp ++;
    memset (&sDstBufInfo, 0, sizeof (SBufferInfo));
    sDstBufInfo.uiInBsTimeStamp = uiTimeStamp;
    sDstBufInfo.UsrData.sSystemBuffer.iWidth =
#ifndef NO_DELAY_DECODING
    pDecoder->DecodeFrameNoDelay (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);
#else
    pDecoder->DecodeFrame2 (pBuf + iBufPos, iSliceSize, pData, &sDstBufInfo);
#endif
    
    iEnd    = getCurrentTime();
    iTotal  = iEnd - iStart;
    if (sDstBufInfo.iBufferStatus == 1)
    {
      iWidth  = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
      iHeight = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
      
      if (pOptionFile != NULL) {
        if (iWidth != iLastWidth && iHeight != iLastHeight) {
          fwrite (&iFrameCount, sizeof (iFrameCount), 1, pOptionFile);
          fwrite (&iWidth , sizeof (iWidth) , 1, pOptionFile);
          fwrite (&iHeight, sizeof (iHeight), 1, pOptionFile);
          iLastWidth  = iWidth;
          iLastHeight = iHeight;
        }
      }
      ++ iFrameCount;
    }
    
#ifdef NO_DELAY_DECODING
    iStart = getCurrentTime();
    pData[0] = NULL;
    pData[1] = NULL;
    pData[2] = NULL;
    memset (&sDstBufInfo, 0, sizeof (SBufferInfo));
    sDstBufInfo.uiInBsTimeStamp = uiTimeStamp;
    pDecoder->DecodeFrame2 (NULL, 0, pData, &sDstBufInfo);
    iEnd    = getCurrentTime();
    iTotal = iEnd - iStart;
    if (sDstBufInfo.iBufferStatus == 1) {
      ComposeByteSteam(pData, sDstBufInfo, outputByteStream, iWidth,iHeight);
      iWidth  = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
      iHeight = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
      std::vector<unsigned char> test(iWidth*iHeight*3/2,0);
      if (pOptionFile != NULL) {
        if (iWidth != iLastWidth && iHeight != iLastHeight) {
          fwrite (&iFrameCount, sizeof (iFrameCount), 1, pOptionFile);
          fwrite (&iWidth , sizeof (iWidth) , 1, pOptionFile);
          fwrite (&iHeight, sizeof (iHeight), 1, pOptionFile);
          iLastWidth  = iWidth;
          iLastHeight = iHeight;
        }
      }
      ++ iFrameCount;
    }
#endif
    if (iFrameCount)
    {
      dElapsed = iTotal / 1e6;
      //fprintf (stderr, "-------------------------------------------------------\n");
       //fprintf (stderr, "iWidth:\t\t%d\nheight:\t\t%d\nFrames:\t\t%d\ndecode time:\t%f sec\nFPS:\t\t%f fps\n",
       //iWidth, iHeight, ++iFrameCountTotal, dElapsed, (iFrameCount * 1.0) / dElapsed);
       //fprintf (stderr, "-------------------------------------------------------\n");
    }
    iBufPos += iSliceSize;
    ++ iSliceIndex;
  }
  // coverity scan uninitial
label_exit:
  if (pBuf) {
    delete[] pBuf;
    pBuf = NULL;
  }
  if (pYuvFile) {
    fclose (pYuvFile);
    pYuvFile = NULL;
  }
  if (pOptionFile) {
    fclose (pOptionFile);
    pOptionFile = NULL;
  }
}
