// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
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
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.


#define INITGUID
#include "internal.h"
#include "audio.h"


static long gRefCnt=0;


extern "C" const GUID CLSID_CAVIFileSynth   // {E6D6B700-124D-11D4-86F3-DB80AFD98778}
  = {0xe6d6b700, 0x124d, 0x11d4, {0x86, 0xf3, 0xdb, 0x80, 0xaf, 0xd9, 0x87, 0x78}};

extern "C" const GUID IID_IAvisynthClipInfo   // {E6D6B708-124D-11D4-86F3-DB80AFD98778}
  = {0xe6d6b708, 0x124d, 0x11d4, {0x86, 0xf3, 0xdb, 0x80, 0xaf, 0xd9, 0x87, 0x78}};


struct IAvisynthClipInfo : IUnknown {
  virtual int __stdcall GetError(const char** ppszMessage) = 0;
  virtual bool __stdcall GetParity(int n) = 0;
  virtual bool __stdcall IsFieldBased() = 0;
};


class CAVIFileSynth: public IAVIFile, public IPersistFile, public IClassFactory, public IAvisynthClipInfo {
  friend class CAVIStreamSynth;
private:
	long m_refs;

    char* szScriptName;
    IScriptEnvironment* env;
    PClip filter_graph;
    const VideoInfo* vi;
    const char* error_msg;

    bool DelayInit();

    void MakeErrorStream(const char* msg);

public:

	CAVIFileSynth(const CLSID& rclsid);
	~CAVIFileSynth();

	////////////

	static HRESULT Create(const CLSID& rclsid, const IID& riid, void **ppv);
	STDMETHODIMP QueryInterface(const IID& iid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	//////////// IClassFactory

	STDMETHODIMP CreateInstance (LPUNKNOWN pUnkOuter, REFIID riid,  void * * ppvObj) ;
	STDMETHODIMP LockServer (BOOL fLock) ;

	//////////// IPersistFile

	STDMETHODIMP GetClassID(LPCLSID lpClassID);

	STDMETHODIMP IsDirty();
	STDMETHODIMP Load(LPCOLESTR lpszFileName, DWORD grfMode);
	STDMETHODIMP Save(LPCOLESTR lpszFileName, BOOL fRemember);
	STDMETHODIMP SaveCompleted(LPCOLESTR lpszFileName);
	STDMETHODIMP GetCurFile(LPOLESTR *lplpszFileName);

	//////////// IAVIFile

	STDMETHODIMP CreateStream(PAVISTREAM *ppStream, AVISTREAMINFOW *psi);
	STDMETHODIMP EndRecord();
	STDMETHODIMP GetStream(PAVISTREAM *ppStream, DWORD fccType, LONG lParam);
	STDMETHODIMP Info(AVIFILEINFOW *psi, LONG lSize);
	STDMETHODIMP Open(LPCSTR szFile, UINT mode, LPCOLESTR lpszFileName);
    STDMETHODIMP Save(LPCSTR szFile, AVICOMPRESSOPTIONS FAR *lpOptions,
				AVISAVECALLBACK lpfnCallback);
	STDMETHODIMP ReadData(DWORD fcc, LPVOID lp, LONG *lpcb);
	STDMETHODIMP WriteData(DWORD fcc, LPVOID lpBuffer, LONG cbBuffer);
	STDMETHODIMP DeleteStream(DWORD fccType, LONG lParam);

	//////////// IAVIFile

	int __stdcall GetError(const char** ppszMessage);
	bool __stdcall GetParity(int n);
	bool __stdcall IsFieldBased();
};

///////////////////////////////////

class CAVIStreamSynth;

class CAVIStreamSynth: public IAVIStream {
private:
	long m_refs;

	CAVIFileSynth *parent;
	BOOL fAudio;

public:

