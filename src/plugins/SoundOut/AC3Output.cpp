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
          if (!!IsDlgButtonChecked(wnd, IDC_AC3CBRMODE)) {
	          CheckDlgButton(wnd, IDC_AC3CBRMODE, BST_CHECKED);
	          EnableWindow(GetDlgItem(wnd, IDC_AC3VBRRATE), false);
	          EnableWindow(GetDlgItem(wnd, IDC_VBRRATEUPDOWN), false);    
          } else {
	          CheckDlgButton(wnd, IDC_AC3VBRMODE, BST_CHECKED);
	          EnableWindow(GetDlgItem(wnd, IDC_AC3VBRRATE), true);
	          EnableWindow(GetDlgItem(wnd, IDC_VBRRATEUPDOWN), true);    
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

AC3Output::AC3Output(PClip _child, IScriptEnvironment* _env) : SoundOutput(_child ,_env)
{
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_AC3SETTINGS),0,AC3DialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  params["outputFileFilter"] = AVSValue("AC3 files\0*.ac3\0All Files\0*.*\0\0");
  params["extension"] = AVSValue(".ac3");
  params["iscbr"] = AVSValue(true);
  params["cbrrate"] = AVSValue(384);
  params["vbrquality"] = AVSValue(220);
  params["blockswitch"] = AVSValue(false);
  params["lfelowpass"] = AVSValue(false);
  params["dchighpass"] = AVSValue(false);
  params["accuratealloc"] = AVSValue(true);
  params["drc"] = AVSValue(DYNRNG_PROFILE_NONE);

  for (int i = 0; AC3_CBRBitrateVal[i] !=-1; i++) {
    char buf[10];
    sprintf_s(buf,10,"%d", AC3_CBRBitrateVal[i]);
    SendDlgItemMessage(wnd, IDC_AC3CBRRATE, CB_ADDSTRING, 0, (LPARAM)buf);
  }
  for (int i = 0; i< AC3_DRCSize; i++) {
    SendDlgItemMessage(wnd, IDC_AC3DRC, CB_ADDSTRING, 0, (LPARAM)AC3_DRCString[i]);
  }

  setParamsToGUI();
}

AC3Output::~AC3Output(void)
{
}

bool AC3Output::getParamsFromGUI() {
  params["cbrrate"] = AVSValue(AC3_CBRBitrateVal[SendDlgItemMessage(wnd, IDC_AC3CBRRATE, CB_GETCURSEL,0,0)]);
  char buf[100];
  int n;
  SendDlgItemMessage(wnd, IDC_AC3VBRRATE, EM_GETLINE,0,(LPARAM)buf);
  sscanf_s(buf,"%d",&n);
  if (n<0 || n>1023) {
    MessageBox(NULL,"VBR Bitrate must be between 0 and 1023","AC3 Encoder",MB_OK);
    return false;
  }
  params["vbrquality"] = AVSValue(n);
  params["iscbr"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3CBRMODE));
  params["blockswitch"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3BLOCKSWITCH));
  params["lfelowpass"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3LFELOWPASS));
  params["dchighpass"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3DCHIGHPASS));
  params["accuratealloc"] = AVSValue(!!IsDlgButtonChecked(wnd, IDC_AC3ACCURATEALLOC));
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

  CheckDlgButton(wnd, IDC_AC3BLOCKSWITCH, params["blockswitch"].AsBool());
  CheckDlgButton(wnd, IDC_AC3LFELOWPASS, params["lfelowpass"].AsBool());
  CheckDlgButton(wnd, IDC_AC3DCHIGHPASS, params["dchighpass"].AsBool());
  CheckDlgButton(wnd, IDC_AC3ACCURATEALLOC, params["accuratealloc"].AsBool());

  return true;
}

bool AC3Output::initEncoder() {
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
  aften.params.use_lfe_filter = params["lfelowpass"].AsBool();
  aften.params.use_dc_filter = params["dchighpass"].AsBool();
  aften.params.use_block_switching = params["blockswitch"].AsBool();
  aften_plain_wav_to_acmod(vi.AudioChannels(), &aften.acmod, &aften.lfe);

  if (aften_encode_init(&aften)) {
     MessageBox(NULL,"Could not initialize encoder. Might be invalid input.","AC3 Encoder",MB_OK);
     try {
      aften_encode_close(&aften); 
     } catch (...) {}
    return false;
  }
  return true;
}

void AC3Output::encodeBlock(unsigned char* in) {
  unsigned char frame[A52_MAX_CODED_FRAME_SIZE];
  //aften_remap_wav_to_a52(in,A52_FRAME_SIZE,aften.channels, aften.sample_format,aften.acmod);
  int out = aften_encode_frame(&aften, frame, in);
  if (out<0) {
    MessageBox(NULL,"Encoder error.","AC3 Encoder",MB_OK);
    exitThread = true;
  }
  int written = (int)fwrite(frame, 1, out, f);
  if (written<0) {
    MessageBox(NULL,"A Disk write error occured.","AC3 Encoder",MB_OK);
    exitThread = true;
  }
  encodedSamples += A52_FRAME_SIZE;
  this->updateSampleStats(encodedSamples, vi.num_audio_samples);
}

void AC3Output::encodeLoop() {
  fopen_s(&f, outputFile, "wbS");
  encodedSamples = 0;
  int sampleSize = vi.BytesPerAudioSample();
  unsigned char* fwav = (unsigned char*)malloc(A52_FRAME_SIZE * sampleSize);  // Block sent to encoder
  int fwav_n = 0;
  SampleBlock* sb;
  
  do {
    sb = input->GetNextBlock();
    char* inSamples = (char*)sb->getSamples();
    while (sb->numSamples > A52_FRAME_SIZE - fwav_n && !exitThread) {  // We have enough to fill a frame
      int cpBytes = (A52_FRAME_SIZE - fwav_n) * sampleSize;
      memcpy(&fwav[fwav_n*sampleSize], inSamples, cpBytes);
      encodeBlock(fwav);
      inSamples += cpBytes;
      sb->numSamples -= A52_FRAME_SIZE - fwav_n;
      fwav_n = 0;
    }
    if (sb->numSamples) {  // We still have some samples left
      memcpy(&fwav[fwav_n*sampleSize], inSamples, sb->numSamples * sampleSize);
      fwav_n += sb->numSamples;
      sb->numSamples = 0;
    }
 } while (!sb->lastBlock && !exitThread);

  if (fwav_n) {
    memset(&fwav[fwav_n*sampleSize], 0, (A52_FRAME_SIZE - fwav_n) * sampleSize);
    encodeBlock(fwav);
  }

	aften_encode_close(&aften);
  fclose(f);
  encodeThread = 0;
}

