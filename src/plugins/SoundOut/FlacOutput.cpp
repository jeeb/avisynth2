#include "FlacOutput.h"

FlacOutput* out;

BOOL CALLBACK FlacDialogProc(
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
        case IDC_FLAC_SAVE:          
          out->startEncoding();
          return true;
        case IDC_FLAC_CANCEL:
          delete out;
          return true;
			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}

FlacOutput::FlacOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_INT8|SAMPLE_INT16|SAMPLE_FLOAT,SAMPLE_FLOAT),_env)
{
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_FLACSETTINGS),0,FlacDialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  SendDlgItemMessage(wnd, IDC_FLAC_COMPRESSIONLEVEL, TBM_SETTICFREQ, 1, 0);
  SendDlgItemMessage(wnd, IDC_FLAC_COMPRESSIONLEVEL, TBM_SETRANGE, TRUE, MAKELONG (1, 8));
  SendDlgItemMessage(wnd, IDC_FLAC_COMPRESSIONLEVEL, TBM_SETPOS, TRUE, 5);
  outputFileFilter = "FLAC files\0*.flac\0All Files\0*.*\0\0";
}

FlacOutput::~FlacOutput(void)
{
  exitThread = true;
}

bool FlacOutput::initEncoder() {
  set_channels(vi.AudioChannels());
  set_bits_per_sample(vi.BytesPerChannelSample() == 4 ? 24 : vi.BytesPerChannelSample()*8);  
  set_sample_rate(vi.audio_samples_per_second);
  set_compression_level((unsigned int)SendDlgItemMessage(wnd, IDC_FLAC_COMPRESSIONLEVEL, TBM_GETPOS, 0, 0));
  set_total_samples_estimate(vi.num_audio_samples);
  init(outputFile);
  if (get_state() != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
      char buf[800];
      sprintf_s(buf, 800, "FLAC Initialization error: %s", _FLAC__StreamEncoderInitStatusString[get_state()]);
      MessageBox(NULL,buf,"FLAC Encoder",MB_OK);
      return false;
  }      
  return true;
}

void FlacOutput::encodeLoop() {
  SampleBlock* sb = input->GetNextBlock();
  while (!sb->lastBlock && !exitThread) {
    int totSmps = sb->numSamples*vi.AudioChannels();
    int* dataArray=new int[totSmps];
    if (vi.IsSampleType(SAMPLE_FLOAT)) { // FLAC has very stupid idea that all samples should be passed as ints.
      float* samples = (float*)sb->getSamples();
      for (int i = 0; i<totSmps ; i++) { 
        dataArray[i] = (int)(samples[i]*(float)(1<<23));
      }
    } else if (vi.IsSampleType(SAMPLE_INT16)) {
      signed short* samples = (signed short*)sb->getSamples();
      for (int i = 0; i<totSmps ; i++) { 
        dataArray[i]=samples[i];
      }
    } else if (vi.IsSampleType(SAMPLE_INT8)) {
      unsigned char* samples = (unsigned char*)sb->getSamples();
      for (int i = 0; i<totSmps ; i++) { 
        dataArray[i]=((int)samples[i]-128);
      }
    }
    process_interleaved(dataArray, sb->numSamples);
    delete dataArray;
    delete sb;
    if (input)
      sb = input->GetNextBlock();

    if (get_state() != FLAC__STREAM_ENCODER_OK) {
      sb->lastBlock = true;
      char buf[800];
      sprintf_s(buf, 800, "FLAC Encoder error: %s", _FLAC__StreamEncoderStateString[get_state()]);
      MessageBox(NULL,buf,"FLAC Encoder",MB_OK);
    }      
  }
  finish();
  encodeThread = 0;
}

void 	FlacOutput::progress_callback (FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate) {
  if (GetTickCount() - lastUpdateTick < 100)
    return;

  lastUpdateTick = GetTickCount();
  int percent = (int)(samples_written * 100 / vi.num_audio_samples);
  int compression = (int)((vi.BytesFromAudioSamples(samples_written) - bytes_written ) * 100 / vi.BytesFromAudioSamples(samples_written));
  const char* tw_m;
  int totalWritten = ConvertDataSizes(bytes_written, &tw_m);
  char buf[800];
  sprintf_s(buf, 800, "Written %dk of %dk samples (%d%%)\n"
    "Written %d%s data. Compression: %d%%.",
    (int)(samples_written), (int)(vi.num_audio_samples), percent,
    totalWritten, tw_m, compression);

  if (!exitThread) {
    SetDlgItemText(wnd,IDC_STC_CONVERTMSG,buf);
    this->updatePercent(percent);
  }

}

const char * const _FLAC__StreamEncoderStateString[] = {
	"FLAC__STREAM_ENCODER_OK",
	"FLAC__STREAM_ENCODER_UNINITIALIZED",
	"FLAC__STREAM_ENCODER_OGG_ERROR",
	"FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR",
	"FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA",
	"FLAC__STREAM_ENCODER_CLIENT_ERROR",
	"FLAC__STREAM_ENCODER_IO_ERROR",
	"FLAC__STREAM_ENCODER_FRAMING_ERROR",
	"FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR"
}; 

const char * const _FLAC__StreamEncoderInitStatusString[] = {
	"FLAC__STREAM_ENCODER_INIT_STATUS_OK",
	"FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR",
	"FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER",
	"FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS",
	"FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS",
	"FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE",
	"FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE",
	"FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE",
	"FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER",
	"FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION",
	"FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER",
	"FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE",
	"FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA",
	"FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED"
}; 