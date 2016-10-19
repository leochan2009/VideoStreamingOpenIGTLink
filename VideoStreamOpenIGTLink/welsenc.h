/*=========================================================================
 
 Program:   OpenH264 encoder wrapper
 Language:  C++
 
 Copyright (c) Insight Software Consortium. All rights reserved.
 
 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notices for more information.
 
 =========================================================================*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#if defined (ANDROID_NDK)
#include <android/log.h>
#endif
#ifdef ONLY_ENC_FRAMES_NUM
#undef ONLY_ENC_FRAMES_NUM
#endif//ONLY_ENC_FRAMES_NUM
#define ONLY_ENC_FRAMES_NUM INT_MAX // 2, INT_MAX // type the num you try to encode here, 2, 10, etc

#if defined (WINDOWS_PHONE)
float   g_fFPS           = 0.0;
double  g_dEncoderTime   = 0.0;
int     g_iEncodedFrame  = 0;
#endif

#if defined (ANDROID_NDK)
#define LOG_TAG "welsenc"
#define LOGI(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define printf(...) LOGI(__VA_ARGS__)
#define fprintf(a, ...) LOGI(__VA_ARGS__)
#endif
//#define STICK_STREAM_SIZE

#include "measure_time.h"
#include "read_config.h"

#include "typedefs.h"

#ifdef _MSC_VER
#include <io.h>     /* _setmode() */
#include <fcntl.h>  /* _O_BINARY */
#endif//_MSC_VER

#include "codec_def.h"
#include "codec_api.h"
#include "extern.h"
#include "macros.h"
#include "wels_const.h"

#include "mt_defs.h"
#include "WelsThreadLib.h"

#ifdef _WIN32
#ifdef WINAPI_FAMILY
#include <winapifamily.h>
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define HAVE_PROCESS_AFFINITY
#endif
#else /* defined(WINAPI_FAMILY) */
#define HAVE_PROCESS_AFFINITY
#endif
#endif /* _WIN32 */

#if defined(__linux__) || defined(__unix__)
#define _FILE_OFFSET_BITS 64
#endif

#include <iostream>
using namespace std;
using namespace WelsEnc;

/*
 *  Layer Context
 */
typedef struct LayerpEncCtx_s {
  int32_t       iDLayerQp;
  SSliceArgument  sSliceArgument;
} SLayerPEncCtx;

typedef struct tagFilesSet {
  string strBsFile;
  string strSeqFile;    // for cmd lines
  string strLayerCfgFile[MAX_DEPENDENCY_LAYER];
  char   sRecFileName[MAX_DEPENDENCY_LAYER][MAX_FNAME_LEN];
  uint32_t uiFrameToBeCoded;
} SFilesSet;

/* Ctrl-C handler */
static int     g_iCtrlC = 0;
static void    SigIntHandler (int a) {
  g_iCtrlC = 1;
}
static int     g_LevelSetting = WELS_LOG_ERROR;


int ParseLayerConfig (CReadConfig& cRdLayerCfg, const int iLayer, SEncParamExt& pSvcParam, SFilesSet& sFileSet) ;

int ParseConfig (CReadConfig& cRdCfg, SSourcePicture* pSrcPic, SEncParamExt& pSvcParam, SFilesSet& sFileSet);

void PrintHelp();

int ParseCommandLine (int argc, char** argv, SSourcePicture* pSrcPic, SEncParamExt& pSvcParam, SFilesSet& sFileSet);

int FillSpecificParameters (SEncParamExt& sParam);

//int ProcessEncoding (ISVCEncoder* pPtrEnc, int argc, char** argv, bool bConfigFile);

void LockToSingleCore();

int32_t CreateSVCEncHandle (ISVCEncoder** ppEncoder);

void DestroySVCEncHandle (ISVCEncoder* pEncoder);
