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

VorbisOutput::VorbisOutput(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_FLOAT,SAMPLE_FLOAT) ,_env)
{
  params["outputFileFilter"] = AVSValue("OGG files\0*.ogg\0All Files\0*.*\0\0");
  params["extension"] = AVSValue(".ogg");
  params["filterID"] = AVSValue("vorbis");
  params["vbrbitrate"] = AVSValue(128);
  params["cbr"] = AVSValue(false);
  delete input;
  input = new SampleFetcher(child, env, 51234);
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
  CheckDlgButton(wnd, IDC_VORBISCBR, params["cbr"].AsBool());
  return true;
}

bool VorbisOutput::getParamsFromGUI() {
  char buf[100];
  ((short*)buf)[0] = 100;
  SendDlgItemMessage(wnd, IDC_VORBISBITRATE, EM_GETLINE,0,(LPARAM)buf);
  int n = atoi(buf);
  if (n<16) {
    MessageBox(NULL,"Bitrate must be 16kbit or more per second.","Vorbis Encoder",MB_OK);
    return false;
  }
  params["vbrbitrate"] = AVSValue(n);  
  params["cbr"] = AVSValue(!!IsDlgButtonChecked(wnd,IDC_VORBISCBR));
  return true;
}
bool VorbisOutput::initEncoder() {
  vorbis_info_init(&vorbis);
  int br = params["vbrbitrate"].AsInt()*1000;
  int min_max = params["cbr"].AsBool() ? br : -1;
  if (vorbis_encode_init(&vorbis,vi.AudioChannels(),vi.audio_samples_per_second, min_max, br, min_max)) {
    MessageBox(NULL,"An encoder error occured while initializing encoder","Vorbis Encoder",MB_OK);
    return false;
  }
  if (fopen_s(&f, outputFile, "wbS")) {
    MessageBox(NULL,"Could not open file for writing","Vorbis Encoder",MB_OK);
    return false;
  }

  return true;
}

void VorbisOutput::encodeLoop() {
  ogg_stream_state os; /* take physical pages, weld into a logical
                       stream of packets */
  ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */

  vorbis_comment   vc; /* struct that stores all the user comments */

  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */

  /* add a comment */
  vorbis_comment_init(&vc);
  vorbis_comment_add(&vc,"Track encoded by SoundOut/AviSynth, by Klaus Post.");
  memset(&op,0, sizeof(op));

  /* set up the analysis state and auxiliary encoding storage */
  if (vorbis_analysis_init(&vd,&vorbis)) 
    setError("An encoder error occured while initializing analysis for encoder");
  
  if (vorbis_block_init(&vd,&vb))
    setError("An encoder error occured while initializing block storage for encoder");

  /* set up our packet->stream encoder */
  /* pick a random serial number; that way we can more likely build
  chained streams just by concatenation */
  srand(GetTickCount());
  if (ogg_stream_init(&os,rand())) 
    setError("An encoder error occured while initializing stream for encoder");

  /* Vorbis streams begin with three headers; the initial header (with
  most of the codec setup parameters) which is mandated by the Ogg
  bitstream spec.  The second header holds any comment fields.  The
  third header holds the bitstream codebook.  We merely need to
  make the headers, then pass them to libvorbis one at a time;
  libvorbis handles the additional Ogg bitstream constraints */

  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;

  if (vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code))
    setError("An encoder error occured while initializing header for encoder");

  ogg_stream_packetin(&os,&header); /* automatically placed in its own page */
  ogg_stream_packetin(&os,&header_comm);
  ogg_stream_packetin(&os,&header_code);

  while(!exitThread){
    int result=ogg_stream_flush(&os,&og);
    if(result==0) break;
    fwrite(og.header,1,og.header_len,f);
    fwrite(og.body,1,og.body_len,f);
  }

  __int64 encodedBytes = 0;
  __int64 encodedSamples = 0;
  SampleBlock* sb;
  do {
    sb = input->GetNextBlock();
    float **buffer=vorbis_analysis_buffer(&vd,sb->numSamples);
    if (vi.AudioChannels()>1) {
      SFLOAT *samples = (SFLOAT*)sb->getSamples();
      int chans = vi.AudioChannels();
      for (int i=0; i<sb->numSamples; i++) {
        for (int ch=0; ch <chans ;ch++) {
          buffer[ch][i] = *samples++;
        }
      }
    } else {
      memcpy(&buffer[0][0], sb->getSamples(), (int)vi.BytesFromAudioSamples(sb->numSamples));
    }

    /* tell the library how much we actually submitted */
    if (vorbis_analysis_wrote(&vd,sb->numSamples))
        setError("An encoder error occured while writing analyzing data");

    if (sb->lastBlock)  
      vorbis_analysis_wrote(&vd,0);

    /* vorbis does some data preanalysis, then divvies up blocks for
    more involved (potentially parallel) processing.  Get a single
    block for encoding now */
    while(vorbis_analysis_blockout(&vd,&vb)==1){

      /* analysis, assume we want to use bitrate management */
      if (vorbis_analysis(&vb,NULL))
        setError("An encoder error occured while analyzing data");

      if(vorbis_bitrate_addblock(&vb)) {
        setError("An encoder error occured while adding data block");
      }
      while(vorbis_bitrate_flushpacket(&vd,&op)){

        /* weld the packet into the bitstream */
        if (ogg_stream_packetin(&os,&op))
          setError("An encoder error occured while weliding data into the bitstream");

        /* write out pages (if any) */
        while(ogg_stream_pageout(&os,&og)){        
          fwrite(og.header,1,og.header_len,f);
          fwrite(og.body,1,og.body_len,f);
          if(ogg_page_eos(&og)) {
//          exitThread = true;
            break;
          }
        }
      }
    }
    encodedSamples += sb->numSamples;
    this->updateSampleStats(encodedSamples, vi.num_audio_samples);
  } while (!sb->lastBlock && !exitThread);
  this->updateSampleStats(encodedSamples, vi.num_audio_samples, true);

  /* clean up and exit.  vorbis_info_clear() must be called last */
  
  ogg_stream_clear(&os);
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_info_clear(&vorbis);
  vorbis_comment_clear(&vc);
  fclose(f);
}