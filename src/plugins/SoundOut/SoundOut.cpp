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

#include "SoundOut.h"
#include <stdio.h>
#include "FlacOutput.h"
#include "MacOutput.h"
#include "Mp2Output.h"
#include "WavOutput.h"
#include "AC3Output.h"
#include "PipeOutput.h"
#include "Mp3Output.h"
#include "VorbisOutput.h"
#include "AnalyzeSound.h"
#include "RegistryIO.h"
#include "DummyClip.h"
#include "WavPackOutput.h"
#include <vector>

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif

HINSTANCE g_hInst;
SoundOut* so;
char buf[4096];
int base_params;
Param p;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
  p.clear();
  Param::iterator i;
  SoundOutput* out;
  AVSValue v = DummyClip::Create(NULL, NULL, env);
  PClip clip = v.AsClip();

  out = new WavOutput(clip,env);  
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  out = new WavPackOutput(clip,env);  
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  out = new MacOutput(clip,env);
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  out = new FlacOutput(clip,env);
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  out = new Mp2Output(clip,env);
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  out = new AC3Output(clip,env);
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  out = new PipeOutput(clip,env);
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  out = new Mp3Output(clip,env);
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  out = new VorbisOutput(clip,env);
  for (i = out->params.begin(); i != out->params.end(); i++)
    p.insert(*i);
  delete out;

  p.erase("filterID");
  p.erase("outputFileFilter");
  p.erase("extension");
  p.erase("useFilename");
  p.erase("showProgress");

  memset(buf,0,sizeof(buf));
  strcat_s(buf,4096,"c[output]s[filename]s[showprogress]b[autoclose]b[silentblock]b[addvideo]b");
  base_params = 7;  // How many arguments are defined above?
  for ( i = p.begin(); i != p.end(); i++) {
    AVSValue v = (*i).second;
    strcat_s(buf,4096,"[");
    strcat_s(buf,4096,(*i).first);
    if (v.IsString()) {
      strcat_s(buf,4096,"]s");
    } else if (v.IsInt()) {
      strcat_s(buf,4096,"]i");
    } else if (v.IsBool()) {
      strcat_s(buf,4096,"]b");
    } else if (v.IsFloat()) {
      strcat_s(buf,4096,"]f");
    }
  }
  env->AddFunction("SoundOut", buf, Create_SoundOut, 0);
  return "SoundOut";
}


DWORD WINAPI StartGUIThread(LPVOID p) {
  SoundOut* t = (SoundOut*)p;
  t->startUp();
  try {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  } catch (...) {
  }
  return 0;
}

AVSValue __cdecl Create_SoundOut(AVSValue args, void* user_data, IScriptEnvironment* env) {
  so = new SoundOut(args, env);
  return so;
}

//Nicked from info()
const char* t_INT8="Integer 8 bit";
const char* t_INT16="Integer 16 bit";
const char* t_INT24="Integer 24 bit";
const char* t_INT32="Integer 32 bit";
const char* t_FLOAT32="Float 32 bit";

const char* t_BYTES="B";
const char* t_KB="KB";
const char* t_MB="MB";
const char* t_GB="GB";


SoundOut::SoundOut(AVSValue args, IScriptEnvironment* _env) : GenericVideoFilter(args[0].AsClip()), currentOut(0), env(_env) {
  guiThread = 0;
  wnd = 0;
  generateVideo = false;
  if (!vi.HasAudio()) {
    MessageBox(NULL,"No audio found in clip. I will just go away!","SoundOut",MB_OK);
    return;
  }
  blockRequests = true;

  if (args[2].Defined()) {
    xferParams["useFilename"] = args[2];
  }
  if (args[3].Defined()) {
    xferParams["showProgress"] = args[3];
  }
  if (args[4].Defined()) {
    xferParams["autoCloseWindow"] = args[4];
  }
  silentBlock = args[5].AsBool(true);

  if (!vi.HasVideo() && args[6].AsBool(true)) {
    vi.pixel_type = VideoInfo::CS_BGR32;
    vi.width = 32;
    vi.height = 32;
    vi.fps_numerator = 25;
    vi.fps_denominator = 1;
    vi.num_frames = vi.FramesFromAudioSamples(vi.num_audio_samples);
    generateVideo = true;
  }
  int n = base_params;  // Offset in arg array
  for (Param::iterator i = p.begin(); i != p.end(); i++) {
    if (args[n].Defined()) {
      const char* name = (*i).first;
      xferParams[name] = args[n];
    }
    n++;  // Next param
  }
  forceOut = 0;
  if (args[1].Defined()) {  // Direct output.
    forceOut = args[1].AsString();
    blockRequests = true;
  }

  guiThread = CreateThread(NULL,NULL,StartGUIThread,this, NULL,NULL);
  SetThreadPriority(guiThread,THREAD_PRIORITY_ABOVE_NORMAL);
}

