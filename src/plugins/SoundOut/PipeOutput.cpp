// SoundOut Copyright Klaus Post 2006-2007
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

// SoundOut (c) 2006-2007 by Klaus Post

#include "PipeOutput.h"
#include <fcntl.h> 
#include <io.h>
#include <vfw.h>
#include "Commdlg.h"

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif

PipeOutput* out;

const char * const PIPE_TypeString[] = {
  "Microsoft WAV", "WAV with WAVEFORMATEX", "RAW PCM", "S.F. WAVE64" };

const char * const PIPE_FormatString[] = {
  "16 Bit", "24 Bit", "32 Bit", "Float"};

const int PIPE_FormatVal[] = {
  SAMPLE_INT16, SAMPLE_INT24, SAMPLE_INT32, SAMPLE_FLOAT };

BOOL CALLBACK PipeDialogProc(
  HWND hwndDlg,  // handle to dialog box
  UINT uMsg,     // message
  WPARAM wParam, // first message parameter
  LPARAM lParam  // second message parameter
  )
{
	switch (uMsg) 
	{
		case WM_COMMAND:
			switch(wParam) {
				case IDCANCEL:
          delete out;
          return true;
        case IDC_PIPESAVE:
          out->startEncoding();
          return true;
        case IDC_PIPECANCEL:
          delete out;
          return true;
        case IDC_PIPEBROWSE: 
          {
            char szFile[MAX_PATH+1];
	          szFile[0] = 0;
            char CurrentDir[] = "";
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(ofn));
            ofn.hInstance = g_hInst;
            ofn.lpstrTitle = "Select Executable..";
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Executable Files (*.EXE)\0*.EXE\0All Files (*.*)\0*.*\0\0";
            ofn.lpstrInitialDir = CurrentDir;
            ofn.Flags = OFN_EXPLORER;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof( szFile );

            if (!GetOpenFileName(&ofn))
              return true;
            out->params["executable"] = AVSValue(szFile);
            out->setParamsToGUI();
          }
        case IDC_PIPENOFILENAME:
          if (out->GUI_ready) {
            out->getParamsFromGUI();
            out->setParamsToGUI();
          }
      }
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}


PipeOutput::PipeOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_INT16|SAMPLE_INT32|SAMPLE_FLOAT,SAMPLE_INT32),_env)
{
  GUI_ready = false;
  params["type"] = AVSValue(0);
  params["peakchunck"] = AVSValue(false);
  params["filterID"] = AVSValue("pipeout");
  params["executable"] = AVSValue("aften.exe");
  params["prefilename"] = AVSValue("-b 384 -");
  params["postfilename"] = AVSValue("");
  params["showoutput"] = AVSValue(true);
  params["outputFileFilter"] = AVSValue("All Files (*.*)\0*.*\0\0");
  params["extension"] = AVSValue("");
  params["filterID"] = AVSValue("pipe");
  params["nofilename"] = AVSValue(false);
  params["format"] = AVSValue(0);

  for (int i = 0; i< 4; i++) {
     if (vi.IsSampleType(PIPE_FormatVal[i]))
       params["format"] = AVSValue(i);
  }
  hProcess = 0;
}

PipeOutput::~PipeOutput(void)
{
  if (hConsole)
    DestroyWindow(hConsole);
  hConsole = 0;

  if (hProcess)
    CloseHandle(hProcess);
  hProcess = 0;
}

void PipeOutput::showGUI() {
  out = this;
  inputThread = 0;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_PIPESETTINGS),0,PipeDialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
  for (int i = 0; i< 4; i++) {
     SendDlgItemMessage(wnd, IDC_PIPEFORMAT, CB_ADDSTRING, 0, (LPARAM)PIPE_FormatString[i]);
     if (vi.IsSampleType(PIPE_FormatVal[i]))
       params["format"] = AVSValue(i);
  }
  for (int i = 0; i< 4; i++) {
     SendDlgItemMessage(wnd, IDC_PIPETYPE, CB_ADDSTRING, 0, (LPARAM)PIPE_TypeString[i]);
  }
	ShowWindow(wnd,SW_NORMAL);
}