	STDMETHODIMP QueryInterface(const IID& iid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	CAVIStreamSynth(CAVIFileSynth *parentPtr, bool isAudio);
	~CAVIStreamSynth();

	STDMETHODIMP Create(LONG lParam1, LONG lParam2);
	STDMETHODIMP Delete(LONG lStart, LONG lSamples);
	STDMETHODIMP_(LONG) Info(AVISTREAMINFOW *psi, LONG lSize);
	STDMETHODIMP_(LONG) FindSample(LONG lPos, LONG lFlags);
	STDMETHODIMP Read(LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, LONG *plBytes, LONG *plSamples);
	STDMETHODIMP ReadData(DWORD fcc, LPVOID lp, LONG *lpcb);
	STDMETHODIMP ReadFormat(LONG lPos, LPVOID lpFormat, LONG *lpcbFormat);
	STDMETHODIMP SetFormat(LONG lPos, LPVOID lpFormat, LONG cbFormat);
	STDMETHODIMP Write(LONG lStart, LONG lSamples, LPVOID lpBuffer,
		LONG cbBuffer, DWORD dwFlags, LONG FAR *plSampWritten, 
		LONG FAR *plBytesWritten);
	STDMETHODIMP WriteData(DWORD fcc, LPVOID lpBuffer, LONG cbBuffer);
	STDMETHODIMP SetInfo(AVISTREAMINFOW *psi, LONG lSize);

	void ReadHelper(void* lpBuffer, int lStart, int lSamples);
	void ReadFrame(void* lpBuffer, int n);
};


BOOL APIENTRY DllMain(HANDLE hModule, ULONG ulReason, LPVOID lpReserved) {

	switch(ulReason) {
	case DLL_PROCESS_ATTACH:
		CoInitialize(NULL);
		_RPT0(0,"Process attach\n");
		break;

	case DLL_PROCESS_DETACH:
		CoUninitialize();
		_RPT0(0,"Process detach\n");
		break;
	}

    return TRUE;
}


// From the Microsoft AVIFile docs.  Dense code...

extern "C" STDAPI DllGetClassObject(const CLSID& rclsid, const IID& riid, void **ppv);

STDAPI DllGetClassObject(const CLSID& rclsid, const IID& riid, void **ppv) {
	_RPT0(0,"DllGetClassObject()\n");

	HRESULT hresult;
	if (rclsid == CLSID_CAVIFileSynth) {
		_RPT0(0,"\tCLSID: CAVIFileSynth\n");
		hresult = CAVIFileSynth::Create(rclsid, riid, ppv);
	} else {
		hresult = CLASS_E_CLASSNOTAVAILABLE;
	}

	_RPT0(0,"DllGetClassObject() exit\n");

	return hresult;
}

extern "C" STDAPI DllCanUnloadNow();

STDAPI DllCanUnloadNow() {
//	_RPT1(0,"DllCanUnloadNow(): gRefCnt = %ld\n", gRefCnt);

	return gRefCnt ? S_FALSE : S_OK;
}


///////////////////////////////////////////////////////////////////////////
//
//	CAVIFileSynth
//
///////////////////////////////////////////////////////////////////////////

STDMETHODIMP CAVIFileSynth::CreateInstance (LPUNKNOWN pUnkOuter, REFIID riid,  void * * ppvObj) {
	if (pUnkOuter) return CLASS_E_NOAGGREGATION;

	return Create(CLSID_CAVIFileSynth, riid, ppvObj);
}

STDMETHODIMP CAVIFileSynth::LockServer (BOOL fLock) {
	_RPT1(0,"%p->CAVIFileSynth::LockServer()\n", this);
	return S_OK;
}

///////////////////////////////////////////////////

STDMETHODIMP CAVIFileSynth::GetClassID(LPCLSID lpClassID) {
	_RPT1(0,"%p->CAVIFileSynth::GetClassID()\n", this);

    *lpClassID = CLSID_CAVIFileSynth;

	return S_OK;
}

STDMETHODIMP CAVIFileSynth::IsDirty() {
	_RPT1(0,"%p->CAVIFileSynth::IsDirty()\n", this);
	return S_FALSE;
}

STDMETHODIMP CAVIFileSynth::Load(LPCOLESTR lpszFileName, DWORD grfMode) {
	char filename[MAX_PATH];

	_RPT1(0,"%p->CAVIFileSynth::Load()\n", this);

	WideCharToMultiByte(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, lpszFileName, -1, filename, sizeof filename, NULL, NULL); 

	return Open(filename, grfMode, lpszFileName);
}

STDMETHODIMP CAVIFileSynth::Save(LPCOLESTR lpszFileName, BOOL fRemember) {
	_RPT1(0,"%p->CAVIFileSynth::Save()\n", this);
	return E_FAIL;
}

STDMETHODIMP CAVIFileSynth::SaveCompleted(LPCOLESTR lpszFileName) {
	_RPT1(0,"%p->CAVIFileSynth::SaveCompleted()\n", this);
	return S_OK;
}

STDMETHODIMP CAVIFileSynth::GetCurFile(LPOLESTR *lplpszFileName) {
	_RPT1(0,"%p->CAVIFileSynth::GetCurFile()\n", this);
	*lplpszFileName = NULL;

	return E_FAIL;
}

///////////////////////////////////////////////////

HRESULT CAVIFileSynth::Create(const CLSID& rclsid, const IID& riid, void **ppv) {
	HRESULT hresult;

	_RPT0(0,"CAVIFileSynth::Create()\n");

	CAVIFileSynth* pAVIFileSynth = new CAVIFileSynth(rclsid);

	if (!pAVIFileSynth) return E_OUTOFMEMORY;

	hresult = pAVIFileSynth->QueryInterface(riid, ppv);
	pAVIFileSynth->Release();

	if (FAILED(hresult)) {
		_RPT0(0,"failed!\n");
	}

	_RPT0(0,"CAVIFileSynth::Create() exit\n");

	return hresult;
}

STDMETHODIMP CAVIFileSynth::QueryInterface(const IID& iid, void **ppv) {
	_RPT1(0,"%08lx->CAVIFileSynth::QueryInterface()\n", this);

	_RPT3(0,"\tGUID: {%08lx-%04x-%04x-", iid.Data1, iid.Data2, iid.Data3);
	_RPT4(0,"%02x%02x-%02x%02x", iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3]);
	_RPT4(0,"%02x%02x%02x%02x} (", iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7]);

