// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

// TCPDeliver (c) 2004 by Klaus Post

#ifndef TCP_Server_h
#define TCP_Server_h


#include "TCPCommon.h"
#include "winsock2.h"
#include "avisynth.h"
#include "resource.h"
#include "TCPCompression.h"


AVSValue __cdecl Create_TCPServer(AVSValue args, void* user_data, IScriptEnvironment* env);

class ClientConnection {
public:
  ClientConnection() {
    status_text = (char*)malloc(512); 
    compression = NULL; 
    isDataPending = NULL;
    reset(); 
  }
  virtual ~ClientConnection() {
    free(status_text);    
    if (compression)
      delete compression;
    if (isDataPending)
      delete[] pendingData;

}

 void setStatus(const char *fmt, ...) {
  va_list val;
  va_start(val, fmt);
  wvsprintf(status_text, fmt, val);
  va_end(val);
}

  SOCKET s;
  bool isConnected;
  bool isDataPending;
  BYTE* pendingData;
  unsigned int pendingBytesSent;
  unsigned int totalPendingBytes;
  TCPCompression* compression;
  char* status_text;

  void reset() {
    if (isDataPending)
      delete[] pendingData;

    s = 0;
    pendingData = 0;
    pendingBytesSent = 0;
    totalPendingBytes = 0;
    isConnected = false;
    isDataPending = false;
    if (compression)
      delete compression;
    compression = new TCPCompression();
    memset(status_text, 0, 512);
  }

};

struct ServerReply {
  BYTE* data;
  unsigned int dataSize;
  BYTE* internal_data;
  ClientConnection* client;

  void allocateBuffer(int bytes) {
    internal_data = new BYTE[bytes+1];
    data = &internal_data[1];
    dataSize = bytes+1;
  }

  void freeBuffer() {
    if (dataSize)
      delete[] internal_data;
    dataSize = 0;
  }

  void setType(BYTE type) {
    internal_data[0] = type;
  }

};

class TCPRecievePacket {
public:
  TCPRecievePacket(SOCKET _s);
  TCPRecievePacket::~TCPRecievePacket();
  int dataSize;
  BYTE* data;
  bool isDisconnected;
private:
  SOCKET s;  
};

  const static int sendbufsize = 262144; // Maximum send size
  const static int rcvbufsize = 1024;   // Smaller rcv size

class TCPServerListener {
public:
  TCPServerListener(int port, PClip child, IScriptEnvironment* env);
  void Listen();
  void KillThread();
  bool thread_running;

private:
  void Receive(TCPRecievePacket* tr, ServerReply* s);
  void AcceptClient(SOCKET s, ClientConnection* s_list);
  void SendPacket(ClientConnection* cc, ServerReply* s);
  void SendPendingData(ClientConnection* cc);
  void SendVideoInfo(ServerReply* s);
  void SendFrameInfo(ServerReply* s, const char* request);
  void SendAudioInfo(ServerReply* s, const char* request);
  void SendParityInfo(ServerReply* s, const char* request);
  void SendVideoFrame(ServerReply* s);
  void SendAudioData(ServerReply* s);
  void CheckClientVersion(ServerReply* s, const char* request);
  void SetStatus(ClientConnection* client, TCHAR *fmt, ...);
  void UpdateStatWindow(DWORD sinceLast);

  WSADATA wsaData;
  SOCKET m_socket;
  sockaddr_in service;
  PClip child;
  IScriptEnvironment* env;
  bool shutdown;
  int prefetch_frame;

  WNDCLASS wc;
  HWND wnd;
  HDC window_hdc;
  __int64 StatImgTransferred;
  __int64 StatImgTransferredUC;
  __int64 StatAudTransferred;
  int StatBytesSinceLast;
  int StatRequested;
  int StatPrerequested;
  int StatClientsTotalConnected;
  int StatClientsConnected;
  int StatFramesLast;
  int StatBytesLast;
  int StatAudBytesLast;
};


class TCPServerConnection {

};

class TCPServer  : public GenericVideoFilter {
public:
  TCPServer(PClip _child, int port, IScriptEnvironment* env);
  ~TCPServer();
private:
  HANDLE ServerThread;
  TCPServerListener* s;
};



#endif