#include "VorbisOutput.h"
VorbisOutput* out;

BOOL CALLBACK VorbisDialogProc(
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
        case IDC_VORBISSAVE:          
          out->startEncoding();
          return true;
        case IDC_VORBISCANCEL:
          delete out;
          return true;
			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}

VorbisOutput::VorbisOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(_child ,_env)
{
  params["outputFileFilter"] = AVSValue("OGG files\0*.ogg\0All Files\0*.*\0\0");
  params["extension"] = AVSValue(".ogg");
  params["filterID"] = AVSValue("vorbis");
  params["vbrbitrate"] = AVSValue(128);
}

VorbisOutput::~VorbisOutput(void)
{
}

void VorbisOutput::showGUI() {
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_VORBISSETTINGS),0,VorbisDialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
}

bool VorbisOutput::setParamsToGUI() {
  char buf[100];
  sprintf_s(buf,sizeof(buf),"%u",params["vbrbitrate"].AsInt());
  SetDlgItemText(wnd,IDC_VORBISBITRATE,buf);
  SendDlgItemMessage(wnd, IDC_VORBISBITRATEUPDOWN, UDM_SETRANGE, 0, MAKELONG (1023, 0));
  return true;
}

bool VorbisOutput::getParamsFromGUI() {
  return true;
}
bool VorbisOutput::initEncoder() {
/*	// set the input WAV format
  WAVEFORMATEX wfeAudioFormat; 
  FillWaveFormatEx(&wfeAudioFormat, vi.audio_samples_per_second, vi.BytesPerChannelSample()*8, vi.AudioChannels());
  pAPECompress = CreateIAPECompress();

  str_utf16 theFile[MAX_PATH+1];
  memset(theFile,0,sizeof(theFile));
  MultiByteToWideChar(CP_ACP,0,outputFile,(int)strlen(outputFile),theFile,MAX_PATH+1);

  int level = (unsigned int)SendDlgItemMessage(wnd, IDC_MAC_COMPRESSIONLEVEL, TBM_GETPOS, 0, 0)*1000;

	// start the encoder
  int nRetVal = pAPECompress->Start(theFile, &wfeAudioFormat, vi.BytesFromAudioSamples(vi.num_audio_samples), 
		level, NULL, CREATE_WAV_HEADER_ON_DECOMPRESSION);

	if (nRetVal != 0)
	{
    char buf[800];
    sprintf_s(buf, 800, "Monkey Audio Initialization Error: %s", FindExplanation(nRetVal));
    MessageBox(NULL,buf,"Monkey Audio Encoder",MB_OK);
    
		SAFE_DELETE(pAPECompress)
		return false;
	}
*/
  return true;
}

void VorbisOutput::encodeLoop() {
/*  FILE *f;
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

  int bytesReady = twolame_encode_flush(encodeOptions, outbuffer, BLOCKSAMPLES);
  fwrite(outbuffer, bytesReady, 1, f);
  fclose(f);
  twolame_close(&encodeOptions);
*/
}