	if (iid == IID_IUnknown) {
		*ppv = (IUnknown *)(IAVIFile *)this;
		_RPT0(0,"IUnknown)\n");
	} else if (iid == IID_IClassFactory) {
		*ppv = (IClassFactory *)this;
		_RPT0(0,"IClassFactory)\n");
	} else if (iid == IID_IPersist) {
		*ppv = (IPersist *)this;
		_RPT0(0,"IPersist)\n");
	} else if (iid == IID_IPersistFile) {
		*ppv = (IPersistFile *)this;
		_RPT0(0,"IPersistFile)\n");
	} else if (iid == IID_IAVIFile) {
		*ppv = (IAVIFile *)this;
		_RPT0(0,"IAVIFile)\n");
	} else if (iid == IID_IAvisynthClipInfo) {
		*ppv = (IAvisynthClipInfo *)this;
		_RPT0(0,"IAvisynthClipInfo)\n");
	} else {
		_RPT0(0,"unknown!)\n");
		*ppv = NULL;
		return E_NOINTERFACE;
	}

	AddRef();

	return NULL;
}

STDMETHODIMP_(ULONG) CAVIFileSynth::AddRef() {
//	_RPT1(0,"%p->CAVIFileSynth::AddRef()\n", this);
    InterlockedIncrement(&gRefCnt);
	return ++m_refs;
}

STDMETHODIMP_(ULONG) CAVIFileSynth::Release() {
//	_RPT3(0,"%p->CAVIFileSynth::Release() - former gRefCnt = %d, m_refs = %d\n", this, gRefCnt, m_refs);

    const int refs = --m_refs;
	if (!refs) delete this;
    InterlockedDecrement(&gRefCnt);
	return refs;
}

///////////////////////