bool PipeOutput::setParamsToGUI() {
  SetDlgItemText(wnd,IDC_PIPEEXECUTABLE,params["executable"].AsString());
  SetDlgItemText(wnd,IDC_PIPEPREFILE,params["prefilename"].AsString());
  SetDlgItemText(wnd,IDC_PIPEPOSTFILE,params["postfilename"].AsString());
  CheckDlgButton(wnd, IDC_PIPESHOWOUTPUT, params["showoutput"].AsBool());
  CheckDlgButton(wnd, IDC_PIPENOFILENAME, params["nofilename"].AsBool());
  SendDlgItemMessage(wnd, IDC_PIPETYPE,CB_SETCURSEL,params["type"].AsInt(),0);
  SendDlgItemMessage(wnd, IDC_PIPEFORMAT,CB_SETCURSEL,params["format"].AsInt(),0);
  EnableWindow(GetDlgItem(wnd, IDC_PIPEPOSTFILE), !params["nofilename"].AsBool());
  GUI_ready = true;
  return true;
}

bool PipeOutput::getParamsFromGUI() {
  char buf[10000];
  ((short*)buf)[0] = 10000;
  SendDlgItemMessage(wnd, IDC_PIPEEXECUTABLE, EM_GETLINE,0,(LPARAM)buf);
  params["executable"] = AVSValue(env->SaveString(buf));
  ((short*)buf)[0] = 10000;
  SendDlgItemMessage(wnd, IDC_PIPEPREFILE, EM_GETLINE,0,(LPARAM)buf);
  params["prefilename"] = AVSValue(env->SaveString(buf));
  ((short*)buf)[0] = 10000;
  SendDlgItemMessage(wnd, IDC_PIPEPOSTFILE, EM_GETLINE,0,(LPARAM)buf);
  params["postfilename"] = AVSValue(env->SaveString(buf));
  params["showoutput"] = AVSValue(!!IsDlgButtonChecked(wnd,IDC_PIPESHOWOUTPUT));
  params["nofilename"] = AVSValue(!!IsDlgButtonChecked(wnd,IDC_PIPENOFILENAME));
  params["type"] = AVSValue((int)SendDlgItemMessage(wnd, IDC_PIPETYPE, CB_GETCURSEL,0,0));
  params["format"] = AVSValue((int)SendDlgItemMessage(wnd, IDC_PIPEFORMAT, CB_GETCURSEL,0,0));

  return true;
}
#define ErrorExit(x) {MessageBox(NULL,x,"Commandline Encoder",MB_OK); \
    return false; }

DWORD WINAPI StartInputPiper(LPVOID p) {
  PipeOutput* t = (PipeOutput*)p;
  t->fetchResults();
  return 0;
}

