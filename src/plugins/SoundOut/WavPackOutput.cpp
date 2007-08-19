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

#include "WavPackOutput.h"
#include <time.h>
#include <vfw.h>

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif

WavPackOutput* out;



const char * const WAVPACK_FormatString[] = {
  "8 bit", "16 Bit", "24 Bit", "32 Bit", "Float"};

const int WAVPACK_FormatVal[] = {
  SAMPLE_INT8, SAMPLE_INT16, SAMPLE_INT24, SAMPLE_INT32, SAMPLE_FLOAT };

  const char * const WAVPACK_CompressionString[] = {
  "Very Fast", "Fast", "Normal", "Slow", "Very Slow","Extremely slow"};

BOOL CALLBACK WavPackDialogProc(
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
        case IDC_WAVPACKSAVE:
          out->startEncoding();
          return true;
        case IDC_WAVPACKCANCEL:
          delete out;
          return true;
			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}

int DoWriteFile (FILE *hFile, void *lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t *lpNumberOfBytesWritten)
{
  uint32_t bcount;

  *lpNumberOfBytesWritten = 0;

  while (nNumberOfBytesToWrite) {
    bcount = (uint32_t) fwrite ((uchar *) lpBuffer + *lpNumberOfBytesWritten, 1, nNumberOfBytesToWrite, hFile);

    if (bcount) {
      *lpNumberOfBytesWritten += bcount;
      nNumberOfBytesToWrite -= bcount;
    }
    else
      break;
  }

  return !ferror (hFile);
} 


static int write_block (void *id, void *data, int32_t length)
{ 
  WavPackOutput *t =  (WavPackOutput*)id;
  uint32_t bcount; 
  if (t && t->f && data && length) {
     if (!DoWriteFile (t->f, data, length, &bcount) || bcount != length) {
        t->setError("WavPack Output: Error Occured while writing to file");
        return FALSE;
      } else {
        t->bytes_written += length;

     }
    }

    return TRUE; 
}

WavPackOutput::WavPackOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(_child,_env)
{
  params["outputFileFilter"] = AVSValue("WV files\0*.wv\0All Files\0*.*\0\0");
  params["extension"] = AVSValue(".wv");
  params["filterID"] = AVSValue("wavpack");
  params["format"] = AVSValue(0);
  params["compressionlevel"] = AVSValue(2);

  for (int i = 0; i< 5; i++) {
     if (vi.IsSampleType(WAVPACK_FormatVal[i]))
       params["format"] = AVSValue(i);
  }
  f = NULL;
}

WavPackOutput::~WavPackOutput(void)
{
}

void WavPackOutput::showGUI() {
  out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_WAVPACKSETTINGS),0,WavPackDialogProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
	ShowWindow(wnd,SW_NORMAL);
  for (int i = 0; i< 6; i++) {
     SendDlgItemMessage(wnd, IDC_WAVPACKCOMPRESSION, CB_ADDSTRING, 0, (LPARAM)WAVPACK_CompressionString[i]);
  }
  for (int i = 0; i< 5; i++) {
     SendDlgItemMessage(wnd, IDC_WAVPACKFORMAT, CB_ADDSTRING, 0, (LPARAM)WAVPACK_FormatString[i]);
     if (vi.IsSampleType(WAVPACK_FormatVal[i]))
       params["format"] = AVSValue(i);
  }
}

bool WavPackOutput::setParamsToGUI() {
  SendDlgItemMessage(wnd, IDC_WAVPACKFORMAT,CB_SETCURSEL,params["format"].AsInt(),0);
  SendDlgItemMessage(wnd, IDC_WAVPACKCOMPRESSION,CB_SETCURSEL,params["compressionlevel"].AsInt(),0);
  return true;
}

bool WavPackOutput::getParamsFromGUI() {
  params["format"] = AVSValue((int)SendDlgItemMessage(wnd, IDC_WAVPACKFORMAT, CB_GETCURSEL,0,0));
  params["compressionlevel"] = AVSValue((int)SendDlgItemMessage(wnd, IDC_WAVPACKCOMPRESSION, CB_GETCURSEL,0,0));
  return true;
}

