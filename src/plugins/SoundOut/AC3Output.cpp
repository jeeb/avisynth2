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

#include "AC3Output.h"

AC3Output* out;

BOOL CALLBACK AC3DialogProc(
  HWND wnd,  // handle to dialog box
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
        case IDC_AC3SAVE:          
          out->startEncoding();
          return true;
        case IDC_AC3CANCEL:
          delete out;
          return true;
        case IDC_AC3CBRMODE:
        case IDC_AC3VBRMODE:
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
const int AC3_CBRBitrateVal[] = {  32,  40,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 576, 640, -1 };

const int AC3_DRCSize = 6;
const int AC3_DRCVal[] = {DYNRNG_PROFILE_NONE,DYNRNG_PROFILE_FILM_LIGHT,DYNRNG_PROFILE_FILM_STANDARD,
  DYNRNG_PROFILE_MUSIC_LIGHT, DYNRNG_PROFILE_MUSIC_STANDARD, DYNRNG_PROFILE_SPEECH};

const char * const AC3_DRCString[] = { "None", "Film Light", "Film Standard", "Music Light", 
"Music Standard","Speech" };

const int AC3_ChannelSize = 9;
const int AC3_ChannelVal[] = {-1, 0, 1, 2, 3, 4 ,5 ,6, 7};
const int AC3_ChannelNum[] = {-1, 2, 1, 2, 3, 3 ,4 ,4, 5};

const char * const AC3_ChannelString[] = { "Auto", "1+1 (Ch1,Ch2)", "1/0 (C)", 
"2/0 (L,R)", "3/0 (L,R,C)","2/1 (L,R,S)",  "3/1 (L,R,C,S)", "2/2 (L,R,SL,SR)", "3/2 (L,R,C,SL,SR)"};

AC3Output::AC3Output(PClip _child, IScriptEnvironment* _env) : SoundOutput(_child ,_env)
{
  GUI_ready = false;
  params["outputFileFilter"] = AVSValue("AC3 files\0*.ac3\0All Files\0*.*\0\0");
  params["extension"] = AVSValue(".ac3");
  params["filterID"] = AVSValue("aften");
  params["iscbr"] = AVSValue(true);
  params["acmod"] = AVSValue(-1);
  params["cbrrate"] = AVSValue(384);
  params["vbrquality"] = AVSValue(220);
  params["blockswitch"] = AVSValue(false);
  params["bandwidthfilter"] = AVSValue(false);
  params["lfelowpass"] = AVSValue(false);
  params["dchighpass"] = AVSValue(false);
  params["accuratealloc"] = AVSValue(true);
  params["dolbysurround"] = AVSValue(false);
  params["drc"] = AVSValue(DYNRNG_PROFILE_NONE);
  params["dialognormalization"] = AVSValue(31);
  if (vi.AudioChannels() < 4)
    params["islfe"] = AVSValue(false);
  else
    params["islfe"] = AVSValue(true);

}

AC3Output::~AC3Output(void)
{
}
void AC3Output::showGUI() {
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_AC3SETTINGS),0,AC3DialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  for (int i = 0; AC3_CBRBitrateVal[i] !=-1; i++) {
    char buf[10];
    sprintf_s(buf,10,"%d", AC3_CBRBitrateVal[i]);
    SendDlgItemMessage(wnd, IDC_AC3CBRRATE, CB_ADDSTRING, 0, (LPARAM)buf);
  }
  for (int i = 0; i< AC3_DRCSize; i++) {
    SendDlgItemMessage(wnd, IDC_AC3DRC, CB_ADDSTRING, 0, (LPARAM)AC3_DRCString[i]);
  }
  for (int i = 0; i<32; i++) {
    char buf[10];
    sprintf_s(buf,10,"%d", i);
    SendDlgItemMessage(wnd, IDC_AC3DIALOG, CB_ADDSTRING, 0, (LPARAM)buf);
  }
  for (int i = 0; i<AC3_ChannelSize; i++) {
    SendDlgItemMessage(wnd, IDC_AC3CHANNELMAPPING, CB_ADDSTRING, 0, (LPARAM)AC3_ChannelString[i]);
  }
  switch (vi.AudioChannels()) {
    case 4:
    case 5:
      EnableWindow(GetDlgItem(wnd, IDC_AC3LFE), true);
      break;
    default:
      EnableWindow(GetDlgItem(wnd, IDC_AC3LFE), false);
  }

}

