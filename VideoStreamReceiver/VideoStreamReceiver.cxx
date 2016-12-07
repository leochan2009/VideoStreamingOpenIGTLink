/*=========================================================================
 
 Program:   Video Streaming Receiver
 Language:  C++
 
 Copyright (c) Insight Software Consortium. All rights reserved.
 
 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notices for more information.
 
 =========================================================================*/

#include "VideoStreamIGTLinkReceiver.h"

#if defined(ANDROID_NDK) || defined(APPLE_IOS) || defined (WINDOWS_PHONE)
extern "C" int EncMain (int argc, char** argv)
#else
int main (int argc, char** argv)
#endif
{
  //char * cfgFileName = argv[1];
  VideoStreamIGTLinkReceiver receiver= VideoStreamIGTLinkReceiver();
  receiver.RunOnUDPSocket();
  return 0;
}
