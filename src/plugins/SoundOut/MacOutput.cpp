#include "MacOutput.h"
MacOutput* out;

BOOL CALLBACK MacDialogProc(
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
        case IDC_MAC_SAVE:          
          out->startEncoding();
          return true;
        case IDC_MAC_CANCEL:
          delete out;
          return true;
			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}

struct {
	int errorcode;
	const char *text;
} error_lookup[] = { ERROR_EXPLANATION };

const char* FindExplanation(int ApeError) {
  int i = 0;
  while (true) {
    if (error_lookup[i].errorcode == ApeError)
      return error_lookup[i].text;
    if (error_lookup[i].errorcode == -1)
      return error_lookup[i].text;
  }
}
MacOutput::MacOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_INT8|SAMPLE_INT16|SAMPLE_INT24,SAMPLE_INT24),_env)
{
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_MACSETTINGS),0,MacDialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  SendDlgItemMessage(wnd, IDC_MAC_COMPRESSIONLEVEL, TBM_SETTICFREQ, 1, 0);
  SendDlgItemMessage(wnd, IDC_MAC_COMPRESSIONLEVEL, TBM_SETRANGE, TRUE, MAKELONG (1, 5));
  params["filterID"] = AVSValue("macout");
  params["outputFileFilter"] = AVSValue("APE files (*.ape)\0*.ape\0All Files (*.*)\0*.*\0\0");
  params["extension"] = AVSValue(".ape");
  params["compressionlevel"] = AVSValue(3);
  setParamsToGUI();
}

MacOutput::~MacOutput(void)
{
}

bool MacOutput::getParamsFromGUI() {
  int c =(int)SendDlgItemMessage(wnd, IDC_MAC_COMPRESSIONLEVEL, TBM_GETPOS, 0, 0);
  params["compressionlevel"] = AVSValue(c);
  return true;
}

bool MacOutput::setParamsToGUI() {
  SendDlgItemMessage(wnd, IDC_MAC_COMPRESSIONLEVEL, TBM_SETPOS, TRUE, params["compressionlevel"].AsInt());
  return true;
}

bool MacOutput::initEncoder() {
	// set the input WAV format
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

  return true;
}

void MacOutput::encodeLoop() {
  __int64 encodedSamples = 0;
  SampleBlock* sb;
  do {
    sb = input->GetNextBlock();
    pAPECompress->AddData((unsigned char*)sb->getSamples(), (int)vi.BytesFromAudioSamples(sb->numSamples));

    encodedSamples += sb->numSamples;
    this->updateSampleStats(encodedSamples, vi.num_audio_samples);

  } while (!sb->lastBlock && !exitThread);

	if (pAPECompress->Finish(NULL, 0, 0) != 0) {
    MessageBox(NULL,"An encoder error occured while finalizing APE Compression. Output file may not work","Monkey Audio Encoder",MB_OK);
	}
  SAFE_DELETE(pAPECompress)

  encodeThread = 0;
}
  