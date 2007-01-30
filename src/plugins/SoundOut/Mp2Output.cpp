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

#include "Mp2Output.h"

Mp2Output* out;



BOOL CALLBACK Mp2DialogProc(
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
        case IDC_MP2SAVEAS:          
          out->startEncoding();
          return true;
        case IDC_MP2CANCEL:
          delete out;
          return true;
        case IDC_MP2VBR:
          if (!!IsDlgButtonChecked(hwndDlg, IDC_MP2VBR)) {
	          EnableWindow(GetDlgItem(hwndDlg, IDC_MP2VBRQUALITY), true);
          } else {
	          EnableWindow(GetDlgItem(hwndDlg, IDC_MP2VBRQUALITY), false);
          }
          return true;
			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}
const char * const MP2_HighBitrateString[] = {
  "32", "48", "56", "64", "80", "96", "112", "128", "160", "192", "224", "256", "320", "384" };

const int MP2_HighBitrateVal[] = {
  32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 };

const char * const MP2_LowBitrateString[] = {
  "8", "16", "24", "32", "40", "48", "56", "64", "80", "96", "112", "128", "144", "160" };

const int MP2_LowBitrateVal[] = {
  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 };

const char * const MP2_StereoString[] = {
  "Automatic", "Separate Stereo", "Joint Stereo", "Dual Channel", "Mono" };

const int MP2_StereoVal[] = {
  -1, 0, 1, 2, 3 };

const char * const MP2_PsymodelString[] = {
  "Fast & Dumb", "Low complexity", "ISO PAM 1", "ISO PAM 2", "PAM 1 Rewrite", "PAM 2 Rewrite" };

const int MP2_PsymodelVal[] = {
  -1, 0, 1, 2, 3, 4 };

const char * const MP2_EmphString[] = {
  "No Emphasis", "50/15 ms", "CCIT J.17" };

  const TWOLAME_Emphasis MP2_EmphVal[] = {
  TWOLAME_EMPHASIS_N, TWOLAME_EMPHASIS_5 , TWOLAME_EMPHASIS_C };

Mp2Output::Mp2Output(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_INT16,SAMPLE_INT16),_env)
{
  out = this;

  params["outputFileFilter"] = AVSValue("MP2 files (*.mp2;*.mpa;*.m2a)\0*.mp2;*.mpa;*.m2a\0All Files (*.*)\0*.*\0\0");
  params["extension"] = AVSValue(".mp2");
  params["filterID"] = AVSValue("twolameout");
  params["stereomode"] = AVSValue(-1);
  params["psymodel"] = AVSValue(3);
  params["bitrate"] = AVSValue(192);
  params["vbrquality"] = AVSValue(0.0f);
  params["padding"] = AVSValue(false);
  params["vbr"] = AVSValue(false);
  params["quick"] = AVSValue(false);
  params["dab"] = AVSValue(false);
  params["crc"] = AVSValue(false);
  params["original"] = AVSValue(false);
  params["copyright"] = AVSValue(false);
  params["private"] = AVSValue(false);
  params["emphasis"] = AVSValue(TWOLAME_EMPHASIS_N);
}

Mp2Output::~Mp2Output(void)
{
}
void Mp2Output::showGUI() {
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_MP2SETTINGS),0,Mp2DialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  int bitrate = params["bitrate"].AsInt();
  for (int i = 0; i< 14; i++) {
    if (vi.audio_samples_per_second >= 32000) {
      SendDlgItemMessage(wnd, IDC_MP2BITRATE, CB_ADDSTRING, 0, (LPARAM)MP2_HighBitrateString[i]);
      if (bitrate == MP2_HighBitrateVal[i])
        SendDlgItemMessage(wnd, IDC_MP2BITRATE,CB_SETCURSEL,i,0);
    } else {
      SendDlgItemMessage(wnd, IDC_MP2BITRATE, CB_ADDSTRING, 0, (LPARAM)MP2_LowBitrateString[i]);
      if (bitrate == MP2_LowBitrateVal[i])
        SendDlgItemMessage(wnd, IDC_MP2BITRATE,CB_SETCURSEL,i,0);
    }
  }

  if (vi.AudioChannels() == 1) {
	  EnableWindow(GetDlgItem(wnd, IDC_MP2CHANNELS), false);
    SendDlgItemMessage(wnd, IDC_MP2CHANNELS, CB_ADDSTRING, 0, (LPARAM)"Mono");
    SendDlgItemMessage(wnd, IDC_MP2CHANNELS, CB_SETCURSEL,0,0);
  } else if (vi.AudioChannels() == 2){
    for (int i = 0; i< 5; i++) {
      SendDlgItemMessage(wnd, IDC_MP2CHANNELS, CB_ADDSTRING, 0, (LPARAM)MP2_StereoString[i]);
      if (params["stereomode"].AsInt() == MP2_StereoVal[i])
        SendDlgItemMessage(wnd, IDC_MP2CHANNELS, CB_SETCURSEL,i,0);
    }    
  } else {
    MessageBox(NULL,"You have too many channels.","MP2 Encoder",MB_OK);
    return;
  }

  for (int i = 0; i< 6; i++) {
    SendDlgItemMessage(wnd, IDC_MP2PSYMODEL, CB_ADDSTRING, 0, (LPARAM)MP2_PsymodelString[i]);
  }

  for (int i = -10; i< 12; i+=2) {
    char buf[20];
    sprintf_s(buf,20,"%d",i);
    SendDlgItemMessage(wnd, IDC_MP2VBRQUALITY, CB_ADDSTRING, 0, (LPARAM)buf);
  }

  for (int i = 0; i< 3; i++) {
    SendDlgItemMessage(wnd, IDC_MP2DEEMPHASIS, CB_ADDSTRING, 0, (LPARAM)MP2_EmphString[i]);
  }
}

