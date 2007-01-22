#pragma once
#include "SoundOutput.h"
#include <map>

using namespace std;


typedef map<const char*, AVSValue,ltstr> Param;

class RegistryIO
{
public:
  RegistryIO(void);
public:
  virtual ~RegistryIO(void);
  static void StoreSettings(Param &params);
  static void RetrieveSettings(Param &params, IScriptEnvironment* env);
};
