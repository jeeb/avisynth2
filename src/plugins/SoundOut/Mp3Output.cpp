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

#include "Mp3Output.h"

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif

Mp3Output* out;

BOOL CALLBACK Mp3DialogProc(
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
        case IDC_MP3SAVE:          
          out->startEncoding();
          return true;
        case IDC_MP3CANCEL:
          delete out;
          return true;
        case IDC_MP3VBR:
        case IDC_MP3ABR:
        case IDC_MP3CBR:
          if (out->GUI_ready) {
            out->getParamsFromGUI();
            out->setParamsToGUI();
          }
          return true;

			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}
const int MP3_HighBitrateVal[] = {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320,-1};
const int MP3_MediumBitrateVal[] = {8, 16, 24 ,32 ,40 ,48 ,56 ,64 ,80 ,96 ,112 ,128 ,144 ,160,-1};
const int MP3_LowBitrateVal[] = {8, 16 ,24 ,32 ,40 ,48 ,56 ,64 ,80 ,96 ,112 ,128 ,144 ,160,-1};
const int nVBRpresets = 7;
const char * const MP3_VBRPresetString[] = {
  "Medium Fast", "Medium", "Fast Standard", "Standard", "Fast Extreme", "Extreme", "Insane" };

const preset_mode MP3_VBRPresetVal[] = { MEDIUM_FAST, MEDIUM, STANDARD_FAST,STANDARD, EXTREME_FAST, EXTREME, INSANE };

Mp3Output::Mp3Output(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_FLOAT|SAMPLE_INT16,SAMPLE_FLOAT) ,_env)
{
  params["outputFileFilter"] = AVSValue("MP3 files\0*.mp3\0All Files\0*.*\0\0");
  params["extension"] = AVSValue(".mp3");
  params["filterID"] = AVSValue("lame");
  params["abrrate"] = AVSValue(128);
  params["cbrrate"] = AVSValue(128);
  params["vbrpreset"] = AVSValue((int)STANDARD);
  params["mode"] = AVSValue(0);  // 0 = VBR, 1 = ABR, 2 = CBR
}

Mp3Output::~Mp3Output(void)
{
}
void Mp3Output::showGUI() {
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_MP3SETTINGS),0,Mp3DialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  switch (vi.audio_samples_per_second) {
    case 48000: case 44100: case 32000: 
      for (int i = 0; MP3_HighBitrateVal[i] !=-1; i++) {
        char buf[10];
        sprintf_s(buf,10,"%d", MP3_HighBitrateVal[i]);
        SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE, CB_ADDSTRING, 0, (LPARAM)buf);
      }
      break;
    case 16000: case 24000: case 22050:
      for (int i = 0; MP3_MediumBitrateVal[i] !=-1; i++) {
        char buf[10];
        sprintf_s(buf,10,"%d", MP3_MediumBitrateVal[i]);
        SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE, CB_ADDSTRING, 0, (LPARAM)buf);
      }
      break;
    case 8000: case 12000: case 11025:
      for (int i = 0; MP3_LowBitrateVal[i] !=-1; i++) {
        char buf[10];
        sprintf_s(buf,10,"%d", MP3_LowBitrateVal[i]);
        SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE, CB_ADDSTRING, 0, (LPARAM)buf);
      }
      break;
    default:
      MessageBox(NULL,"Notice: Unsupported Samplerate.","MP3 Encoder",MB_OK);
  }
  for (int i = 0; i< nVBRpresets; i++) {
    SendDlgItemMessage(wnd, IDC_MP3PRESET, CB_ADDSTRING, 0, (LPARAM)MP3_VBRPresetString[i]);
  }

}

