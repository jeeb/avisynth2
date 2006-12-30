#include "WavOutput.h"

WavOutput* out;

const char * const WAV_TypeString[] = {
  "Microsoft WAV", "MS WAVE with WAVEFORMATEX", "Apple/SGI AIFF", "Sun/NeXT AU", "RAW PCM", "S.F. WAVE64", "Core Audio File" };

const int WAV_TypeVal[] = {
  SF_FORMAT_WAV,    SF_FORMAT_WAVEX,             SF_FORMAT_AIFF,  SF_FORMAT_AU, SF_FORMAT_RAW,SF_FORMAT_W64,SF_FORMAT_CAF };

const char * const WAV_ExtString[] = {
  "Microsoft WAV (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0", "Microsoft WAV (*.wav))\0*.wav\0All Files (*.*)\0*.*\0\0",    "Apple/SGI AIFF (*.aif))\0*.aif;*.aiff\0All Files (*.*)\0*.*\0\0", "Sun/NeXT AU (*.au))\0*.au\0All Files (*.*)\0*.*\0\0", "RAW PCM (*.pcm))\0*.pcm;*.raw\0All Files (*.*)\0*.*\0\0", "S.F. WAVE64 (*.W64))\0*.w64\0All Files (*.*)\0*.*\0\0", "Core Audio File (*.caf))\0*.caf\0All Files (*.*)\0*.*\0\0" };


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
          out->outputFileFilter = WAV_ExtString[SendDlgItemMessage(hwndDlg, IDC_WAVTYPE, CB_GETCURSEL,0,0)];
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



WavOutput::WavOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_INT16|SAMPLE_INT24|SAMPLE_INT32|SAMPLE_FLOAT,SAMPLE_INT16),_env)
{
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_WAVSETTINGS),0,WavDialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  for (int i = 0; i< 7; i++) {
     SendDlgItemMessage(wnd, IDC_WAVTYPE, CB_ADDSTRING, 0, (LPARAM)WAV_TypeString[i]);
  }
  SendDlgItemMessage(wnd, IDC_WAVTYPE,CB_SETCURSEL,0,0);  // TODO: Registry
}

WavOutput::~WavOutput(void)
{
}

bool WavOutput::initEncoder() {
  info.channels = vi.AudioChannels();
  info.frames = vi.num_audio_samples;
  info.samplerate = vi.audio_samples_per_second;
  int format = WAV_TypeVal[SendDlgItemMessage(wnd, IDC_WAVTYPE, CB_GETCURSEL,0,0)];
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
  return true;
}

void WavOutput::encodeLoop() {
  SampleBlock* sb = input->GetNextBlock();
  __int64 encodedSamples = 0;
  while (!sb->lastBlock && !exitThread) {
    encodedSamples += sb->numSamples;
    this->updateSampleStats(encodedSamples / 1000, vi.num_audio_samples / 1000);
    sf_write_raw(sndfile, sb->getSamples(), vi.BytesFromAudioSamples(sb->numSamples));
    delete sb;
    if (input)
      sb = input->GetNextBlock();
  }
	if (sf_close(sndfile)) {
    MessageBox(NULL,"An encoder error occured while finalizing WAV output. Output file may not work","WAVE Encoder",MB_OK);
	}
  encodeThread = 0;
}