bool PipeOutput::initEncoder() {
  exitThread = false;
  int format = PIPE_FormatVal[params["format"].AsInt()];
  if (!vi.IsSampleType(format)) {
    IScriptEnvironment* env = input->env;

    if (input)
      delete input;

    input = new SampleFetcher(ConvertAudio::Create(child,format,format), env);
    vi = input->child->GetVideoInfo();    
  }
  const char* cmd = params["executable"].AsString();
  HANDLE hInputFile = CreateFile(cmd, GENERIC_READ, FILE_SHARE_READ, NULL, 
      OPEN_EXISTING, 0, NULL); 
  char* executable = 0;

  if (hInputFile == INVALID_HANDLE_VALUE)  {
    // Try in plugindir/Soundout
    try {
      CloseHandle(hInputFile);
      const char* plugin_dir = env->GetVar("$PluginDir$").AsString();
      executable = (char*)malloc(5000);
		  sprintf_s(executable, 5000, "%s\\SoundOut\\%s",plugin_dir, cmd);
      hInputFile = CreateFile(executable, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); 
      if (hInputFile == INVALID_HANDLE_VALUE)  {
        MessageBox(NULL,executable,"Couldn't Find Executable",MB_OK);
        free(executable);
        executable = 0;
        return false;
      }
    } catch (...) {executable = 0;}

  } else {
    executable = _strdup(params["executable"].AsString());
  }

  if (!executable)
    ErrorExit("Could not find executable.\n");
   
  CloseHandle(hInputFile);

  // Create pipe
  SECURITY_ATTRIBUTES saAttr; 

// Set the bInheritHandle flag so pipe handles are inherited. 
 
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
  saAttr.bInheritHandle = TRUE; 
  saAttr.lpSecurityDescriptor = NULL; 

 
// Create a pipes for the child process's STDOUT and STDERR. 

   if (! CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) 
      ErrorExit("Stdout pipe creation failed\n"); 

   if (! CreatePipe(&hChildStderrRd, &hChildStderrWr, &saAttr, 0)) 
      ErrorExit("Stderr pipe creation failed\n"); 

   if (! CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) 
      ErrorExit("Stdin pipe creation failed\n"); 

// Ensure the read handles to the pipes are not inherited.
   SetHandleInformation( hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);
   SetHandleInformation( hChildStderrRd, HANDLE_FLAG_INHERIT, 0);
   SetHandleInformation( hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

  //hChildStdinWr = CreateFile("debug_out.wav", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); 
 
// Now create the child process. 
   
   PROCESS_INFORMATION piProcInfo; 
   STARTUPINFO siStartInfo;
   BOOL bFuncRetn = FALSE; 
 
// Set up members of the PROCESS_INFORMATION structure. 
 
   ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
 
// Set up members of the STARTUPINFO structure. 
 
   ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
   siStartInfo.cb = sizeof(STARTUPINFO); 
   siStartInfo.hStdError = hChildStderrWr;
   siStartInfo.hStdOutput = hChildStdoutWr;
   siStartInfo.hStdInput = hChildStdinRd;
   siStartInfo.wShowWindow = SW_HIDE;
   siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
   siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;


// Create the child process. 
   char szCmdline[32000];
   if (!params["nofilename"].AsBool()) {
    sprintf_s(szCmdline, 32000, "\"%s\" %s \"%s\" %s", executable, 
       params["prefilename"].AsString(), outputFile, params["postfilename"].AsString());
   } else {
    sprintf_s(szCmdline, 32000, "\"%s\" %s", executable, 
     params["prefilename"].AsString());
   }

   bFuncRetn = CreateProcess(NULL, 
      szCmdline,     // command line 
      NULL,          // process security attributes 
      NULL,          // primary thread security attributes 
      TRUE,          // handles are inherited 
      0,             // creation flags 
      NULL,          // use parent's environment 
      NULL,          // use parent's current directory 
      &siStartInfo,  // STARTUPINFO pointer 
      &piProcInfo);  // receives PROCESS_INFORMATION 
   
   if (bFuncRetn == 0) 
      ErrorExit("CreateProcess failed\n");
  sprintf_s(screenBuf, 32000, "Command Line:%s\r\n", szCmdline);
  hProcess = piProcInfo.hProcess;
  CloseHandle(piProcInfo.hThread);

   if (params["showoutput"].AsBool()) {
	  hConsole=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_COMMANDOUTPUT),0,PipeDialogProc);
    SendMessage(hConsole,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
   }

  inputThread = CreateThread(NULL,NULL,StartInputPiper,this, NULL,NULL);
  SetThreadPriority(inputThread,THREAD_PRIORITY_ABOVE_NORMAL);

  free(executable);
  if (params["type"].AsInt() == 2)      // RAW mode - don't write header.
    return true;

  bool bWriteWAVE64 = params["type"].AsInt() == 3;
  bool UseWaveExtensible = params["type"].AsInt() == 1;
  int audioFmtLen = 0;

  int headerSize;
  if (bWriteWAVE64)
    headerSize = 64;
  else 
    headerSize  = 20;

  WAVEFORMATEXTENSIBLE wfxt;
  WAVEFORMATEX wfx;

	if (UseWaveExtensible) {  // Use WAVE_FORMAT_EXTENSIBLE audio output format 
	  const GUID KSDATAFORMAT_SUBTYPE_PCM       = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
	  const GUID KSDATAFORMAT_SUBTYPE_IEEE_FLOAT= {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

	  memset(&wfxt, 0, sizeof(wfxt));
	  wfxt.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	  wfxt.Format.nChannels = vi.AudioChannels();
	  wfxt.Format.nSamplesPerSec = vi.SamplesPerSecond();
	  wfxt.Format.wBitsPerSample = vi.BytesPerChannelSample() * 8;
	  wfxt.Format.nBlockAlign = vi.BytesPerAudioSample();
	  wfxt.Format.nAvgBytesPerSec = wfxt.Format.nSamplesPerSec * wfxt.Format.nBlockAlign;
	  wfxt.Format.cbSize = sizeof(wfxt) - sizeof(wfxt.Format);
	  wfxt.Samples.wValidBitsPerSample = wfxt.Format.wBitsPerSample;
	  wfxt.dwChannelMask = 0;  // Default
    switch (vi.AudioChannels()) {
        case 1 :	/* center channel mono */
					wfxt.dwChannelMask = SPEAKER_FRONT_CENTER;
					break ;

				case 2 :	/* front left and right */
          wfxt.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
					break ;

				case 3 :	/* front left and right */
          wfxt.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT;
					break ;

				case 4 :	/* Quad */
					wfxt.dwChannelMask = SPEAKER_FRONT_LEFT |SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT;
					break ;

				case 5 :	/* 3/2 */
          wfxt.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT;
					break ;

				case 6 :	/* 5.1 */
          wfxt.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY;
					break ;

				case 8 :	/* 7.1 */
          wfxt.dwChannelMask = SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER |SPEAKER_FRONT_LEFT |SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT | SPEAKER_LOW_FREQUENCY;
          break ;
		} 	
	  wfxt.SubFormat = vi.IsSampleType(SAMPLE_FLOAT) ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
    audioFmtLen = sizeof(wfxt);
  }	else {
	  memset(&wfx, 0, sizeof(wfx));
	  wfx.wFormatTag = vi.IsSampleType(SAMPLE_FLOAT) ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
	  wfx.nChannels = vi.AudioChannels();
	  wfx.nSamplesPerSec = vi.SamplesPerSecond();
	  wfx.wBitsPerSample = vi.BytesPerChannelSample() * 8;
	  wfx.nBlockAlign = vi.BytesPerAudioSample();
	  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    audioFmtLen = sizeof(wfx);
	}
  headerSize += audioFmtLen;

	unsigned int dwHeader[22];
  if (bWriteWAVE64) {
	  static const unsigned char kGuidRIFF[16]={0x72, 0x69, 0x66, 0x66, 0x2E, 0x91, 0xCF, 0x11, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00};
	  static const unsigned char kGuidWAVE[16]={0x77, 0x61, 0x76, 0x65, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};
	  static const unsigned char kGuidfmt[16]={0x66, 0x6D, 0x74, 0x20, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};

		memcpy(dwHeader + 0, kGuidRIFF, 16);
	  *(__int64 *)&dwHeader[4] = vi.BytesFromAudioSamples(vi.num_audio_samples) + headerSize;

		memcpy(dwHeader + 6, kGuidWAVE, 16);
		memcpy(dwHeader + 10, kGuidfmt, 16);
		dwHeader[14] = audioFmtLen + 24;
		dwHeader[15] = 0;
    writeSamples(dwHeader, 64);

    if (UseWaveExtensible) 
      writeSamples(&wfxt, audioFmtLen);
    else
      writeSamples(&wfx, audioFmtLen);


		if (dwHeader[14] & 7) {
      char padding[8];
      memset(padding,0,8);
			unsigned int pad = -(signed int)audioFmtLen & 7;
			writeSamples(padding, pad);
		}

	  static const unsigned char kGuiddata[16]={0x64, 0x61, 0x74, 0x61, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};

		memcpy(dwHeader + 0, kGuiddata, 16);
		*(__int64 *)&dwHeader[4] = vi.BytesFromAudioSamples(vi.num_audio_samples);
    writeSamples(dwHeader, 24);

  } else {
    dwHeader[0]	= FOURCC_RIFF;
    dwHeader[1] = (unsigned int)(vi.BytesFromAudioSamples(vi.num_audio_samples) + headerSize);
	  dwHeader[2] = mmioFOURCC('W', 'A', 'V', 'E');
	  dwHeader[3] = mmioFOURCC('f', 'm', 't', ' ');
	  dwHeader[4] = audioFmtLen;
    writeSamples(dwHeader, 20);

    if (UseWaveExtensible) 
      writeSamples(&wfxt, audioFmtLen);
    else
      writeSamples(&wfx, audioFmtLen);

    int zero = 0;
		if (audioFmtLen & 1)
      writeSamples(&zero, 1);

    dwHeader[0] = mmioFOURCC('d', 'a', 't', 'a');
	  dwHeader[1] = (unsigned int)vi.BytesFromAudioSamples(vi.num_audio_samples);
    writeSamples(dwHeader, 8);
  }

  return true;
}

void PipeOutput::fetchResults() {
  CHAR chBuf[128]; 
  // Read output from the child process
  DWORD nBytesRead;
  while(TRUE)
  {
    DWORD exitCode;
    GetExitCodeProcess(hProcess, &exitCode);
    if (exitCode != STILL_ACTIVE || exitThread) {
      exitThread = true; 
      break;;
    }
    if (!ReadFile(hChildStderrRd,chBuf,sizeof(chBuf)-1,
      &nBytesRead,NULL) || !nBytesRead)
    {
      if (GetLastError() == ERROR_BROKEN_PIPE) {
        strcat_s(screenBuf, 32000, "\r\n>PIPE: Input pipe thread killed (EOF)\r\n");
        _RPT0(0,"Input pipe thread killed (EOF)\n");
        break; // pipe done - normal exit path.
      } else {
        strcat_s(screenBuf, 32000, "\r\n>PIPE: Input pipe thread killed (Error)\r\n");
        _RPT0(0,"Input pipe thread killed (Error)\n");
        break;
      }
    }
    chBuf[nBytesRead] = 0;
    _RPT1(0,"%s", chBuf);
    if (params["showoutput"].AsBool()) {
      int cursor = (int)strlen(screenBuf);  // This is the next char to be written.
      char *p;
      for (int i= 0; i< (int)nBytesRead; i++) {
        if (chBuf[i] == '\r' && chBuf[i+1] != '\n' && (cursor>1)) {
          screenBuf[cursor] = 0;  // End the string
          SetDlgItemText(hConsole,IDC_PIPETEXT,screenBuf);
          p = strrchr(screenBuf, '\n');
          if (p) {
            cursor = (int)(p - screenBuf) + 1;
          }
        } else {
          if (cursor >= 32000) {  // Truncate text
            memmove(screenBuf,&screenBuf[1000], cursor - 1000);
            cursor -= 1000;
          }
          screenBuf[cursor] = 0; // End string
          if (chBuf[i] == '\n') {
            SetDlgItemText(hConsole,IDC_PIPETEXT,screenBuf);
          }
          screenBuf[cursor++] = chBuf[i];
        }
      }
      screenBuf[cursor] = 0;  // End string
    }
  }
  SetDlgItemText(hConsole,IDC_PIPETEXT,screenBuf);
}

void PipeOutput::encodeLoop() {
  __int64 encodedSamples = 0;
  SampleBlock* sb;
  do {
    sb = input->GetNextBlock();
    writeSamples(sb->getSamples(), (int)vi.BytesFromAudioSamples(sb->numSamples));
    encodedSamples += sb->numSamples;
    this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);
  } while (!sb->lastBlock && !exitThread);

// Close the pipe handle so the child process stops reading. 
 
   if (! CloseHandle(hChildStdinWr) && !quietExit)
      MessageBox(NULL,"An encoder error occured while closing client output pipe. Output file may not work","Commandline PIPE Encoder",MB_OK);

   if (! CloseHandle(hChildStdoutWr) && !quietExit) 
      MessageBox(NULL,"An encoder error occured while closing output pipe. Output file may not work","Commandline PIPE Encoder",MB_OK);

   if (! CloseHandle(hChildStderrWr) && !quietExit) 
      MessageBox(NULL,"An encoder error occured while closing error output pipe. Output file may not work","Commandline PIPE Encoder",MB_OK);

   if (!sb->lastBlock && !quietExit) 
     this->setError("Encoding aborted before all samples were delivered (not last block).\n");
   if (encodedSamples != vi.num_audio_samples && !quietExit) 
     this->setError("Encoding aborted before all samples were delivered (mismatch).\n");
  this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);
  encodeThread = 0;
}

void PipeOutput::writeSamples(const void *ptr, int count) {
  DWORD nBytesWrote;
  DWORD exitCode;
  GetExitCodeProcess(hProcess, &exitCode);
  if (exitCode != STILL_ACTIVE || exitThread) {
      exitThread = true; 
      return;
  }
  if (!WriteFile(hChildStdinWr,ptr,count,&nBytesWrote,NULL)) {
    if (GetLastError() == ERROR_NO_DATA)
      exitThread = true; // Pipe was closed (normal exit path).
    else {
//      MessageBox(NULL,"Pipe Write Failed","Commandline PIPE Encoder",MB_OK);
      exitThread = true; 
    }
  }
}