bool Mp3Output::setParamsToGUI() {
  char buf[100];
  sprintf_s(buf,sizeof(buf),"%u",params["abrrate"].AsInt());
  SendDlgItemMessage(wnd, IDC_MP3ABRUPDOWN, UDM_SETRANGE, 0, MAKELONG (320, 0));
  SetDlgItemText(wnd,IDC_MP3ABRBITRATE,buf);
  int n = params["vbrpreset"].AsInt();
  for (int i = 0; i<nVBRpresets; i++) {
    if ((int)MP3_VBRPresetVal[i] == n) 
      SendDlgItemMessage(wnd, IDC_MP3PRESET,CB_SETCURSEL,i,0);
  }

  n = params["cbrrate"].AsInt();
  switch (vi.audio_samples_per_second) {
    case 48000: case 44100: case 32000: 
      for (int i = 0; MP3_HighBitrateVal[i] !=-1; i++) {
        if (MP3_HighBitrateVal[i] == n) 
        SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE,CB_SETCURSEL,i,0);
      }
      break;
    case 16000: case 24000: case 22050:
      for (int i = 0; MP3_MediumBitrateVal[i] !=-1; i++) {
        if (MP3_MediumBitrateVal[i] == n) 
        SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE,CB_SETCURSEL,i,0);
      }
      break;
    case 8000: case 12000: case 11025:
      for (int i = 0; MP3_LowBitrateVal[i] !=-1; i++) {
        if (MP3_LowBitrateVal[i] == n) 
        SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE,CB_SETCURSEL,i,0);
      }
      break;
    default:
      MessageBox(NULL,"Notice: Unsupported Samplerate.","MP3 Encoder",MB_OK);
      return false;
  }

  if (params["mode"].AsInt() == 0) {  // VBR
	  CheckDlgButton(wnd, IDC_MP3VBR, BST_CHECKED);
	  CheckDlgButton(wnd, IDC_MP3ABR, BST_UNCHECKED);
	  CheckDlgButton(wnd, IDC_MP3CBR, BST_UNCHECKED);
	  EnableWindow(GetDlgItem(wnd, IDC_MP3ABRBITRATE), false);
	  EnableWindow(GetDlgItem(wnd, IDC_MP3CBRBITRATE), false);    
	  EnableWindow(GetDlgItem(wnd, IDC_MP3PRESET), true);

  } else if (params["mode"].AsInt() == 1){  //ABR
	  CheckDlgButton(wnd, IDC_MP3VBR, BST_UNCHECKED);
	  CheckDlgButton(wnd, IDC_MP3ABR, BST_CHECKED);
	  CheckDlgButton(wnd, IDC_MP3CBR, BST_UNCHECKED);
	  EnableWindow(GetDlgItem(wnd, IDC_MP3ABRBITRATE), true);
	  EnableWindow(GetDlgItem(wnd, IDC_MP3CBRBITRATE), false);    
	  EnableWindow(GetDlgItem(wnd, IDC_MP3PRESET), false);
  } else {  // CBR
	  CheckDlgButton(wnd, IDC_MP3VBR, BST_UNCHECKED);
	  CheckDlgButton(wnd, IDC_MP3ABR, BST_UNCHECKED);
	  CheckDlgButton(wnd, IDC_MP3CBR, BST_CHECKED);
	  EnableWindow(GetDlgItem(wnd, IDC_MP3ABRBITRATE), false);
	  EnableWindow(GetDlgItem(wnd, IDC_MP3CBRBITRATE), true);    
	  EnableWindow(GetDlgItem(wnd, IDC_MP3PRESET), false);
  }
  GUI_ready = true;
  return true;
}

