#include "AnalyzeSound.h"
#include <math.h>
AnalyzeSound* out;

BOOL CALLBACK AnalyzeDialogProc(
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
          if (out->encodeThread) {
            if(IDNO == MessageBox(hwndDlg,"Are you sure you want to abort the conversion?","Abort?",MB_YESNO)) {
              return true;
            }
            out->quietExit = true;
            out->exitThread = true;
            int timeout = 50;  // 5 seconds
            while (out->encodeThread) {
              Sleep(100);
              if (!timeout--) {
                TerminateThread(out->encodeThread,0);
                out->encodeThread = 0;
              }
            }
            delete out;
          } else {
              delete out;
          }
          return true;

			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}

AnalyzeSound::AnalyzeSound(PClip _child, IScriptEnvironment* _env) : SoundOutput(ConvertAudio::Create(_child, SAMPLE_FLOAT,SAMPLE_FLOAT) ,_env)
{  
  params["filterID"] = AVSValue("analyze");
  params["showoutput"] = AVSValue(true);
  params["filterID"] = AVSValue("analyze");
  params["nofilename"] = AVSValue(true);
  params["showProgress"] = AVSValue(false);
  hStats = 0;
  canReplayGain = true;
  out = this;
  this->startEncoding();
}

AnalyzeSound::~AnalyzeSound(void)
{
  if (hStats)
    DestroyWindow(hStats);
  hStats = 0;
}

bool AnalyzeSound::initEncoder() {
  maximum = new float[vi.AudioChannels()];
  accumulated  = new double[vi.AudioChannels()];
  squared_accumulated  = new double[vi.AudioChannels()];

  for (int i = 0; i < vi.AudioChannels(); i++) {
    maximum[i] = 0.0f;
    accumulated[i] = 0.0;
    squared_accumulated[i] = 0.0;
  }
  if (params["showoutput"].AsBool()) {
	  hStats = CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_ANALYZESTATS),0,AnalyzeDialogProc);
    SendMessage(hStats,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
  }

  rGain = (replaygain_t**)malloc(sizeof(replaygain_t*)*vi.AudioChannels());
  for (int i = 0; i<vi.AudioChannels(); i++){
    rGain[i] = (replaygain_t*)malloc(sizeof(replaygain_t));
    if (INIT_GAIN_ANALYSIS_OK != InitGainAnalysis(rGain[i],vi.audio_samples_per_second))
      canReplayGain= false;
  }
  return true;
}

#define SCALE_TO_DB(scale) (20.0 * log10(scale))

void AnalyzeSound::updateStats(__int64 processed,__int64 total, bool force) {
  if (quietExit)
    return;
  if (GetTickCount() - lastUpdateTick < 100 && !force)
    return;

  this->updateSampleStats(processed, total, force);

  if (!hStats)
    return;

  int percent = (int)(processed * 100 / total);
  char buf[2000];
  sprintf_s(buf, 2000, "Analyzed %d%%.\r\n\r\n", percent);

  double nsamps = (double)processed;
  float all_maximum = 0.0f;
  double all_accumulated = 0.0;
  double all_squared_accumulated = 0.0;

  for (int i = 0; i < vi.AudioChannels(); i++) {
    double rms = squared_accumulated[i] / nsamps;
    rms = SCALE_TO_DB(sqrt(rms + 1e-10));
    double avg = SCALE_TO_DB(accumulated[i] / nsamps);
    float max = 20.0f * log10f(maximum[i]);

    char buf2[100];
    sprintf_s(buf2, 100, "[Ch %d] Maximum:%1.2fdB. Average:%1.2fdB. RMS:%1.2fdB. ",
      i, (float)max, (float)avg, (float)rms);
    strcat_s(buf, 2000, buf2);

    if (canReplayGain) {
      GetTitleGain(rGain[i]);
      sprintf_s(buf2, 100, "ReplayGain:%1.2fdB\r\n", GetAlbumGain(rGain[i]));      
      strcat_s(buf, 2000, buf2);
    } else {
      strcat_s(buf, 2000, "\r\n");
    }
    // Add to all channel stats
    all_squared_accumulated += squared_accumulated[i];
    all_accumulated += accumulated[i];
    if (maximum[i] > all_maximum)
      all_maximum = maximum[i];
  }
  
  double rmsA = all_squared_accumulated / (nsamps*vi.AudioChannels());
  rmsA = SCALE_TO_DB(sqrt(rmsA + 1e-10));
  double avgA = SCALE_TO_DB(all_accumulated / (nsamps*vi.AudioChannels()));
  float maxA = 20.0f * log10f(all_maximum);

  char buf2[100];
  sprintf_s(buf2, 100, "\r\n[All channels] Maximum:%1.2fdB. Average:%1.2fdB. RMS:%1.2fdB\r\n",
      (float)maxA, (float)avgA, (float)rmsA);
    strcat_s(buf, 2000, buf2);

  if (!exitThread) {
    SetDlgItemText(hStats,IDC_ANALYZETEXT,buf);
    this->updatePercent(percent);
  }
  lastUpdateTick = GetTickCount();
}

void AnalyzeSound::encodeLoop() {
  __int64 encodedSamples = 0;
  SampleBlock* sb;
  int channels = vi.AudioChannels();
  do {
    sb = input->GetNextBlock();
    SFLOAT* samples = (SFLOAT*)sb->getSamples();
    for (int n = 0; n < sb->numSamples; n++) {
      for (int ch = 0; ch < channels; ch++) {
        float sample = fabs(*samples++);
        if (sample > maximum[ch]) 
        maximum[ch] = sample;
        accumulated[ch] += sample;
        squared_accumulated[ch] += sample * sample;
      }
    }
    if (canReplayGain) {
      SFLOAT* s = (SFLOAT*)malloc(sb->numSamples*sizeof(SFLOAT));
      for (int ch = 0; ch < channels; ch++) {
        samples = (SFLOAT*)sb->getSamples();
        samples = &samples[ch];
        for (int n = 0; n < sb->numSamples; n++) {
          s[n] = samples[n*channels] * 32768.0f;  // Why, LAME, why?
        }
        AnalyzeSamples(rGain[ch], s,s,sb->numSamples,1);
      }
      free(s);
    }
    encodedSamples += sb->numSamples;
    this->updateStats(encodedSamples, vi.num_audio_samples, false);
  } while (!sb->lastBlock && !exitThread);

 this->updateStats(encodedSamples, vi.num_audio_samples, true);
 if (canReplayGain) {
    for (int i = 0; i<vi.AudioChannels(); i++){ 
      free(rGain[i]);
    }
 }
 free(rGain);
 encodeThread = 0;
}