bool Mp2Output::setParamsToGUI() {
  int bitrate = params["bitrate"].AsInt();
  for (int i = 0; i< 14; i++) {
    if (vi.audio_samples_per_second >= 32000) {
      if (bitrate == MP2_HighBitrateVal[i])
        SendDlgItemMessage(wnd, IDC_MP2BITRATE,CB_SETCURSEL,i,0);
    } else {
      if (bitrate == MP2_LowBitrateVal[i])
        SendDlgItemMessage(wnd, IDC_MP2BITRATE,CB_SETCURSEL,i,0);
    }
  }
  if (vi.AudioChannels() == 1) {
    SendDlgItemMessage(wnd, IDC_MP2CHANNELS, CB_SETCURSEL,0,0);
  } else if (vi.AudioChannels() == 2){
    for (int i = 0; i< 5; i++) {
      if (params["stereomode"].AsInt() == MP2_StereoVal[i])
        SendDlgItemMessage(wnd, IDC_MP2CHANNELS, CB_SETCURSEL,i,0);
    }    
  } else {
    return false;
  }
  for (int i = 0; i< 6; i++) {
    if (params["psymodel"].AsInt() == MP2_PsymodelVal[i]) 
      SendDlgItemMessage(wnd, IDC_MP2PSYMODEL, CB_SETCURSEL,i,0);
  }

  int q = (params["vbrquality"].AsFloat() + 10.01f) / 2.0f;
    SendDlgItemMessage(wnd, IDC_MP2VBRQUALITY, CB_SETCURSEL,q,0);

  for (int i = 0; i< 3; i++) {
    if (params["emphasis"].AsInt() == MP2_EmphVal[i])
      SendDlgItemMessage(wnd, IDC_MP2DEEMPHASIS, CB_SETCURSEL,i,0);
  }
  CheckDlgButton(wnd, IDC_MP2ADDPADDING, params["padding"].AsBool());
  CheckDlgButton(wnd, IDC_MP2VBR, params["vbr"].AsBool());
  CheckDlgButton(wnd, IDC_MP2QUICK, params["quick"].AsBool());
  CheckDlgButton(wnd, IDC_MP2DABEXTENSIONS, params["dab"].AsBool());
  CheckDlgButton(wnd, IDC_MP2ADDCRC, params["crc"].AsBool());
  CheckDlgButton(wnd, IDC_MP2ORIGINAL, params["original"].AsBool());
  CheckDlgButton(wnd, IDC_MP2VBR, params["copyright"].AsBool());
  CheckDlgButton(wnd, IDC_MP2PRIVATE, params["private"].AsBool());
  EnableWindow(GetDlgItem(wnd, IDC_MP2VBRQUALITY), !!IsDlgButtonChecked(wnd, IDC_MP2VBR));
  return true;
}

bool Mp2Output::getParamsFromGUI() {
  params["padding"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_MP2ADDPADDING)); 
  params["vbr"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_MP2VBR));
  params["quick"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_MP2QUICK));
  params["dab"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_MP2DABEXTENSIONS));
  params["crc"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_MP2ADDCRC));
  params["original"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_MP2ORIGINAL));
  params["copyright"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_MP2VBR));
  params["private"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_MP2PRIVATE));
  params["stereomode"] = AVSValue(MP2_StereoVal[SendDlgItemMessage(wnd, IDC_MP2CHANNELS, CB_GETCURSEL,0,0)]);
  params["psymodel"] = AVSValue(MP2_PsymodelVal[SendDlgItemMessage(wnd, IDC_MP2PSYMODEL, CB_GETCURSEL,0,0)]);
  params["vbrquality"] = AVSValue((float)SendDlgItemMessage(wnd, IDC_MP2VBRQUALITY, CB_GETCURSEL,0,0)*2.0f - 10.0f);
  params["emphasis"] = AVSValue(MP2_EmphVal[SendDlgItemMessage(wnd, IDC_MP2DEEMPHASIS, CB_GETCURSEL,0,0)]);
  int bitrate = 0;
  if (vi.audio_samples_per_second >= 32000) {
    bitrate = MP2_HighBitrateVal[SendDlgItemMessage(wnd, IDC_MP2BITRATE, CB_GETCURSEL,0,0)];
  } else {
    bitrate = MP2_LowBitrateVal[SendDlgItemMessage(wnd, IDC_MP2BITRATE, CB_GETCURSEL,0,0)];
  }
  params["bitrate"] = AVSValue(bitrate);
  return true;
}

