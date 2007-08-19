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

#include "WavOutput.h"
#include <time.h>

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif

WavOutput* out;

const char * const WAV_TypeString[] = {
  "Microsoft WAV", "MS WAVE with WAVEFORMATEX", "Apple/SGI AIFF", "Sun/NeXT AU", "RAW PCM", "S.F. WAVE64", "Core Audio File", "Broadcast Wave" };

const int WAV_TypeVal[] = {
  SF_FORMAT_WAV,    SF_FORMAT_WAVEX,             SF_FORMAT_AIFF,  SF_FORMAT_AU, SF_FORMAT_RAW,SF_FORMAT_W64,SF_FORMAT_CAF, SF_FORMAT_WAV };

const char * const WAV_ExtString[] = {
  "Microsoft WAV (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0", "Microsoft WAV (*.wav))\0*.wav\0All Files (*.*)\0*.*\0\0",    "Apple/SGI AIFF (*.aif))\0*.aif;*.aiff\0All Files (*.*)\0*.*\0\0", "Sun/NeXT AU (*.au))\0*.au\0All Files (*.*)\0*.*\0\0", "RAW PCM (*.pcm))\0*.pcm;*.raw\0All Files (*.*)\0*.*\0\0", "S.F. WAVE64 (*.W64))\0*.w64\0All Files (*.*)\0*.*\0\0", "Core Audio File (*.caf))\0*.caf\0All Files (*.*)\0*.*\0\0","Broacast (*.bwf)\0*.bwf\0All Files (*.*)\0*.*\0\0" };


const char * const WAV_FormatString[] = {
  "8 Bit", "16 Bit", "24 Bit", "32 Bit", "Float"};

const int WAV_FormatVal[] = {
  SAMPLE_INT8, SAMPLE_INT16, SAMPLE_INT24, SAMPLE_INT32, SAMPLE_FLOAT };

BOOL CALLBACK WavDialogProc(
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
        case IDC_WAVSAVE:
          out->startEncoding();
          return true;
        case IDC_WAVCANCEL:
          delete out;
          return true;
			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}



WavOutput::WavOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child,  SAMPLE_INT8|SAMPLE_INT16|SAMPLE_INT24|SAMPLE_INT32|SAMPLE_FLOAT,SAMPLE_INT16),_env)
{
  params["type"] = AVSValue(0);
  params["outputFileFilter"] = AVSValue("WAV files\0*.wav\0PCM files\0*.pcm\0All Files\0*.*\0\0");
  params["extension"] = AVSValue(".wav");
  params["peakchunck"] = AVSValue(false);
  params["filterID"] = AVSValue("libsnd");
  params["format"] = AVSValue(0);

  for (int i = 0; i< 5; i++) {
     if (vi.IsSampleType(WAV_FormatVal[i]))
       params["format"] = AVSValue(i);
  }
}

WavOutput::~WavOutput(void)
{
}

void WavOutput::showGUI() {
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_WAVSETTINGS),0,WavDialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  for (int i = 0; i< 8; i++) {
     SendDlgItemMessage(wnd, IDC_WAVTYPE, CB_ADDSTRING, 0, (LPARAM)WAV_TypeString[i]);
  }
  for (int i = 0; i< 5; i++) {
     SendDlgItemMessage(wnd, IDC_WAVFORMAT, CB_ADDSTRING, 0, (LPARAM)WAV_FormatString[i]);
     if (vi.IsSampleType(WAV_FormatVal[i]))
       params["format"] = AVSValue(i);
  }
}

bool WavOutput::setParamsToGUI() {
  SendDlgItemMessage(wnd, IDC_WAVTYPE,CB_SETCURSEL,params["type"].AsInt(),0);
  CheckDlgButton(wnd, IDC_WAVPEAKINFO, params["peakchunck"].AsBool());
  SendDlgItemMessage(wnd, IDC_WAVFORMAT,CB_SETCURSEL,params["format"].AsInt(),0);
  return true;
}