bool WavPackOutput::initEncoder() {
  wpc = WavpackOpenFileOutput (write_block,this, NULL);  // No hybrid encoding
  int sample_format = WAVPACK_FormatVal[params["format"].AsInt()];
  memset(&config,0,sizeof(config));

  config.sample_rate = vi.audio_samples_per_second;
  config.num_channels = vi.AudioChannels();

  if (sample_format == SAMPLE_FLOAT ) {
    if (!vi.IsSampleType(sample_format)) {
      IScriptEnvironment* env = input->env;

      if (input)
        delete input;

      input = new SampleFetcher(ConvertAudio::Create(child,sample_format,sample_format), env);
      vi = input->child->GetVideoInfo();    
    }
    config.bits_per_sample = 32;
    config.bytes_per_sample = 4;
    config.float_norm_exp = 127;

  } else {  // Integer
    switch (sample_format) {
      case SAMPLE_INT8:
        config.bits_per_sample = 8;
        config.bytes_per_sample = 1;
        break;
      case SAMPLE_INT16:
        config.bits_per_sample = 16;
        config.bytes_per_sample = 2;
        break;
      case SAMPLE_INT24:
        config.bits_per_sample = 24;
        config.bytes_per_sample = 3;
        break;
      case SAMPLE_INT32:
        config.bits_per_sample = 32;
        config.bytes_per_sample = 4;
        break;
    }

    if (!vi.IsSampleType(sample_format) ) {
      IScriptEnvironment* env = input->env;

      if (input)
        delete input;

      input = new SampleFetcher(ConvertAudio::Create(child,sample_format,sample_format), env);
      vi = input->child->GetVideoInfo();    
    }

  }
	DWORD dwChannelMask = 0;  // Default
  switch (vi.AudioChannels()) {
      case 1 :	/* center channel mono */
				dwChannelMask = SPEAKER_FRONT_CENTER;
				break ;

			case 2 :	/* front left and right */
        dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
				break ;

			case 3 :	/* front left and right */
        dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT;
				break ;

			case 4 :	/* Quad */
				dwChannelMask = SPEAKER_FRONT_LEFT |SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT;
				break ;

			case 5 :	/* 3/2 */
        dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT;
				break ;

			case 6 :	/* 5.1 */
        dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY;
				break ;

			case 8 :	/* 7.1 */
        dwChannelMask = SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER |SPEAKER_FRONT_LEFT |SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT | SPEAKER_LOW_FREQUENCY;
        break ;
	} 	
  config.channel_mask = dwChannelMask;
  switch (params["compressionlevel"].AsInt()) {
    case 0:
      config.flags |= CONFIG_FAST_FLAG | CONFIG_EXTRA_MODE;
      config.xmode = 0;
      break;
    case 1:
      config.flags |= CONFIG_FAST_FLAG | CONFIG_EXTRA_MODE;
      config.xmode = 1;
      break;
    case 3:
      config.flags |= CONFIG_HIGH_FLAG | CONFIG_EXTRA_MODE;
      config.xmode = 1;
      break;
    case 4:
      config.flags |= CONFIG_VERY_HIGH_FLAG | CONFIG_EXTRA_MODE;
      config.xmode = 2;
      break;
    case 5:
      config.flags |= CONFIG_VERY_HIGH_FLAG | CONFIG_EXTRA_MODE ;
      config.xmode = 6;
      break;
    default:
      break;
  }
  if (fopen_s(&f, outputFile, "wbS")) {
    MessageBox(NULL,"Could not open file for writing","WavPack Encoder",MB_OK);
    return false;
  }

  if (!WavpackSetConfiguration(wpc, &config, (unsigned int)vi.num_audio_samples)) {
    MessageBox(NULL,"Configuration not accepted.","WacPack Encoder",MB_OK);
    fclose(f);
    return false;
  }
  if (!WavpackPackInit(wpc)) {
    MessageBox(NULL,"Could not initialize WacPack packer.","WacPack Encoder",MB_OK);
    fclose(f);
    return false;
  }

  return true;
}

void WavPackOutput::encodeLoop() {
  __int64 encodedSamples = 0;
  SampleBlock* sb;
  do {
    sb = input->GetNextBlock();
    if (config.bytes_per_sample == 4) { // No conversion
      if (!WavpackPackSamples(wpc, (int*)sb->getSamples(), sb->numSamples)) {
        this->setError("Error occured while encoding WavPack output. Output file may not work.");
        exitThread = true;
      }
    } else {  // Convert
      int totalSamples = sb->numSamples * vi.AudioChannels();
      int* buffer = (int*)malloc(totalSamples * sizeof(int));
      switch(vi.sample_type) {
        case SAMPLE_INT8: {
          unsigned char* input = (unsigned char*)sb->getSamples();
          for (int i = 0; i < totalSamples; i++) 
            buffer[i] = input[i] - 128;          
          break;
        }

        case SAMPLE_INT16: {
          short* input = (short*)sb->getSamples();
          for (int i = 0; i < totalSamples; i++) 
            buffer[i] = input[i];          
          break;
        }

        case SAMPLE_INT24: {
          unsigned char* input = (unsigned char*)sb->getSamples();
          for (int i = 0; i < totalSamples; i++) {
            buffer[i] =  input [i*3] | (((int32_t) input [i*3+1]) << 8) | (((int32_t)(signed char) input [i*3+2]) << 16);           }
          break;
        }
      }
      if (!WavpackPackSamples(wpc, buffer, sb->numSamples)) {
        this->setError("Error occured while encoding WavPack output. Output file may not work.");
        exitThread = true;
      }
      free(buffer);
    }
    encodedSamples += sb->numSamples;
    this->updateSampleStats(encodedSamples, vi.num_audio_samples);
  } while (!sb->lastBlock && !exitThread);

  if (!WavpackFlushSamples(wpc)) {
    MessageBox(NULL,"An encoder error occured while finalizing WavPack output.","WavPack Encoder",MB_OK);
  }

  WavpackCloseFile(wpc);
	fclose(f);
  this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);
  encodeThread = 0;
}