STDMETHODIMP CAVIStreamSynth::QueryInterface(const IID& iid, void **ppv) {
	_RPT1(0,"%08lx->CAVIStreamSynth::QueryInterface()\n", this);

	_RPT3(0,"\tGUID: {%08lx-%04x-%04x-", iid.Data1, iid.Data2, iid.Data3);
	_RPT4(0,"%02x%02x-%02x%02x", iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3]);
	_RPT4(0,"%02x%02x%02x%02x} (", iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7]);

	if (iid == IID_IUnknown) {
		*ppv = (IUnknown *)this;
		_RPT0(0,"IUnknown)\n");
	} else if (iid == IID_IAVIStream) {
		*ppv = (IAVIStream *)this;
		_RPT0(0,"IAVIStream)\n");
	} else {
		_RPT0(0,"unknown)\n");
		*ppv = NULL;
		return E_NOINTERFACE;
	}

	AddRef();

	return NULL;
}

STDMETHODIMP_(ULONG) CAVIStreamSynth::AddRef() {
//	_RPT0(0,"CAVIStreamSynth::AddRef()\n");

    InterlockedIncrement(&gRefCnt);
	return ++m_refs;
}

STDMETHODIMP_(ULONG) CAVIStreamSynth::Release() {
//	_RPT0(0,"CAVIStreamSynth::Release()\n");

    const int refs = --m_refs;
	if (!refs) delete this;
    InterlockedDecrement(&gRefCnt);
	return refs;
}

////////////////////////////////////////////////////////////////////////
//
//		CAVIFileSynth
//
////////////////////////////////////////////////////////////////////////

STDMETHODIMP CAVIFileSynth::CreateStream(PAVISTREAM *ppStream, AVISTREAMINFOW *psi) {
	_RPT1(0,"%p->CAVIFileSynth::CreateStream()\n", this);
	return S_OK;//AVIERR_READONLY;
}