bool Mp2Output::initEncoder() {
	encodeOptions = twolame_init();
  if(twolame_set_in_samplerate(encodeOptions,vi.audio_samples_per_second)) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_in_samplerate'","MP2 Encoder",MB_OK);
    return false;
  }
  if(twolame_set_out_samplerate(encodeOptions,vi.audio_samples_per_second)) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_out_samplerate'","MP2 Encoder",MB_OK);
    return false;
  }
  if(twolame_set_num_channels(encodeOptions,vi.AudioChannels())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_num_channels'","MP2 Encoder",MB_OK);
    return false;
  }
  if (vi.AudioChannels() == 2) {
    if(twolame_set_mode(encodeOptions,(TWOLAME_MPEG_mode)params["stereomode"].AsInt())) {
      MessageBox(NULL,"An encoder error occured while setting 'twolame_set_mode'","MP2 Encoder",MB_OK);
      return false;
    }  
  }
  if(twolame_set_bitrate(encodeOptions, params["bitrate"].AsInt())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_bitrate'","MP2 Encoder",MB_OK);
    return false;
  }
  if(twolame_set_VBR_max_bitrate_kbps(encodeOptions, params["bitrate"].AsInt())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_VBR_max_bitrate_kbps'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_psymodel(encodeOptions, params["psymodel"].AsInt())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_psymodel'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_emphasis(encodeOptions, (TWOLAME_Emphasis)params["emphasis"].AsInt())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_emphasis'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_error_protection(encodeOptions, params["crc"].AsBool())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_error_protection'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_copyright(encodeOptions, params["copyright"].AsBool())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_copyright'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_original(encodeOptions, params["original"].AsBool())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_original'","MP2 Encoder",MB_OK);
    return false;
  }

/*  if(twolame_set_private(encodeOptions, (TWOLAME_Padding)params["private"].AsBool())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_private'","MP2 Encoder",MB_OK);
    return false;
  }
*/
  if(twolame_set_padding(encodeOptions, (TWOLAME_Padding)params["padding"].AsBool())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_padding'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_VBR(encodeOptions, params["vbr"].AsBool())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_VBR'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_quick_mode(encodeOptions, params["quick"].AsBool())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_quick_mode'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_DAB(encodeOptions, params["dab"].AsBool())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_DAB'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_set_VBR_level(encodeOptions, params["vbrquality"].AsFloat())) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_set_VBR_level'","MP2 Encoder",MB_OK);
    return false;
  }

  if(twolame_init_params(encodeOptions)) {
    MessageBox(NULL,"An encoder error occured while setting 'twolame_init_params'","MP2 Encoder",MB_OK);
    return false;
  }
  return true;
}

void Mp2Output::encodeLoop() {
  FILE *f;
  fopen_s(&f, outputFile, "wbS");
  unsigned char *outbuffer = (unsigned char*)malloc(BLOCKSAMPLES);
  __int64 encodedBytes = 0;
  __int64 encodedSamples = 0;
  SampleBlock* sb;
  do {
    sb = input->GetNextBlock();
    int bytesReady = 0;
    if (vi.IsSampleType(SAMPLE_INT16)) {
      bytesReady = twolame_encode_buffer_interleaved(encodeOptions, (short*)sb->getSamples(), sb->numSamples,outbuffer,BLOCKSAMPLES);
    }
    if (bytesReady<0) {
       MessageBox(NULL,"An encoder error occured.","MP2 Encoder",MB_OK);
       exitThread = true;
    }
    if (bytesReady) {
      encodedBytes += bytesReady;
      int written = (int)fwrite(outbuffer, bytesReady, 1, f);
      if (!written) {
         MessageBox(NULL,"A Disk write error occured.","MP2 Encoder",MB_OK);
        exitThread = true;
      }
    }
    encodedSamples += sb->numSamples;
    this->updateSampleStats(encodedSamples, vi.num_audio_samples);
  } while (!sb->lastBlock && !exitThread);
  this->updateSampleStats(encodedSamples, vi.num_audio_samples);

  int bytesReady = twolame_encode_flush(encodeOptions, outbuffer, BLOCKSAMPLES);
  fwrite(outbuffer, bytesReady, 1, f);
  fclose(f);
  twolame_close(&encodeOptions);
}
