#include "SoundOutput.h"
#include "Commdlg.h"

SoundOutput* so_out;

BOOL CALLBACK ConvertProgressProc(
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
          delete so_out;
          return true;
        case IDC_BTN_CONVERTABORT:
          if (so_out->encodeThread) {
            if(IDYES == MessageBox(hwndDlg,"Are you sure you want to abort the conversion?","Abort?",MB_YESNO)) {
              so_out->exitThread = true;
            }
          } else {
              delete so_out;
          }
          return true;
			}
			break;

		case WM_INITDIALOG:
			return true;
	}
  return false;
}

DWORD WINAPI StartEncoder(LPVOID p) {
  SoundOutput* t = (SoundOutput*)p;
  try {
    t->encodeLoop();
    t->encodingFinished();
  } catch (...) {
    t->setError("Unhandled Exception in encoder main loop");
  }
  return 0;
}

SoundOutput::SoundOutput(PClip _child, IScriptEnvironment* _env) : 
  GenericVideoFilter(_child), wnd(0), env(_env), encodeThread(0), outputFile(0)
{
  input = new SampleFetcher(child, env);
  lastUpdateTick = GetTickCount();
}

SoundOutput::~SoundOutput(void)
{
  if (input)
    delete input;
  input = NULL;

  exitThread = true;
  while (encodeThread) {  // Wait for thread to exit.
    Sleep(100);
  }

  if (wnd)
    DestroyWindow(wnd);
  wnd = NULL;

  if (outputFile)
    free(outputFile);

  outputFile = 0;
  
}

void SoundOutput::startEncoding() {
  if (!this->getParamsFromGUI()) {
    return;
  }
  char szFile[MAX_PATH+1];
	szFile[0] = 0;

  char CurrentDir[MAX_PATH];
	GetCurrentDirectory( MAX_PATH, CurrentDir );

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.hInstance = g_hInst;
  ofn.lpstrTitle = "Select where to save file..";
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFilter = params["outputFileFilter"].AsString("All Files (*.*)\0*.*\0\0");
	ofn.lpstrInitialDir = CurrentDir;
  ofn.Flags = OFN_EXPLORER;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof( szFile );

  if (!GetSaveFileName(&ofn))
    return;

  char *p;  
  p = strrchr(szFile, '.');
  
  if (!p)
    strcat_s(szFile,MAX_PATH+1,params["extension"].AsString());

  outputFile = _strdup(szFile);

  if (!initEncoder())
    return;
  DestroyWindow(wnd);
  so_out = this;
	wnd=CreateDialog(g_hInst,MAKEINTRESOURCE(IDD_DLG_CONVERT),0,ConvertProgressProc);
  SendMessage(wnd,WM_SETICON,ICON_SMALL, (LPARAM)LoadImage( g_hInst, MAKEINTRESOURCE(ICO_AVISYNTH),IMAGE_ICON,0,0,0));
  SetDlgItemText(wnd,IDC_STC_CONVERTMSG,"Initializing...");

  exitThread = false;
  encodeThread = CreateThread(NULL,NULL,StartEncoder,this, NULL,NULL);
  SetThreadPriority(encodeThread,THREAD_PRIORITY_BELOW_NORMAL);
  encodingStartedTick = GetTickCount();
}

void SoundOutput::encodingFinished() {
  SetDlgItemText(wnd,IDC_STC_CONVERTMSG,"Finished!");
  SetDlgItemText(wnd,IDC_BTN_CONVERTABORT,"Close");
  this->updatePercent(100);
  encodeThread = 0;
}
void SoundOutput::setError(const char* err) {
  char buf[800];
  sprintf_s(buf, 800, "An error occurred during conversion: %s\nThe output file is probably incomplete.", err);
  SetDlgItemText(wnd,IDC_STC_CONVERTMSG,buf);
  SetDlgItemText(wnd,IDC_BTN_CONVERTABORT,"Close");
  this->updatePercent(0);
  encodeThread = 0;
}

void SoundOutput::updatePercent(int p) {
  if (!exitThread)
    SendDlgItemMessage(wnd, IDC_PGB_CONVERTPROGRESS, PBM_SETPOS, p, 0);

  lastUpdateTick = GetTickCount();
}