bool Mp3Output::getParamsFromGUI() {
  int mode = 0;
  mode = !!IsDlgButtonChecked(wnd, IDC_MP3ABR) ? 1 : mode;
  mode = !!IsDlgButtonChecked(wnd, IDC_MP3CBR) ? 2 : mode;
  params["mode"] = AVSValue(mode);
  char buf[100];
  ((short*)buf)[0] = 100;
  SendDlgItemMessage(wnd, IDC_AC3VBRRATE, EM_GETLINE,0,(LPARAM)buf);
  int n = atoi(buf);
  if (n<8 || n>320 && mode == 1) {
    MessageBox(NULL,"ABR Bitrate must be between 8 and 320","MP3 Encoder",MB_OK);
    return false;
  }
  params["abrrate"] = AVSValue(n);

  switch (vi.audio_samples_per_second) {
    case 48000: case 44100: case 32000: 
      params["cbrrate"] = AVSValue(MP3_HighBitrateVal[SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE, CB_GETCURSEL,0,0)]);
      break;
    case 16000: case 24000: case 22050:
      params["cbrrate"] = AVSValue(MP3_MediumBitrateVal[SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE, CB_GETCURSEL,0,0)]);
      break;
    case 8000: case 12000: case 11025:
      params["cbrrate"] = AVSValue(MP3_LowBitrateVal[SendDlgItemMessage(wnd, IDC_MP3CBRBITRATE, CB_GETCURSEL,0,0)]);
      break;
    default:
      MessageBox(NULL,"Notice: Unsupported Samplerate.","MP3 Encoder",MB_OK);
      return false;
  }
  params["vbrpreset"] = AVSValue((int)MP3_VBRPresetVal[SendDlgItemMessage(wnd, IDC_MP3PRESET, CB_GETCURSEL,0,0)]);

  return true;
}
bool Mp3Output::initEncoder() {
  lame = lame_init();  
  lame_set_num_channels(lame,vi.AudioChannels());
  lame_set_in_samplerate(lame,vi.audio_samples_per_second);
  if (params["mode"].AsInt() == 0) {  // VBR
    int preset = params["vbrpreset"].AsInt();
    switch (preset) {
      case MEDIUM_FAST:
        lame_set_VBR_q(lame, 4);
        lame_set_VBR(lame, vbr_mtrh);
        break;
      case MEDIUM:
        lame_set_VBR_q(lame, 4);
        lame_set_VBR(lame, vbr_rh);
        break;
      case STANDARD_FAST:
        lame_set_VBR_q(lame, 2);
        lame_set_VBR(lame, vbr_mtrh);
        break;
      case STANDARD:
        lame_set_VBR_q(lame, 2);
        lame_set_VBR(lame, vbr_rh);
        break;
      case EXTREME_FAST:
        lame_set_VBR_q(lame, 0);
        lame_set_VBR(lame, vbr_mtrh);
        break;
      case EXTREME:
        lame_set_VBR_q(lame, 0);
        lame_set_VBR(lame, vbr_rh);
        break;
      case INSANE:
        lame_set_preset(lame, INSANE);
    }
    lame_set_bWriteVbrTag( lame, 1 );
  } else if (params["mode"].AsInt() == 1)  { // ABR
    lame_set_preset(lame, params["abrrate"].AsInt());
    lame_set_bWriteVbrTag( lame, 1 );
  } else { // CBR
    lame_set_brate(lame, params["cbrrate"].AsInt());
    lame_set_VBR(lame, vbr_off);
    lame_set_bWriteVbrTag( lame, 0 );
  }
  if (vi.AudioChannels() == 1)
		lame_set_mode(lame, MONO);

  int ret_code = lame_init_params(lame);
  if (ret_code) {
    MessageBox(NULL,"An encoder error occured while initializing encoder","MP3 Encoder",MB_OK);
  }
  if (fopen_s(&f, outputFile, "wbS")) {
    MessageBox(NULL,"Could not open file for writing","MP3 Encoder",MB_OK);
    return false;
  }
  return true;
}

void Mp3Output::encodeLoop() {
  unsigned char *outbuffer = (unsigned char*)malloc(BLOCKSAMPLES);
  __int64 encodedBytes = 0;
  __int64 encodedSamples = 0;
  SampleBlock* sb;
  do {
    sb = input->GetNextBlock();
    int bytesReady = 0;
    if (vi.IsSampleType(SAMPLE_INT16)) {
      bytesReady = lame_encode_buffer_interleaved(lame, (short*)sb->getSamples(), sb->numSamples,outbuffer,BLOCKSAMPLES);
    } else {
      if (vi.AudioChannels()==2) {
        SFLOAT *samples = (SFLOAT*)sb->getSamples();
        SFLOAT *ch1 = (SFLOAT*)malloc(sb->numSamples * sizeof(SFLOAT));
        SFLOAT *ch2 = (SFLOAT*)malloc(sb->numSamples * sizeof(SFLOAT));
        for (int i=0; i<sb->numSamples; i++) {
          ch1[i] = samples[i*2] * 32768.0f;
          ch2[i] = samples[i*2+1]* 32768.0f;
        }
        bytesReady = lame_encode_buffer_float(lame, ch1, ch2, sb->numSamples, outbuffer, BLOCKSAMPLES);
        free(ch1);
        free(ch2);
      } else {
        SFLOAT *samples = (SFLOAT*)sb->getSamples();
        for (int i=0; i<sb->numSamples; i++)
          samples[i] *= 32768.0f;
        bytesReady = lame_encode_buffer_float(lame, (SFLOAT*)sb->getSamples(), (SFLOAT*)sb->getSamples(), sb->numSamples, outbuffer, BLOCKSAMPLES);
      }      
    }
    if (bytesReady<0) {
       MessageBox(NULL,"An encoder error occured.","MP3 Encoder",MB_OK);
       exitThread = true;
    }
    if (bytesReady) {
      encodedBytes += bytesReady;
      int written = (int)fwrite(outbuffer, bytesReady, 1, f);
      if (!written) {
         MessageBox(NULL,"A Disk write error occured.","MP3 Encoder",MB_OK);
        exitThread = true;
      }
    }
    encodedSamples += sb->numSamples;
    this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);
  } while (!sb->lastBlock && !exitThread);
  this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);

  int bytesReady = lame_encode_flush(lame, outbuffer, BLOCKSAMPLES);
  fwrite(outbuffer, bytesReady, 1, f);
  lame_mp3_tags_fid(lame, f);
  lame_close(lame);
  fclose(f);
  free(outbuffer);
}