bool AC3Output::getParamsFromGUI() {
  params["cbrrate"] = AVSValue(AC3_CBRBitrateVal[SendDlgItemMessage(wnd, IDC_AC3CBRRATE, CB_GETCURSEL,0,0)]);
  params["dialognormalization"] = AVSValue((int)SendDlgItemMessage(wnd, IDC_AC3DIALOG, CB_GETCURSEL,0,0));
  params["acmod"] = AVSValue(AC3_ChannelVal[SendDlgItemMessage(wnd, IDC_AC3CHANNELMAPPING, CB_GETCURSEL,0,0)]);
  char buf[100];
  ((short*)buf)[0] = 100;
  SendDlgItemMessage(wnd, IDC_AC3VBRRATE, EM_GETLINE,0,(LPARAM)buf);
  int n = atoi(buf);
  if (n<0 || n>1023) {
    MessageBox(NULL,"VBR Quality must be between 0 and 1023","AC3 Encoder",MB_OK);
    return false;
  }
  params["vbrquality"] = AVSValue(n);
  params["iscbr"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3CBRMODE));
  params["bandwidthfilter"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3BANDWIDTH));
  params["lfelowpass"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3LFELOWPASS));
  params["dchighpass"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3DCHIGHPASS));
  params["dolbysurround"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3DCHIGHPASS));
  params["islfe"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3LFE));
  params["drc"] = AVSValue(AC3_DRCVal[SendDlgItemMessage(wnd, IDC_AC3DRC, CB_GETCURSEL,0,0)]);
  return true;
}

bool AC3Output::setParamsToGUI() {
  char buf[100];
  if (params["iscbr"].AsBool()) {
	  CheckDlgButton(wnd, IDC_AC3CBRMODE, BST_CHECKED);
	  CheckDlgButton(wnd, IDC_AC3VBRMODE, BST_UNCHECKED);
	  EnableWindow(GetDlgItem(wnd, IDC_AC3VBRRATE), false);
	  EnableWindow(GetDlgItem(wnd, IDC_VBRRATEUPDOWN), false);    

  } else {
	  CheckDlgButton(wnd, IDC_AC3VBRMODE, BST_CHECKED);
	  CheckDlgButton(wnd, IDC_AC3CBRMODE, BST_UNCHECKED);
	  EnableWindow(GetDlgItem(wnd, IDC_AC3VBRRATE), true);
	  EnableWindow(GetDlgItem(wnd, IDC_VBRRATEUPDOWN), true);    
  }
  CheckDlgButton(wnd, IDC_DSINPUT, params["dolbysurround"].AsBool());
  EnableWindow(GetDlgItem(wnd, IDC_DSINPUT), vi.AudioChannels() == 2);
  SendDlgItemMessage(wnd, IDC_AC3DIALOG,CB_SETCURSEL,params["dialognormalization"].AsInt(),0);

  sprintf_s(buf,sizeof(buf),"%u",params["vbrquality"].AsInt());
  SetDlgItemText(wnd,IDC_AC3VBRRATE,buf);
  SendDlgItemMessage(wnd, IDC_VBRRATEUPDOWN, UDM_SETRANGE, 0, MAKELONG (1023, 0));
  int n = params["cbrrate"].AsInt();
  for (int i = 0; AC3_CBRBitrateVal[i] !=-1; i++) {
    if (AC3_CBRBitrateVal[i] == n) 
      SendDlgItemMessage(wnd, IDC_AC3CBRRATE,CB_SETCURSEL,i,0);
  }

  n = params["drc"].AsInt();
  for (int i = 0; i<AC3_DRCSize; i++) {
    if (AC3_DRCVal[i] == n) 
      SendDlgItemMessage(wnd, IDC_AC3DRC,CB_SETCURSEL,i,0);
  }

  n = params["acmod"].AsInt();
  for (int i = 0; i<AC3_ChannelSize; i++) {
    if (AC3_ChannelVal[i] == n) 
      SendDlgItemMessage(wnd, IDC_AC3CHANNELMAPPING,CB_SETCURSEL,i,0);
  }
  CheckDlgButton(wnd, IDC_AC3BANDWIDTH, params["bandwidthfilter"].AsBool());
  CheckDlgButton(wnd, IDC_AC3LFELOWPASS, params["lfelowpass"].AsBool());
  CheckDlgButton(wnd, IDC_AC3DCHIGHPASS, params["dchighpass"].AsBool());
  if (vi.AudioChannels() == 4 || vi.AudioChannels() == 5) {
    CheckDlgButton(wnd, IDC_AC3LFE, params["islfe"].AsBool());
  } else {
    if (vi.AudioChannels()<4)
      CheckDlgButton(wnd, IDC_AC3LFE, false);
    else 
      CheckDlgButton(wnd, IDC_AC3LFE, true);
  }

  GUI_ready = true;
  return true;
}