STDMETHODIMP CAVIFileSynth::EndRecord() {
	_RPT1(0,"%p->CAVIFileSynth::EndRecord()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIFileSynth::Save(LPCSTR szFile, AVICOMPRESSOPTIONS FAR *lpOptions,
				AVISAVECALLBACK lpfnCallback) {
	_RPT1(0,"%p->CAVIFileSynth::Save()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIFileSynth::ReadData(DWORD fcc, LPVOID lp, LONG *lpcb) {
	_RPT1(0,"%p->CAVIFileSynth::ReadData()\n", this);
	return AVIERR_NODATA;
}

STDMETHODIMP CAVIFileSynth::WriteData(DWORD fcc, LPVOID lpBuffer, LONG cbBuffer) {
	_RPT1(0,"%p->CAVIFileSynth::WriteData()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIFileSynth::DeleteStream(DWORD fccType, LONG lParam) {
	_RPT1(0,"%p->CAVIFileSynth::DeleteStream()\n", this);
	return AVIERR_READONLY;
}


CAVIFileSynth::CAVIFileSynth(const CLSID& rclsid) {
	_RPT0(0,"CAVIFileSynth::CAVIFileSynth()\n");

	m_refs = 0; AddRef();

    szScriptName = 0;
    env = 0;

    error_msg = 0;
}

CAVIFileSynth::~CAVIFileSynth() {
	_RPT0(0,"CAVIFileSynth::~CAVIFileSynth()\n");

    delete[] szScriptName;

    filter_graph = 0;
    
	delete env;
}


STDMETHODIMP CAVIFileSynth::Open(LPCSTR szFile, UINT mode, LPCOLESTR lpszFileName) {

	_RPT2(0,"CAVIFileSynth::Open(\"%s\", %08lx)\n", szFile, mode);

	if (mode & (OF_CREATE|OF_WRITE))
      return E_FAIL;

    delete env;   // just in case
    env = 0;
    filter_graph = 0;
    vi = 0;

    szScriptName = new char[lstrlen(szFile)+1];
    if (!szScriptName)
      return AVIERR_MEMORY;
    lstrcpy(szScriptName, szFile);

    return S_OK;
}

bool CAVIFileSynth::DelayInit() {
  if (szScriptName) {
    try {
      try {
        // create a script environment and load the script into it
        env = CreateScriptEnvironment();
        if (!env) return false;
        AVSValue return_val = env->Invoke("Import", szScriptName);
        // store the script's return value (a video clip)
        if (return_val.IsClip())
          filter_graph = ConvertAudio::Create(return_val.AsClip(), SAMPLE_INT8|SAMPLE_INT16|SAMPLE_INT24|SAMPLE_INT32, SAMPLE_INT16);  // Ensure samples are int     [filter_graph = return_val.AsClip();]
        else
          throw AvisynthError("The script's return value was not a video clip");
        if (!filter_graph)
          throw AvisynthError("The returned video clip was nil (this is a bug)");
        // get information about the clip
        vi = &filter_graph->GetVideoInfo();

        if (vi->IsYV12()&&(vi->width&3))
          throw AvisynthError("Avisynth error: YV12 images for output must have a width divisible by 4 (use crop)!");
        if (vi->IsYUY2()&&(vi->width&3))
          throw AvisynthError("Avisynth error: YUY2 images for output must have a width divisible by 4 (use crop)!");
      }
      catch (AvisynthError error) {
        error_msg = error.msg;
        AVSValue args[2] = { error.msg, 0xff3333 };
        static const char* arg_names[2] = { 0, "text_color" };
        filter_graph = env->Invoke("MessageClip", AVSValue(args, 2), arg_names).AsClip();
        vi = &filter_graph->GetVideoInfo();
      }
      delete[] szScriptName;
      szScriptName = 0;
      return true;
    }
    catch (...) {
      return false;
    }
  } else {
    return (env && filter_graph && vi);
  }
}


void CAVIFileSynth::MakeErrorStream(const char* msg) {
  error_msg = msg;
  filter_graph = Create_MessageClip(msg, vi->width, vi->height, vi->pixel_type, false, 0xFF3333, 0, 0, env);
}


STDMETHODIMP CAVIFileSynth::Info(AVIFILEINFOW *pfi, LONG lSize) {

//	_RPT0(0,"CAVIFileSynth::Info()\n");

	if (!DelayInit()) return E_FAIL;

	pfi->dwMaxBytesPerSec		= 0;
	pfi->dwFlags				= AVIFILEINFO_HASINDEX | AVIFILEINFO_ISINTERLEAVED;
	pfi->dwCaps					= AVIFILECAPS_CANREAD | AVIFILECAPS_ALLKEYFRAMES | AVIFILECAPS_NOCOMPRESSION;
	
	int nrStreams=0;
	if (vi->HasVideo()==true)	nrStreams=1;
	if (vi->HasAudio()==true)	nrStreams++;

	pfi->dwStreams				= nrStreams;
	pfi->dwSuggestedBufferSize	= 0;
	pfi->dwWidth				= vi->width;
	pfi->dwHeight				= vi->height;
	pfi->dwEditCount			= 0;

    pfi->dwRate					= vi->fps_numerator;
	pfi->dwScale				= vi->fps_denominator;
	pfi->dwLength				= vi->num_frames;

    wcscpy(pfi->szFileType, L"Avisynth");

	return S_OK;
}

STDMETHODIMP CAVIFileSynth::GetStream(PAVISTREAM *ppStream, DWORD fccType, LONG lParam) {
	CAVIStreamSynth *casr;

//	_RPT4(0,"%p->CAVIFileSynth::GetStream(%p, %08lx, %ld)\n", this, ppStream, fccType, lParam);

	if (!DelayInit()) return E_FAIL;

    *ppStream = NULL;

	if (!fccType) 
	{

		if ((lParam==0) && (vi->HasVideo()) )
			fccType = streamtypeVIDEO;
		else 
		  if ( ((lParam==1) && (vi->HasVideo())) ||  ((lParam==0) && vi->HasAudio()) )
		  {
			lParam=0;
    		fccType = streamtypeAUDIO;
		  }
	}

	if (lParam > 0) return AVIERR_NODATA;

	if (fccType == streamtypeVIDEO) {
		if (!vi->HasVideo())
			return AVIERR_NODATA;

        if ((casr = new CAVIStreamSynth(this, false)) == 0)
			return AVIERR_MEMORY;

		*ppStream = (IAVIStream *)casr;

	} else if (fccType == streamtypeAUDIO) {
		if (!vi->HasAudio())
			return AVIERR_NODATA;

		if ((casr = new CAVIStreamSynth(this, true)) == 0)
			return AVIERR_MEMORY;

		*ppStream = (IAVIStream *)casr;
	} else
		return AVIERR_NODATA;

	return 0;
}


int __stdcall CAVIFileSynth::GetError(const char** ppszMessage) {
  if (!DelayInit())
    error_msg = "Avisynth: script open failed!";

  if (ppszMessage)
    *ppszMessage = error_msg;
  return !!error_msg;
}

bool __stdcall CAVIFileSynth::GetParity(int n) {
  if (!DelayInit())
    return false;
  return filter_graph->GetParity(n);
}

bool __stdcall CAVIFileSynth::IsFieldBased() {
  if (!DelayInit())
    return false;
  return vi->IsFieldBased();
}


////////////////////////////////////////////////////////////////////////
//
//		CAVIStreamSynth
//
////////////////////////////////////////////////////////////////////////

STDMETHODIMP CAVIStreamSynth::Create(LONG lParam1, LONG lParam2) {
	_RPT1(0,"%p->CAVIStreamSynth::Create()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIStreamSynth::Delete(LONG lStart, LONG lSamples) {
	_RPT1(0,"%p->CAVIStreamSynth::Delete()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIStreamSynth::ReadData(DWORD fcc, LPVOID lp, LONG *lpcb) {
	_RPT1(0,"%p->CAVIStreamSynth::ReadData()\n", this);
	return AVIERR_NODATA;
}

STDMETHODIMP CAVIStreamSynth::SetFormat(LONG lPos, LPVOID lpFormat, LONG cbFormat) {
	_RPT1(0,"%p->CAVIStreamSynth::SetFormat()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIStreamSynth::WriteData(DWORD fcc, LPVOID lpBuffer, LONG cbBuffer) {
	_RPT1(0,"%p->CAVIStreamSynth::WriteData()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIStreamSynth::SetInfo(AVISTREAMINFOW *psi, LONG lSize) {
	return AVIERR_READONLY;
}

CAVIStreamSynth::CAVIStreamSynth(CAVIFileSynth *parentPtr, bool isAudio) {
  _RPT2(0,"%p->CAVIStreamSynth(%s)\n", this, isAudio ? "audio" : "video");
  m_refs = 0; AddRef();

  parent			= parentPtr;
  fAudio			= isAudio;

  parent->AddRef();
}

CAVIStreamSynth::~CAVIStreamSynth() {
//  _RPT1(0,"%p->~CAVIStreamSynth()\n", this);

  if (parent)
    parent->Release();
}

STDMETHODIMP_(LONG) CAVIStreamSynth::Info(AVISTREAMINFOW *psi, LONG lSize) {
//	_RPT3(0,"%p->CAVIStreamSynth::Info(%p, %ld)\n", this, psi, lSize);

	AVISTREAMINFOW asi;

    const VideoInfo* const vi = parent->vi;

    memset(&asi, 0, sizeof(asi));
    asi.fccType = fAudio ? streamtypeAUDIO : streamtypeVIDEO;
    asi.dwQuality = DWORD(-1);
    if (fAudio) {
      asi.fccHandler = 0;
      int bytes_per_sample = vi->BytesPerAudioSample();
      asi.dwScale = bytes_per_sample;
      asi.dwRate = vi->audio_samples_per_second * bytes_per_sample;
      asi.dwLength = vi->num_audio_samples;
      asi.dwSampleSize = bytes_per_sample;
      wcscpy(asi.szName, L"Avisynth audio #1");
    } else {
      const int image_size = vi->BMPSize();
      asi.fccHandler = 'UNKN';
      if (vi->IsRGB()) 
        asi.fccHandler = ' BID';
      else if (vi->IsYUY2())
        asi.fccHandler = '2YUY';
      else if (vi->IsYV12())
        asi.fccHandler = '21VY'; 
      else {
        _ASSERT(FALSE);
      }
      //      asi.fccHandler = vi->IsYUY2() ? '2YUY' : ' BID';
      asi.dwScale = vi->fps_denominator;
      asi.dwRate = vi->fps_numerator;
      asi.dwLength = vi->num_frames;
      asi.rcFrame.right = vi->width;
      asi.rcFrame.bottom = vi->height;
      asi.dwSampleSize = image_size;
      asi.dwSuggestedBufferSize = image_size;
      wcscpy(asi.szName, L"Avisynth video #1");
    }

    memset(psi, 0, lSize);
    memcpy(psi, &asi, min(size_t(lSize), sizeof(asi)));

	return 0;
}

STDMETHODIMP_(LONG) CAVIStreamSynth::FindSample(LONG lPos, LONG lFlags) {
//	_RPT3(0,"%p->CAVIStreamSynth::FindSample(%ld, %08lx)\n", this, lPos, lFlags);

	if (lFlags & FIND_FORMAT)
		return -1;

	if (lFlags & FIND_FROM_START)
		return 0;

	return lPos;
}


void CAVIStreamSynth::ReadFrame(void* lpBuffer, int n) {
  PVideoFrame frame = parent->filter_graph->GetFrame(n, parent->env);
  if (!frame)
    parent->env->ThrowError("Avisynth error: generated video frame was nil (this is a bug)");
//  VideoInfo vi = parent->filter_graph->GetVideoInfo();
  const int pitch = frame->GetPitch();
  const int row_size = frame->GetRowSize();
  // BMP scanlines are always dword-aligned
  const int out_pitch = (row_size+3) & -4;
  BitBlt((BYTE*)lpBuffer, out_pitch, frame->GetReadPtr(), pitch, row_size, frame->GetHeight());
  // TODO: Make the following more eyepleasing
  BitBlt((BYTE*)lpBuffer+(out_pitch*frame->GetHeight()), out_pitch/2, frame->GetReadPtr(PLANAR_V), frame->GetPitch(PLANAR_V), frame->GetRowSize(PLANAR_V), frame->GetHeight(PLANAR_V));
  BitBlt((BYTE*)lpBuffer+(out_pitch*frame->GetHeight()+frame->GetHeight(PLANAR_U)*out_pitch/2), out_pitch/2, frame->GetReadPtr(PLANAR_U), frame->GetPitch(PLANAR_U), frame->GetRowSize(PLANAR_V), frame->GetHeight(PLANAR_U));
}


void CAVIStreamSynth::ReadHelper(void* lpBuffer, int lStart, int lSamples) {
  // It's illegal to call GetExceptionInformation() inside an __except
  // block!  Hence this variable and the horrible hack below...
#ifndef _DEBUG
  EXCEPTION_POINTERS* ei;
  DWORD code;
  __try { 
#endif
    if (fAudio)
      parent->filter_graph->GetAudio(lpBuffer, lStart, lSamples, parent->env);
    else
      ReadFrame(lpBuffer, lStart);
#ifndef _DEBUG
  }
  __except (ei = GetExceptionInformation(), code = GetExceptionCode(), (code >> 28) == 0xC) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
      parent->env->ThrowError("Avisynth: caught an access violation at 0x%08x,\nattempting to %s 0x%08x",
        ei->ExceptionRecord->ExceptionAddress,
        ei->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from",
        ei->ExceptionRecord->ExceptionInformation[1]);
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      parent->env->ThrowError("Avisynth: illegal instruction at 0x%08x",
        ei->ExceptionRecord->ExceptionAddress);
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      parent->env->ThrowError("Avisynth: division by zero at 0x%08x",
        ei->ExceptionRecord->ExceptionAddress);
    case EXCEPTION_STACK_OVERFLOW:
      throw AvisynthError("Avisynth: stack overflow");
    default:
      parent->env->ThrowError("Avisynth: unknown exception 0x%08x at 0x%08x",
        code, ei->ExceptionRecord->ExceptionAddress);
    }
  }
#endif
}

STDMETHODIMP CAVIStreamSynth::Read(LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, LONG *plBytes, LONG *plSamples) {

//  _RPT3(0,"%p->CAVIStreamSynth::Read(%ld samples at %ld)\n", this, lSamples, lStart);
//  _RPT2(0,"\tbuffer: %ld bytes at %p\n", cbBuffer, lpBuffer);

  if (fAudio) {
    if (lSamples == AVISTREAMREAD_CONVENIENT)
      lSamples = parent->vi->AudioSamplesFromFrames(1);
    if (plBytes) *plBytes = parent->vi->BytesFromAudioSamples(lSamples);
    if (plSamples) *plSamples = lSamples;
    if (!lpBuffer)
      return S_OK;

  } else {
    int image_size = parent->vi->BMPSize();
    if (plSamples) *plSamples = 1;
    if (plBytes) *plBytes = image_size;

    if (!lpBuffer) {
      return S_OK;
    } else if (cbBuffer < image_size) {
      _RPT1(0,"\tBuffer too small; should be %ld samples\n", image_size);
      return AVIERR_BUFFERTOOSMALL;
    }
  }

#ifndef _DEBUG
  try {
    try {
#endif
      // VC compiler says "only one form of exception handling permitted per
      // function."  Sigh...
      ReadHelper(lpBuffer, lStart, lSamples);
#ifndef _DEBUG
    }
    catch (AvisynthError error) {
      parent->MakeErrorStream(error.msg);
      ReadHelper(lpBuffer, lStart, lSamples);
    }
    catch (...) {
      parent->MakeErrorStream("Avisynth: unknown exception");
      ReadHelper(lpBuffer, lStart, lSamples);
    }
  }
  catch (...) {
    return E_FAIL;
  }
#endif
  return S_OK;
}

STDMETHODIMP CAVIStreamSynth::ReadFormat(LONG lPos, LPVOID lpFormat, LONG *lpcbFormat) {
//  _RPT1(0,"%p->CAVIStreamSynth::ReadFormat()\n", this);

  if (!lpFormat) {
    *lpcbFormat = fAudio ? sizeof(WAVEFORMATEX) : sizeof(BITMAPINFOHEADER);
	  return S_OK;
  }

  memset(lpFormat, 0, *lpcbFormat);

  const VideoInfo* const vi = parent->vi;

  if (fAudio) {
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = 1;
    wfx.nChannels = vi->AudioChannels();  // Perhaps max out at 2?
    wfx.nSamplesPerSec = vi->SamplesPerSecond();
    wfx.wBitsPerSample = vi->BytesPerChannelSample() * 8;
    wfx.nBlockAlign = vi->BytesPerAudioSample();
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    memcpy(lpFormat, &wfx, min(size_t(*lpcbFormat), sizeof(wfx)));
  } else {
    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(bi);
    bi.biWidth = vi->width;
    bi.biHeight = vi->height;
    bi.biPlanes = 1;
    bi.biBitCount = vi->BitsPerPixel();
      if (vi->IsRGB()) 
        bi.biCompression = BI_RGB;
      else if (vi->IsYUY2())
        bi.biCompression = '2YUY';
      else if (vi->IsYV12())
        bi.biCompression = '21VY';
      else {
        _ASSERT(FALSE);
      }
    bi.biSizeImage = vi->BMPSize();
//    bi.biSizeImage = bi.biWidth * bi.biHeight * bi.biBitCount / 8;
    memcpy(lpFormat, &bi, min(size_t(*lpcbFormat), sizeof(bi)));
  }

  return S_OK;
}

STDMETHODIMP CAVIStreamSynth::Write(LONG lStart, LONG lSamples, LPVOID lpBuffer,
	LONG cbBuffer, DWORD dwFlags, LONG FAR *plSampWritten, 
    LONG FAR *plBytesWritten) {

	_RPT1(0,"%p->CAVIStreamSynth::Write()\n", this);

	return AVIERR_READONLY;
}