SoundOut::~SoundOut() {
  while (blockRequests) { // Encoder still running
    Sleep(1000);
  }
  if (wnd)
    DestroyWindow(wnd);
  if (guiThread)
   TerminateThread(guiThread,0);
  blockRequests = false;
  p.clear();
  _CrtDumpMemoryLeaks();
}
void SoundOut::startUp() {
  if (forceOut) {  // Direct output.
    SoundOutput *out = 0;
    const char* type = forceOut;
    if (!_stricmp(type,"wav"))
      out = new WavOutput(child,env);  
    if (!_stricmp(type,"mac"))
      out = new MacOutput(child,env);
    if (!_stricmp(type,"flac"))
      out = new FlacOutput(child,env);
    if (!_stricmp(type,"mp2"))
      out = new Mp2Output(child,env);
    if (!_stricmp(type,"ac3"))
      out = new AC3Output(child,env);
    if (!_stricmp(type,"cmd"))
      out = new PipeOutput(child,env);
    if (!_stricmp(type,"mp3"))
      out = new Mp3Output(child,env);
    if (!_stricmp(type,"ogg"))
      out = new VorbisOutput(child,env);
    if (!_stricmp(type,"wv"))
      out = new WavPackOutput(child,env);

    if (!out) {
      blockRequests = false;  // Error - let allow it to destroy itself.
      env->ThrowError("SoundOut: Output type not recognized.");
    }
    out->parent = this;
    passSettings(out);
    disableControls();
    if (!out->startEncoding()) {
      blockRequests = false;  // Error - let allow it to destroy itself.
	  }
  } else {
    openGUI();
    blockRequests = false;  // We open GUI, so now we no longer need to block requests.
  }
}

void SoundOut::passSettings(SoundOutput *s) {
  for (Param::iterator i = xferParams.begin(); i != xferParams.end(); i++) {
    s->params[(*i).first] = (*i).second;
  }  
}

void SoundOut::openGUI() {
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_DLG_MAIN),0,MainDialogProc);
	ShowWindow(wnd,SW_NORMAL);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));


  // Mostly copy/paste from info()
  const char* s_type = NULL;
  if (vi.SampleType()==SAMPLE_INT8) s_type=t_INT8;
  if (vi.SampleType()==SAMPLE_INT16) s_type=t_INT16;
  if (vi.SampleType()==SAMPLE_INT24) s_type=t_INT24;
  if (vi.SampleType()==SAMPLE_INT32) s_type=t_INT32;
  if (vi.SampleType()==SAMPLE_FLOAT) s_type=t_FLOAT32;

  int aLenInMsecs = 0;
  if (vi.HasAudio()) {
    aLenInMsecs = (int)(1000.0 * (double)vi.num_audio_samples / (double)vi.audio_samples_per_second);
  }

  const char* s_metric = t_BYTES;
  __int64 datasize = vi.BytesFromAudioSamples(vi.num_audio_samples);
  if (datasize > 10*1024) { // More than 10k, switch to KB notation
    datasize >>=10;
    s_metric = t_KB;
  }
  if (datasize > 10*1024) { // More than 10MB, switch to MB notation
    datasize >>=10;
    s_metric = t_MB;
  }
  if (datasize > 10*1024) { // More than 10GB, switch to GB notation
    datasize >>=10;
    s_metric = t_GB;
  }
  char samples[20];
  if (vi.num_audio_samples > 0x7ffffffe) {
    sprintf_s(samples,"%u%u"
      ,(unsigned int)(vi.num_audio_samples>>32)
      ,(unsigned int)(vi.num_audio_samples&0xfffffffff));
  } else {
    sprintf_s(samples,"%u"
      ,(unsigned int)(vi.num_audio_samples&0xfffffffff));
  }
  char text[800];
  sprintf_s(text, 800,
      "Audio Channels: %u.\n"
      "Sample Type: %s.\n"
      "Samples Per Second: %4d.\n"
      "Audio Samples: %s samples.\n"
      "Audio Data: %d%s.\n"
      "Audio Length: %02d:%02d:%02d:%03d.\n"
      ,vi.AudioChannels()
      ,s_type
      ,vi.audio_samples_per_second
      , samples
      ,(int)datasize, s_metric
      ,(aLenInMsecs/(60*60*1000)), (aLenInMsecs/(60*1000)) % 60, (aLenInMsecs/1000) % 60, aLenInMsecs % 1000
    );
  SetDlgItemText(wnd,IDC_STC_INFOTEXT,text);
}
void SoundOut::SetOutput(SoundOutput* newinput) {
  currentOut = newinput;
  RegistryIO::RetrieveSettings(newinput->params, env);
  passSettings(newinput);
  newinput->showGUI();
  newinput->setParamsToGUI();
  newinput->parent = this;
  disableControls();
}