void SoundOutput::updateSampleStats(__int64 processed,__int64 total) {
  if (GetTickCount() - lastUpdateTick < 50)
    return;
  char buf[800];
  double percent = ((double)processed * 100.0 / (double)total);
  int millis = GetTickCount() - encodingStartedTick;
  int hours = millis / (1000*60*60);
  int mins = (millis / (1000*60)) % 60;
  int secs = (millis / 1000) %60;
  int e_millis = (int)((double)millis * 100.0 / percent) - millis;
  int e_hours = e_millis / (1000*60*60);
  int e_mins = (e_millis / (1000*60)) % 60;
  int e_secs = (e_millis / 1000) %60;
  int sp_secs = (int)(processed / vi.audio_samples_per_second);
  int sp_hours = sp_secs / (60*60);
  int sp_mins = (sp_secs / 60) % 60;
  float speed = (1000.0f * sp_secs) / millis;
  sp_secs %= 60;
  int tot_secs = (int)(total / vi.audio_samples_per_second);
  int tot_hours = sp_secs / (60*60);
  int tot_mins = (sp_secs / 60) % 60;
  tot_secs %= 60;
  if (total > 0xffffffff) {
    sprintf_s(buf, 800, "Processed %d%u of %d%u samples (%d%%)\n"
      "Source Position: [%02d:%02d:%02d] of [%02d:%02d:%02d]  Speed: %1.2f x Realtime\n"
      "Time Elapsed: [%02d:%02d:%02d]  ETA: [%02d:%02d:%02d]\n"
      ,(int)(processed>>32),(unsigned int)(processed&0xffffffff),(int)(total>>32),(unsigned int)(total&0xffffffff), (int)percent,
      sp_hours, sp_mins, sp_secs, tot_hours, tot_mins, tot_secs, speed,
      hours,mins,secs, e_hours, e_mins, e_secs
      );
  } else {
    sprintf_s(buf, 800, "Processed %u of %u samples (%d%%)\n"
      "Source Position: [%02d:%02d:%02d] of [%02d:%02d:%02d]  Speed: %1.2f x Realtime\n"
      "Time Elapsed: [%02d:%02d:%02d]  ETA: [%02d:%02d:%02d]\n"
       ,(unsigned int)(processed), 
      (unsigned int)(total), (int)percent,
      sp_hours, sp_mins, sp_secs, tot_hours, tot_mins, tot_secs, speed,
      hours,mins,secs, e_hours, e_mins, e_secs
      );
  }
  if (!exitThread) {
    SetDlgItemText(wnd,IDC_STC_CONVERTMSG,buf);
    this->updatePercent((int)percent);
  }
  lastUpdateTick = GetTickCount();
}

/******************  SAMPLEFETCHER *******************/

DWORD WINAPI StartFetcher(LPVOID p) {
  SampleFetcher* t = (SampleFetcher*)p;
  t->FetchLoop();
  return 0;
}

SampleFetcher::SampleFetcher(PClip _child, IScriptEnvironment* _env) : child(_child), env(_env) {
  exitThread = false;
  evtNextBlockReady = ::CreateEvent (NULL, FALSE, FALSE, NULL);
  evtProcessNextBlock = ::CreateEvent (NULL, FALSE, FALSE, NULL);
  thread = CreateThread(NULL,NULL,StartFetcher,this, NULL,NULL);
  SetThreadPriority(thread,THREAD_PRIORITY_BELOW_NORMAL);
  prev_sb = NULL;
}

SampleFetcher::~SampleFetcher() {
  if (prev_sb)
     delete prev_sb;
  if (!exitThread) {
    exitThread = true;
    SetEvent(evtProcessNextBlock);
    while (thread) {  // Wait for thread to exit.
      Sleep(100);
    }
  }
}

SampleBlock* SampleFetcher::GetNextBlock() {
   HRESULT wait_result = WAIT_TIMEOUT;
   if (prev_sb)
     delete prev_sb;
   while (wait_result == WAIT_TIMEOUT && !exitThread) {
      wait_result = WaitForSingleObject(evtNextBlockReady, 1000);
   }
   SampleBlock* s = FinishedBlock;
   prev_sb = s;
   SetEvent(evtProcessNextBlock);
   return s;
}
void SampleFetcher::FetchLoop() {
  VideoInfo vi = child->GetVideoInfo();
  __int64 currentPos = 0;
  while (!exitThread) {
    int getsamples = BLOCKSAMPLES;
    if (currentPos + getsamples > vi.num_audio_samples) {
      getsamples = (int)(vi.num_audio_samples - currentPos);
      exitThread = true;
    }
    SampleBlock *s = new SampleBlock(&vi, getsamples);
    try {
      child->GetAudio(s->getSamples(), currentPos, getsamples, env);    
      currentPos += getsamples;
    } catch (...) {
      if(IDNO == MessageBox(NULL,"An Error occured, while fetching samples.\nCurrent sampleblock will be skipped\nWould you like to abort conversion?","Abort?",MB_YESNO)) {
        memset(s->getSamples(), 0, (size_t)vi.BytesFromAudioSamples(s->numSamples));
      } else {
        exitThread = true;
      }
    }
    if (exitThread)
      s->lastBlock = true;
    FinishedBlock = s;
    SetEvent(evtNextBlockReady);
    HRESULT wait_result = WAIT_TIMEOUT;
    while (wait_result == WAIT_TIMEOUT && !exitThread) {
      wait_result = WaitForSingleObject(evtProcessNextBlock, 1000);
    }
  }
  thread = 0;
}


extern const char* t_BYTES;
extern const char* t_KB;
extern const char* t_MB;
extern const char* t_GB;

int ConvertDataSizes(__int64 bytes, const char** OutMetric) {
  const char* s_metric = "";
  __int64 datasize = bytes;
  if (datasize > 10*1024) { // More than 10k, switch to KB notation
    datasize >>=10;
    s_metric = t_KB;
  }
  if (datasize > 10*1024) { // More than 10MB, switch to MB notation
    datasize >>=10;
    s_metric = t_MB;
  }
  if (datasize > 10*1024) { // More than 10GB, switch to GB notation
    datasize >>=10;
    s_metric = t_GB;
  }
  OutMetric[0] = s_metric;
  return (int)datasize;
}