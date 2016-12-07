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
  if (argc != 2) // check number of arguments
  {
    // If not correct, print usage
    std::cerr << "Usage: " << argv[0] << " <configurationfile>"    << std::endl;
    std::cerr << "    <configurationfile> : file name "  << std::endl;
    exit(0);
  }
  VideoStreamIGTLinkServer server(argc, argv);
  server.InitializeEncoderAndServer();
  server.SetWaitSTTCommand(false);
  server.StartServer();
  while(1)
  {
    if(server.transportMethod==0)
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
    else if (server.transportMethod==1)
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