void SoundOut::disableControls() {
  blockRequests = true;
  if (wnd) {
    ShowWindow(wnd,SW_MINIMIZE);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_ANALYZE), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEWAV), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEFLAC), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEMP3), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEAPE), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEOGG), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEMP2), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEAC3), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEWAVPACK), false);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_COMMANDOUT), false);
  }
}

void SoundOut::reEnableControls() {
  blockRequests = false;
  if (wnd) {
    ShowWindow(wnd,SW_NORMAL);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_ANALYZE), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEWAV), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEFLAC), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEMP3), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEAPE), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEOGG), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEMP2), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEAC3), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_COMMANDOUT), true);
    EnableWindow(GetDlgItem(wnd, IDC_BTN_SAVEWAVPACK), true);
  }
}
void __stdcall SoundOut::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) {
  while (blockRequests) {
    if (silentBlock) {
      memset(buf, 0, vi.BytesFromAudioSamples(count));
      return;
    }
    Sleep(1000);
  }
  child->GetAudio(buf, start, count, env);
}

PVideoFrame __stdcall SoundOut::GetFrame(int n, IScriptEnvironment* env) {

  if (this->generateVideo) {
    PVideoFrame v = env->NewVideoFrame(vi);
    BYTE* dst = v->GetWritePtr();
    memset(dst,0,v->GetPitch() * v->GetHeight());
    return v;
  }
  return child->GetFrame(n, env);
}

BOOL CALLBACK MainDialogProc(
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
					ShowWindow(hwndDlg,SW_MINIMIZE);
//          DestroyWindow(hwndDlg);
          return true;
					break;
        case IDC_BTN_SAVEWAV:
          so->SetOutput(new WavOutput(so->GetClip(),so->env));
          return true;
        case IDC_BTN_SAVEAPE:
          so->SetOutput(new MacOutput(so->GetClip(),so->env));
          return true;
        case IDC_BTN_SAVEFLAC:
          so->SetOutput(new FlacOutput(so->GetClip(),so->env));
          return true;
        case IDC_BTN_SAVEMP2:
          so->SetOutput(new Mp2Output(so->GetClip(),so->env));
          return true;
        case IDC_BTN_SAVEAC3:
          so->SetOutput(new AC3Output(so->GetClip(),so->env));
          return true;
        case IDC_BTN_COMMANDOUT:
          so->SetOutput(new PipeOutput(so->GetClip(),so->env));
          return true;
        case IDC_BTN_SAVEMP3:
          so->SetOutput(new Mp3Output(so->GetClip(),so->env));
          return true;
        case IDC_BTN_SAVEOGG:
          so->SetOutput(new VorbisOutput(so->GetClip(),so->env));
          return true;
        case IDC_BTN_ANALYZE:
          so->SetOutput(new AnalyzeSound(so->GetClip(),so->env));
          return true;
        case IDC_BTN_SAVEWAVPACK:
          so->SetOutput(new WavPackOutput(so->GetClip(),so->env));          
          return true;
			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}

BOOL WINAPI DllMain(
  HINSTANCE hinstDLL,
  DWORD fdwReason,
  LPVOID lpvReserved
  ) 
{
  g_hInst = hinstDLL;
  return TRUE;
}