bool WavOutput::getParamsFromGUI() {
  params["outputFileFilter"] = AVSValue(WAV_ExtString[SendDlgItemMessage(wnd, IDC_WAVTYPE, CB_GETCURSEL,0,0)]);
  params["type"] = AVSValue((int)SendDlgItemMessage(wnd, IDC_WAVTYPE, CB_GETCURSEL,0,0));
  params["peakchunck"] = AVSValue(!!IsDlgButtonChecked(wnd,IDC_WAVPEAKINFO));
  params["format"] = AVSValue((int)SendDlgItemMessage(wnd, IDC_WAVFORMAT, CB_GETCURSEL,0,0));
  return true;
}

bool WavOutput::initEncoder() {
  info.channels = vi.AudioChannels();
  info.frames = vi.num_audio_samples;
  info.samplerate = vi.audio_samples_per_second;
  int format = WAV_TypeVal[params["type"].AsInt()];
  int sample_format = WAV_FormatVal[params["format"].AsInt()];
  if (!vi.IsSampleType(sample_format)) {
    IScriptEnvironment* env = input->env;

    if (input)
      delete input;

    input = new SampleFetcher(ConvertAudio::Create(child,sample_format,sample_format), env);
    vi = input->child->GetVideoInfo();    
  }

  if (vi.IsSampleType(SAMPLE_INT8))
    format |= SF_FORMAT_PCM_U8;
  if (vi.IsSampleType(SAMPLE_INT16))
    format |= SF_FORMAT_PCM_16;
  if (vi.IsSampleType(SAMPLE_INT24))
    format |= SF_FORMAT_PCM_24;
  if (vi.IsSampleType(SAMPLE_INT32))
    format |= SF_FORMAT_PCM_32;
  if (vi.IsSampleType(SAMPLE_FLOAT))
    format |= SF_FORMAT_FLOAT;

  info.format = format;  

  if (!sf_format_check(&info)) {
    MessageBox(NULL,"An encoder error occured while initializing WAVE format.","WAVE Encoder",MB_OK);
    return false;
  }
  sndfile = sf_open(outputFile, SFM_WRITE, &info);
  if (!sndfile) {
    MessageBox(NULL,"Could not open file for writing.","WAV Encoder",MB_OK);
    return false;
  }
  int cmd = params["peakchunck"].AsBool();
  sf_command(sndfile, SFC_SET_ADD_PEAK_CHUNK, &cmd,1);
  if (params["type"].AsInt() == 7) {  // BWF   
    memset(&bi, 0, sizeof(bi));
    char name[32];
    DWORD len = 32;
    GetUserName(name, &len);
    memcpy(bi.originator, name, 32);
    memcpy(bi.originator_reference, name, 32);
	  time_t lt_t;
	  struct tm * lt;
	  time(&lt_t);
	  lt = localtime (&lt_t);
    strftime(bi.origination_date,10+1,"%Y-%m-%d",lt);
    strftime(bi.origination_time,8+1,"%H-%M-%S",lt);
    const char *desc = "Export from SoundOut for AviSynth";
    memcpy(&bi.description, desc, strlen(desc)+1);
    bi.version = 1;
    srand(GetTickCount());

    for (int i = 0; i<sizeof(bi.umid); i++) {
      bi.umid[i] = rand()%0xff;
    }

    sf_command(sndfile, SFC_SET_BROADCAST_INFO, &bi,1);
  }
  return true;
}

void WavOutput::encodeLoop() {
  __int64 encodedSamples = 0;
  SampleBlock* sb;
  do {
    sb = input->GetNextBlock();
    sf_write_raw(sndfile, sb->getSamples(), vi.BytesFromAudioSamples(sb->numSamples));
    encodedSamples += sb->numSamples;
    this->updateSampleStats(encodedSamples, vi.num_audio_samples);
  } while (!sb->lastBlock && !exitThread);
	if (sf_close(sndfile)) {
    MessageBox(NULL,"An encoder error occured while finalizing WAV output. Output file may not work","WAVE Encoder",MB_OK);
	}

 this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);
 encodeThread = 0;
}