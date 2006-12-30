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
  t->encodeLoop();
  t->encodingFinished();
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
  char szFile[MAX_PATH];
	szFile[0] = 0;

  char CurrentDir[MAX_PATH];
	GetCurrentDirectory( MAX_PATH, CurrentDir );

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.hInstance = g_hInst;
  ofn.lpstrTitle = "Select where to save file..";
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFilter = outputFileFilter;
	ofn.lpstrInitialDir = CurrentDir;
  ofn.Flags = OFN_EXPLORER;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof( szFile );

  if (!GetSaveFileName(&ofn))
    return;
  
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
}

void SoundOutput::encodingFinished() {
  SetDlgItemText(wnd,IDC_STC_CONVERTMSG,"Finished!");
  SetDlgItemText(wnd,IDC_BTN_CONVERTABORT,"Close");
  this->updatePercent(100);
  encodeThread = 0;
}

void SoundOutput::updatePercent(int p) {
  if (!exitThread)
    SendDlgItemMessage(wnd, IDC_PGB_CONVERTPROGRESS, PBM_SETPOS, p, 0);
}

void SoundOutput::updateSampleStats(__int64 processed,__int64 total) {
  char buf[800];
  int percent = (int)(processed * 100 / total);
  sprintf_s(buf, 800, "Processed %d%u of %d%u samples (%d%%)\n",
    (int)(processed>>32),(unsigned int)(processed&0xffffffff), 
    (int)(total>>32),(unsigned int)(total&0xffffffff), percent);

  if (!exitThread) {
    SetDlgItemText(wnd,IDC_STC_CONVERTMSG,buf);
    this->updatePercent(percent);
  }

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
}

SampleFetcher::~SampleFetcher() {
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
   while (wait_result == WAIT_TIMEOUT && !exitThread) {
      wait_result = WaitForSingleObject(evtNextBlockReady, 1000);
   }
   SampleBlock* s = FinishedBlock;
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
    child->GetAudio(s->getSamples(), currentPos, getsamples, env);
    currentPos += getsamples;
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