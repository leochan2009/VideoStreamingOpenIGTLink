/*=========================================================================

  Program:   Video Streaming server
  Language:  C++

  Copyright (c) Insight Software Consortium. All rights reserved.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "VideoStreamIGTLinkServer.h"

#if defined(ANDROID_NDK) || defined(APPLE_IOS) || defined (WINDOWS_PHONE)
extern "C" int EncMain (int argc, char** argv)
#else
int main (int argc, char** argv)
#endif
{
  VideoStreamIGTLinkServer server(argc, argv);
  server.SetWaitSTTCommand(true);
  server.StartServer();
  while(1)
  {
    if(server.GetServerConnectStatus())
    {
      if (!server.GetInitializationStatus())
      {
        server.InitializeEncoderAndServer();
      }
      server.EncodeFile();
    }
  }
  return 0;
}
