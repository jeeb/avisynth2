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

FlacOutput::FlacOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_INT8|SAMPLE_INT16|SAMPLE_INT24,SAMPLE_INT24),_env)
{
  params["outputFileFilter"] = AVSValue("FLAC files\0*.flac\0All Files\0*.*\0\0");
  params["extension"] = AVSValue(".flac");
  params["filterID"] = AVSValue("flac");
  params["compressionlevel"] = AVSValue(6);
}

FlacOutput::~FlacOutput(void)
{
}

void FlacOutput::showGUI() {
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_FLACSETTINGS),0,FlacDialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
  SendDlgItemMessage(wnd, IDC_FLAC_COMPRESSIONLEVEL, TBM_SETTICFREQ, 1, 0);
  SendDlgItemMessage(wnd, IDC_FLAC_COMPRESSIONLEVEL, TBM_SETRANGE, TRUE, MAKELONG (1, 8));
	ShowWindow(wnd,SW_NORMAL);
  exitThread = true;
}

bool FlacOutput::getParamsFromGUI() {
  int c =(int)SendDlgItemMessage(wnd, IDC_FLAC_COMPRESSIONLEVEL, TBM_GETPOS, 0, 0);
  params["compressionlevel"] = AVSValue(c);
  return true;
}

bool FlacOutput::setParamsToGUI() {
  SendDlgItemMessage(wnd, IDC_FLAC_COMPRESSIONLEVEL, TBM_SETPOS, TRUE, params["compressionlevel"].AsInt());
  return true;
}



bool FlacOutput::initEncoder() {
  encoder = new FlacOutEncoder(this);
  encoder->set_channels(vi.AudioChannels());
  encoder->set_bits_per_sample(vi.BytesPerChannelSample()*8);  
  encoder->set_sample_rate(vi.audio_samples_per_second);
  encoder->set_compression_level(params["compressionlevel"].AsInt());
  encoder->set_total_samples_estimate(vi.num_audio_samples);

  if (fopen_s(&f, outputFile, "wbS")) {
    MessageBox(NULL,"Could not open file for writing","FLAC Encoder",MB_OK);
    return false;
  }

  if (!encoder->init(f)) {
    if (encoder->get_state() != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
      char buf[800];
      sprintf_s(buf, 800, "FLAC Initialization error: %s", _FLAC__StreamEncoderInitStatusString[encoder->get_state()]);
      MessageBox(NULL,buf,"FLAC Encoder",MB_OK);
      return false;
    }     
  }
  return true;
}

void FlacOutput::encodeLoop() {
  SampleBlock* sb;
  __int64 encodedSamples = 0;
  do {
    sb = input->GetNextBlock();
    int totSmps = sb->numSamples*vi.AudioChannels();
    int* dataArray=new int[totSmps];
    if (vi.IsSampleType(SAMPLE_INT24)) { // FLAC has very stupid idea that all samples should be passed as ints.
      unsigned char* samples = (unsigned char*)sb->getSamples();
      for (int i = 0; i<totSmps ; i++) { 
        //dataArray[i] = (int)(samples[i]*(float)(1<<23));
        dataArray[i] =  samples [i*3] | (((int) samples [i*3+1]) << 8) | (((int)(signed char) samples [i*3+2]) << 16);
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
    if (!encoder->process_interleaved(dataArray, sb->numSamples)) {
      FLAC::Encoder::File::State state = encoder->get_state(); 
      if (state != FLAC__STREAM_ENCODER_OK) {
        sb->lastBlock = true;
        char buf[800];
        sprintf_s(buf, 800, "FLAC Encoder error: %s", state.as_cstring());
        MessageBox(NULL,buf,"FLAC Encoder",MB_OK);
      }      
    } else {
      encodedSamples += sb->numSamples;
    }
    delete dataArray;
  } while (!sb->lastBlock && !exitThread);
  encoder->finish();
  delete encoder;
  this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);
  fclose(f);
  encodeThread = 0;
}


void FlacOutEncoder::progress_callback (FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate) {
  parent->updateSampleStats(samples_written, parent->GetVideoInfo().num_audio_samples);
}

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
