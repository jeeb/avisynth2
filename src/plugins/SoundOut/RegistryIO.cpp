#include "RegistryIO.h"
#include <vector>

using namespace std;

const HKEY RegRootKey = HKEY_CURRENT_USER;
const char RegFirstAvisynthKey[] = "Software\\Avisynth";
const char RegAvisynthKey[] = "Software\\Avisynth\\SoundOut";

RegistryIO::RegistryIO() {
}

RegistryIO::~RegistryIO(void) {
}

void RegistryIO::StoreSettings(Param &params) {
  vector<const char*> ignoreParams;
  ignoreParams.push_back("filterID");
  ignoreParams.push_back("outputFileFilter");
  ignoreParams.push_back("extension");
  ignoreParams.push_back("useFilename");
  ignoreParams.push_back("showProgress");
  ignoreParams.push_back("overwriteFile");
  ignoreParams.push_back("autoCloseWindow");

  char FilterKey[200];
  sprintf_s(FilterKey,200,"%s\\%s",RegAvisynthKey,params["filterID"].AsString());
  HKEY AvisynthKey;
  DWORD disposition;
  if (ERROR_SUCCESS != RegCreateKeyEx(RegRootKey,FilterKey,0,NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE,NULL, &AvisynthKey,&disposition)) 
    return;

  Param::iterator i;
  for ( i = params.begin(); i != params.end(); i++) {
    bool ignoreP = false;
    for (vector<const char*>::iterator j=ignoreParams.begin(); j != ignoreParams.end(); j++) {
      if (!_stricmp((*i).first,*j)) 
        ignoreP = true;
    }
    if (!ignoreP) {
      AVSValue v = (*i).second;
      if (v.IsString()) {
        RegSetValueEx(AvisynthKey,(*i).first, 0, REG_SZ, (const BYTE*)v.AsString(), (DWORD)strlen(v.AsString())+1);
      } else if (v.IsInt() || v.IsBool()) {
        int val = v.IsInt() ? v.AsInt() : v.AsBool();
        RegSetValueEx(AvisynthKey,(*i).first, 0, REG_DWORD, (const BYTE*)&val, sizeof(int));
      } else if (v.IsFloat()) {
        double val = v.AsFloat();
        RegSetValueEx(AvisynthKey,(*i).first, 0, REG_BINARY, (const BYTE*)&val, sizeof(double));
      }
    }
  }
  RegCloseKey(AvisynthKey);
}

void RegistryIO::RetrieveSettings(Param &params, IScriptEnvironment* env) {
  vector<const char*> ignoreParams;
  ignoreParams.push_back("filterID");
  ignoreParams.push_back("outputFileFilter");
  ignoreParams.push_back("extension");
  ignoreParams.push_back("useFilename");
  ignoreParams.push_back("showProgress");
  ignoreParams.push_back("overwriteFile");
  ignoreParams.push_back("autoCloseWindow");
  char FilterKey[200];
  sprintf_s(FilterKey,200,"%s\\%s",RegAvisynthKey,params["filterID"].AsString());
  HKEY AvisynthKey;
  Param::iterator i;
  if (ERROR_SUCCESS != RegOpenKeyEx(RegRootKey,FilterKey, 0,KEY_READ,&AvisynthKey))
    return;

  for ( i = params.begin(); i != params.end(); i++) {
    bool ignoreP = false;
    for (vector<const char*>::iterator j=ignoreParams.begin(); j != ignoreParams.end(); j++) {
      if (!_stricmp((*i).first,*j)) 
        ignoreP = true;
    }
    if (!ignoreP) {
      AVSValue v = (*i).second;
      if (v.IsString()) {
        DWORD type = REG_SZ;
        char buf[4000];
        DWORD size = 4000;
        if (ERROR_SUCCESS == RegQueryValueEx(AvisynthKey, (*i).first,0,&type, (LPBYTE)buf, &size)) {
          buf[size] = 0;
          (*i).second = AVSValue(env->SaveString(buf,size));
        }
      } else if (v.IsInt() || v.IsBool()) {
        DWORD type = REG_DWORD;
        int val = 0;
        DWORD size = sizeof(int);
        if (ERROR_SUCCESS == RegQueryValueEx(AvisynthKey, (*i).first,0,&type, (LPBYTE)&val, &size)) {
          if (v.IsInt())
            (*i).second = AVSValue(val);
          else 
            (*i).second = AVSValue(!!val);
        }
      } else if (v.IsFloat()) {
        DWORD type = REG_BINARY;
        double val;
        DWORD size = sizeof(double);
        if (ERROR_SUCCESS == RegQueryValueEx(AvisynthKey, (*i).first,0,&type, (LPBYTE)&val, &size))
          (*i).second = AVSValue(val);
      }
    }
  }
  RegCloseKey(AvisynthKey);
}