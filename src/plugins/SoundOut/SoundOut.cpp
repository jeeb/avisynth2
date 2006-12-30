// SoundOut Copyright Klaus Post 2006
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

// SoundOut (c) 2006 by Klaus Post

#include "SoundOut.h"
#include <stdio.h>
#include "FlacOutput.h"
#include "MacOutput.h"
#include "Mp2Output.h"
#include "WavOutput.h"

HINSTANCE g_hInst;
SoundOut* so;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
    env->AddFunction("SoundOut", "c", Create_SoundOut, 0);
    return "SoundOut";
}

AVSValue __cdecl Create_SoundOut(AVSValue args, void* user_data, IScriptEnvironment* env) {
  so = new SoundOut(args[0].AsClip(), env);
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


SoundOut::SoundOut(PClip _child, IScriptEnvironment* _env) : GenericVideoFilter(_child), currentOut(0), env(_env) {
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

  char text[800];
  sprintf_s(text, 800,
      "Audio Channels: %u.\n"
      "Sample Type: %s.\n"
      "Samples Per Second: %4d.\n"
      "Audio Samples: %u%u samples.\n"
      "Audio Data: %d%s.\n"
      "Audio Length: %02d:%02d:%02d:%03d.\n"
      ,vi.AudioChannels()
      ,s_type
      ,vi.audio_samples_per_second
      ,(unsigned int)(vi.num_audio_samples>>32)
      ,(unsigned int)(vi.num_audio_samples&0xfffffffff)
      ,(int)datasize, s_metric
      ,(aLenInMsecs/(60*60*1000)), (aLenInMsecs/(60*1000)) % 60, (aLenInMsecs/1000) % 60, aLenInMsecs % 1000
    );
  SetDlgItemText(wnd,IDC_STC_INFOTEXT,text);

}

SoundOut::~SoundOut() {
  DestroyWindow(wnd);
}

void SoundOut::SetOutput(SoundOutput* newinput) {
  currentOut = newinput;
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