bool AC3Output::initEncoder() {
  if (!params["iscbr"].AsBool())
    MessageBox(NULL,"VBR Bitrate is not compatible with hardware players and most software players.","Warning",MB_OK);

  aften_set_defaults(&aften);
  aften.channels = vi.AudioChannels();
  aften.samplerate = vi.audio_samples_per_second;
  switch (vi.sample_type) {
    case SAMPLE_INT8:
      aften.sample_format = A52_SAMPLE_FMT_U8; break;
    case SAMPLE_INT16:
      aften.sample_format = A52_SAMPLE_FMT_S16; break;
    case SAMPLE_INT24:
      aften.sample_format = A52_SAMPLE_FMT_S24; break;
    case SAMPLE_INT32:
      aften.sample_format = A52_SAMPLE_FMT_S32; break;
    case SAMPLE_FLOAT:
      aften.sample_format = A52_SAMPLE_FMT_FLT; break;
  }
  aften.params.bitrate = params["cbrrate"].AsInt();
  aften.params.dynrng_profile = (DynRngProfile)params["drc"].AsInt();
  aften.params.quality = params["vbrquality"].AsInt();
  aften.params.encoding_mode = params["iscbr"].AsBool() ? AFTEN_ENC_MODE_CBR : AFTEN_ENC_MODE_VBR;
  aften.params.bitalloc_fast = !params["accuratealloc"].AsBool();
  aften.params.use_bw_filter = params["bandwidthfilter"].AsBool();
  aften.params.use_lfe_filter = params["lfelowpass"].AsBool();
  aften.params.use_dc_filter = params["dchighpass"].AsBool();
  aften.params.use_block_switching = params["blockswitch"].AsBool();
  aften.meta.dialnorm = params["dialognormalization"].AsInt();
  aften.lfe = params["islfe"].AsBool();

  if (params["acmod"].AsInt() == -1) {
    aften_wav_channels_to_acmod(vi.AudioChannels(),0xFFFFFFFF, &aften.acmod, &aften.lfe);
  } else {
    aften.acmod = params["acmod"].AsInt();
    for (int i = 0; i<AC3_ChannelSize; i++) {
      if (AC3_ChannelVal[i] == aften.acmod) 
        if (AC3_ChannelNum[i] != (vi.AudioChannels() - aften.lfe))
          aften.acmod = -1;
    }
    if (aften.acmod == -1) {
      MessageBox(NULL,"Channel number does not match selected channel mapping.","AC3 Encoder",MB_OK);
      return false;
    }
  }

  if (vi.AudioChannels() == 2) 
    aften.meta.dsurmod = params["dolbysurround"].AsBool()+1;

  aften.system.n_threads = 1;  // Temporary fix to avoid hanging.

  if (aften_encode_init(&aften)) {
     MessageBox(NULL,"Could not initialize encoder. Probably invalid input.","AC3 Encoder",MB_OK);
     try {
      aften_encode_close(&aften); 
     } catch (...) {}
    return false;
  }
  if (fopen_s(&f, outputFile, "wbS")) {
    MessageBox(NULL,"Could not open file for writing","AC3 Encoder",MB_OK);
    return false;
  }

  return true;
}

void AC3Output::encodeBlock(unsigned char* in) {
  unsigned char frame[A52_MAX_CODED_FRAME_SIZE];
  aften_remap_wav_to_a52(in,A52_SAMPLES_PER_FRAME,aften.channels, aften.sample_format,aften.acmod);
  int out = aften_encode_frame(&aften, frame, in);
  if (out<0) {
    MessageBox(NULL,"Encoder error.","AC3 Encoder",MB_OK);
    exitThread = true;
    return;
  }
  int written = (int)fwrite(frame, 1, out, f);
  if (written<0) {
    MessageBox(NULL,"A Disk write error occured.","AC3 Encoder",MB_OK);
    exitThread = true;
    return;
  }
  encodedSamples += A52_SAMPLES_PER_FRAME;
  this->updateSampleStats(encodedSamples, vi.num_audio_samples);
}

void AC3Output::encodeLoop() {
  encodedSamples = 0;
  int sampleSize = vi.BytesPerAudioSample();
  unsigned char* fwav = (unsigned char*)malloc(A52_SAMPLES_PER_FRAME * sampleSize);  // Block sent to encoder
  int fwav_n = 0;
  SampleBlock* sb;
  do {
    sb = input->GetNextBlock();
    char* inSamples = (char*)sb->getSamples();
    while (sb->numSamples > A52_SAMPLES_PER_FRAME - fwav_n && !exitThread) {  // We have enough to fill a frame
      int cpBytes = (A52_SAMPLES_PER_FRAME - fwav_n) * sampleSize;
      memcpy(&fwav[fwav_n*sampleSize], inSamples, cpBytes);

      encodeBlock(fwav);
      inSamples += cpBytes;
      sb->numSamples -= A52_SAMPLES_PER_FRAME - fwav_n;
      fwav_n = 0;
    }
    if (sb->numSamples && !exitThread) {  // We still have some samples left
      memcpy(&fwav[fwav_n*sampleSize], inSamples, sb->numSamples * sampleSize);
      fwav_n += sb->numSamples;
      sb->numSamples = 0;
    }
 } while (!sb->lastBlock && !exitThread);

  if (fwav_n && !exitThread) {
    memset(&fwav[fwav_n*sampleSize], 0, (A52_SAMPLES_PER_FRAME - fwav_n) * sampleSize);
    encodeBlock(fwav);
  }
  this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);

	aften_encode_close(&aften);
  fclose(f);
  free(fwav);
  encodeThread = 0;
}

