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


#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "internal.h"
#include "script.h"

#ifdef _MSC_VER
  #define strnicmp(a,b,c) _strnicmp(a,b,c)
  #define strdup(a) _strdup(a)
#else
  #define _RPT1(x,y,z) ((void)0)
#endif


extern AVSFunction Audio_filters[], Combine_filters[], Convert_filters[], 
                   Convolution_filters[], Edit_filters[], Field_filters[], 
                   Focus_filters[], Fps_filters[], Histogram_filters[], 
                   Layer_filters[], Levels_filters[], Misc_filters[], 
                   Plugin_functions[], Resample_filters[], Resize_filters[], 
                   Script_functions[], Source_filters[], Text_filters[],
                   Transform_filters[], Merge_filters[], Color_filters[],
                   Debug_filters[];
                   

AVSFunction* builtin_functions[] = {  
                   Audio_filters, Combine_filters, Convert_filters, 
                   Convolution_filters, Edit_filters, Field_filters, 
                   Focus_filters, Fps_filters, Histogram_filters, 
                   Layer_filters, Levels_filters, Misc_filters, 
                   Resample_filters, Resize_filters, 
                   Script_functions, Source_filters, Text_filters,
                   Transform_filters, Merge_filters, Color_filters, 
                   Debug_filters, Plugin_functions };



const HKEY RegRootKey = HKEY_LOCAL_MACHINE;
const char RegAvisynthKey[] = "Software\\Avisynth";
const char RegPluginDir[] = "PluginDir2_5";

// in plugins.cpp
AVSValue LoadPlugin(AVSValue args, void* user_data, IScriptEnvironment* env);
void FreeLibraries(void* loaded_plugins, IScriptEnvironment* env);



class LinkedVideoFrame {
public:
  LinkedVideoFrame* next;
  VideoFrame vf;
};

static LinkedVideoFrame* g_VideoFrame_recycle_bin = 0;

void* VideoFrame::operator new(unsigned) {
  // CriticalSection
  for (LinkedVideoFrame* i = g_VideoFrame_recycle_bin; i; i = i->next)
    if (i->vf.refcount == 0)
      return &i->vf;
  LinkedVideoFrame* result = (LinkedVideoFrame*)::operator new(sizeof(LinkedVideoFrame));
  result->next = g_VideoFrame_recycle_bin;
  g_VideoFrame_recycle_bin = result;
  return &result->vf;
}


VideoFrame::VideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height)
  : refcount(0), vfb(_vfb), offset(_offset), pitch(_pitch), row_size(_row_size), height(_height),offsetU(_offset),offsetV(_offset),pitchUV(0)  // PitchUV=0 so this doesn't take up additional space
{
  InterlockedIncrement(&vfb->refcount);
}

VideoFrame::VideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height, int _offsetU, int _offsetV, int _pitchUV)
  : refcount(0), vfb(_vfb), offset(_offset), pitch(_pitch), row_size(_row_size), height(_height),offsetU(_offsetU),offsetV(_offsetV),pitchUV(_pitchUV)
{
  InterlockedIncrement(&vfb->refcount);
}

VideoFrame* VideoFrame::Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height) const {
  return new VideoFrame(vfb, offset+rel_offset, new_pitch, new_row_size, new_height);
}

VideoFrame* VideoFrame::Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV) const {
  return new VideoFrame(vfb, offset+rel_offset, new_pitch, new_row_size, new_height, rel_offsetU+offsetU, rel_offsetV+offsetV, new_pitchUV);
}


VideoFrameBuffer::VideoFrameBuffer(int size)
 : refcount(0), data(new BYTE[size]), data_size(size) { InterlockedIncrement(&sequence_number); }

VideoFrameBuffer::VideoFrameBuffer() : refcount(0), data(0), data_size(0) {}

VideoFrameBuffer::~VideoFrameBuffer() {
//  _ASSERTE(refcount == 0);
  delete[] data;
}


class LinkedVideoFrameBuffer : public VideoFrameBuffer {
public:
  LinkedVideoFrameBuffer *prev, *next;
  LinkedVideoFrameBuffer(int size) : VideoFrameBuffer(size) { next=prev=this; }
  LinkedVideoFrameBuffer() { next=prev=this; }
};


class VarTable {
  VarTable* const dynamic_parent;
  VarTable* const lexical_parent;

  struct Variable {
    Variable* next;
    const char* const name;
    AVSValue val;
    Variable(const char* _name, Variable* _next) : name(_name), next(_next) {}
  };

  Variable variables;   // first entry is "last"

public:
  VarTable(VarTable* _dynamic_parent, VarTable* _lexical_parent)
    : dynamic_parent(_dynamic_parent), lexical_parent(_lexical_parent), variables("last", 0) {}

  ~VarTable() {
    Variable* v = variables.next;
    while (v) {
      Variable* next = v->next;
      delete v;
      v = next;
    }
  }

  VarTable* Pop() {
    VarTable* _dynamic_parent = this->dynamic_parent;
    delete this;
    return _dynamic_parent;
  }

  const AVSValue& Get(const char* name) {
    for (Variable* v = &variables; v; v = v->next)
      if (!lstrcmpi(name, v->name))
        return v->val;
    if (lexical_parent)
      return lexical_parent->Get(name);
    else
      throw IScriptEnvironment::NotFound();
  }

  bool Set(const char* name, const AVSValue& val) {
    for (Variable* v = &variables; v; v = v->next)
      if (!lstrcmpi(name, v->name)) {
        v->val = val;
        return false;
      }
    variables.next = new Variable(name, variables.next);
    variables.next->val = val;
    return true;
  }
};


class FunctionTable {
  struct LocalFunction : AVSFunction {
    LocalFunction* prev;
  };

  struct Plugin {
    char* name;
    LocalFunction* plugin_functions;
    Plugin* prev;
  };

  LocalFunction* local_functions;
  Plugin* plugins;
  bool prescanning, reloading;

  IScriptEnvironment* const env;

public:

  FunctionTable(IScriptEnvironment* _env) : env(_env), prescanning(false), reloading(false) {
    local_functions = 0;
    plugins = 0;
  }

  ~FunctionTable() {
    while (local_functions) {
      LocalFunction* prev = local_functions->prev;
      delete local_functions;
      local_functions = prev;
    }
    while (plugins) {
      RemovePlugin(plugins);
    }
  }

  void StartPrescanning() { prescanning = true; }
  void StopPrescanning() { prescanning = false; }

  void PrescanPluginStart(const char* name)
  {
    if (!prescanning)
      env->ThrowError("FunctionTable: Not in prescanning state");
    _RPT1(0, "Prescanning plugin: %s\n", name);
    Plugin* p = new Plugin;
    p->name = strdup(name);
    p->plugin_functions = 0;
    p->prev = plugins;
    plugins = p;
  }

  void RemovePlugin(Plugin* p)
  {
    LocalFunction* cur = p->plugin_functions;
    while (cur) {
      LocalFunction* prev = cur->prev;
      free((void*)cur->name);
      free((void*)cur->param_types);
      delete cur;
      cur = prev;
    }
    if (p == plugins) {
      plugins = plugins->prev;
    } else {
      Plugin* pp = plugins;
      while (pp->prev != p) pp = pp->prev;
      pp->prev = p->prev;
    }
    free(p->name);
    delete p;
  }

  void AddFunction(const char* name, const char* params, IScriptEnvironment::ApplyFunc apply, void* user_data) {
    if (prescanning && !plugins)
      env->ThrowError("FunctionTable in prescanning state but no plugin has been set");
    LocalFunction* f = new LocalFunction;
    if (!prescanning) {
      f->name = name;
      f->param_types = params;
      f->apply = apply;
      f->user_data = user_data;
      f->prev = local_functions;
      local_functions = f;
    } else {
      _RPT1(0, "  function %s (prescan)\n", name);
      f->name = strdup(name);     // needs to copy here since the plugin will be unloaded
      f->param_types = strdup(params);
      f->prev = plugins->plugin_functions;
      plugins->plugin_functions = f;
    }
  }

  AVSFunction* Lookup(const char* search_name, const AVSValue* args, int num_args, bool* pstrict) {
    for (int strict = 1; strict >= 0; --strict) {
      *pstrict = strict&1;
      // first, look in loaded plugins
      for (LocalFunction* p = local_functions; p; p = p->prev)
        if (!lstrcmpi(p->name, search_name) && TypeMatch(p->param_types, args, num_args, strict&1, env))
          return p;
      // now looks in prescanned plugins
      for (Plugin* pp = plugins; pp; pp = pp->prev)
        for (LocalFunction* p = pp->plugin_functions; p; p = p->prev)
          if (!lstrcmpi(p->name, search_name) && TypeMatch(p->param_types, args, num_args, strict&1, env)) {
            _RPT2(0, "Loading plugin %s (lookup for function %s)\n", pp->name, p->name);
            // sets reloading in case the plugin is performing env->FunctionExists() calls
            reloading = true;
            LoadPlugin(AVSValue(&AVSValue(&AVSValue(pp->name), 1), 1), (void*)false, env);
            reloading = false;
            // just in case the function disappeared from the plugin, avoid infinte recursion
            RemovePlugin(pp);
            // restart the search
            return Lookup(search_name, args, num_args, pstrict);
          }
      // finally, look for a built-in function
      for (int i = 0; i < sizeof(builtin_functions)/sizeof(builtin_functions[0]); ++i)
        for (AVSFunction* j = builtin_functions[i]; j->name; ++j)
          if (!lstrcmpi(j->name, search_name) && TypeMatch(j->param_types, args, num_args, strict&1, env))
            return j;
    }
    return 0;
  }

  bool Exists(const char* search_name) {
    for (LocalFunction* p = local_functions; p; p = p->prev)
      if (!lstrcmpi(p->name, search_name))
        return true;
    if (!reloading) {
      for (Plugin* pp = plugins; pp; pp = pp->prev)
        for (LocalFunction* p = pp->plugin_functions; p; p = p->prev)
          if (!lstrcmpi(p->name, search_name))
            return true;
    }
    for (int i = 0; i < sizeof(builtin_functions)/sizeof(builtin_functions[0]); ++i)
      for (AVSFunction* j = builtin_functions[i]; j->name; ++j)
        if (!lstrcmpi(j->name, search_name))
          return true;
    return false;
  }

  static bool SingleTypeMatch(char type, const AVSValue& arg, bool strict) {
    switch (type) {
      case '.': return true;
      case 'b': return arg.IsBool();
      case 'i': return arg.IsInt();
      case 'f': return arg.IsFloat() && (!strict || !arg.IsInt());
      case 's': return arg.IsString();
      case 'c': return arg.IsClip();
      default:  return false;
    }
  }

  bool TypeMatch(const char* param_types, const AVSValue* args, int num_args, bool strict, IScriptEnvironment* env) {

    bool optional = false;

    for (int i=0; i<num_args; ++i) {

      if (*param_types == 0) {
        // more args than params
        return false;
      }

      if (*param_types == '[') {
        // named arg: skip over the name
        param_types = strchr(param_types+1, ']');
        if (!param_types)
          env->ThrowError("TypeMatch: unterminated parameter name (bug in filter)");
        ++param_types;
        optional = true;
      }

      if (param_types[1] == '*') {
        // skip over initial test of type for '*' (since zero matches is ok)
        ++param_types;
      }

      switch (*param_types) {
        case 'b': case 'i': case 'f': case 's': case 'c':
          if ((!optional || args[i].Defined()) && !SingleTypeMatch(*param_types, args[i], strict))
            return false;
          // fall thru
        case '.':
          ++param_types;
          break;
        case '+': case '*':
          if (!SingleTypeMatch(param_types[-1], args[i], strict)) {
            ++param_types;
            --i;
          }
          break;
        default:
          env->ThrowError("TypeMatch: invalid character in parameter list (bug in filter)");
      }
    }

    // We're out of args.  We have a match if one of the following is true:
    // (a) we're out of params; (b) remaining params are named; (c) we're
    // at a '+' or '*'.
    return (*param_types == 0 || *param_types == '[' || *param_types == '+' || *param_types == '*');
  }
};


// This doles out storage space for strings.  No space is ever freed
// until the class instance is destroyed (which happens when a script
// file is closed).
class StringDump {
  enum { BLOCK_SIZE = 4096 };
  char* current_block;
  int block_pos, block_size;

public:
  StringDump() : current_block(0), block_pos(BLOCK_SIZE), block_size(BLOCK_SIZE) {}
  ~StringDump();
  char* SaveString(const char* s, int len = -1);
};

StringDump::~StringDump() {
  char* p = current_block;
  while (p) {
    char* next = *(char**)p;
    delete[] p;
    p = next;
  }
}

char* StringDump::SaveString(const char* s, int len) {
  if (len == -1)
    len = lstrlen(s);
  if (block_pos+len+1 > block_size) {
    char* new_block = new char[block_size = max(block_size, len+1)];
    *(char**)new_block = current_block;   // beginning of block holds pointer to previous block
    current_block = new_block;
    block_pos = sizeof(char*);
  }
  char* result = current_block+block_pos;
  memcpy(result, s, len);
  result[len] = 0;
  block_pos += len+1;
  return result;
}


class AtExiter {
  IScriptEnvironment* const env;
  struct AtExitRec {
    const IScriptEnvironment::ShutdownFunc func;
    void* const user_data;
    AtExitRec* const next;
    AtExitRec(IScriptEnvironment::ShutdownFunc _func, void* _user_data, AtExitRec* _next)
      : func(_func), user_data(_user_data), next(_next) {}
  };
  AtExitRec* atexit_list;
public:
  AtExiter(IScriptEnvironment* _env) : env(_env) {
    atexit_list = 0;
  }
  void Add(IScriptEnvironment::ShutdownFunc f, void* d) {
    atexit_list = new AtExitRec(f, d, atexit_list);
  }
  ~AtExiter() {
    while (atexit_list) {
      AtExitRec* next = atexit_list->next;
      atexit_list->func(atexit_list->user_data, env);
      delete atexit_list;
      atexit_list = next;
    }
  }
};


class ScriptEnvironment : public IScriptEnvironment {
public:
  ScriptEnvironment();
  void __stdcall CheckVersion(int version);
  long __stdcall GetCPUFlags();
  char* __stdcall SaveString(const char* s, int length = -1);
  char* __stdcall Sprintf(const char* fmt, ...);
  char* __stdcall VSprintf(const char* fmt, void* val);
  void __stdcall ThrowError(const char* fmt, ...);
  void __stdcall AddFunction(const char* name, const char* params, ApplyFunc apply, void* user_data=0);
  bool __stdcall FunctionExists(const char* name);
  AVSValue __stdcall Invoke(const char* name, const AVSValue args, const char** arg_names=0);
  AVSValue __stdcall GetVar(const char* name);
  bool __stdcall SetVar(const char* name, const AVSValue& val);
  bool __stdcall SetGlobalVar(const char* name, const AVSValue& val);
  void __stdcall PushContext(int level=0);
  void __stdcall PopContext();
  PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, int align);
  PVideoFrame NewVideoFrame(int row_size, int height, int align);
  PVideoFrame NewPlanarVideoFrame(int width, int height, int align, bool U_first);
  bool __stdcall MakeWritable(PVideoFrame* pvf);
  void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height);
  void __stdcall AtExit(IScriptEnvironment::ShutdownFunc function, void* user_data);
  PVideoFrame __stdcall Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height);
  int __stdcall SetMemoryMax(int mem);
  int __stdcall SetWorkingDir(const char * newdir);
  __stdcall ~ScriptEnvironment();

private:

  AtExiter at_exit;

  StringDump string_dump;

  FunctionTable function_table;

  VarTable global_var_table;
  VarTable* var_table;

  LinkedVideoFrameBuffer video_frame_buffers;
  int memory_max, memory_used;

  LinkedVideoFrameBuffer* NewFrameBuffer(int size);

  LinkedVideoFrameBuffer* GetFrameBuffer2(int size);
  VideoFrameBuffer* GetFrameBuffer(int size);

  // helper for Invoke
  int Flatten(const AVSValue& src, AVSValue* dst, int index, int max, const char** arg_names=0);

  IScriptEnvironment* This() { return this; }
  const char* GetPluginDirectory();
  bool LoadPluginsMatching(const char* pattern);
  void PrescanPlugins();

};


ScriptEnvironment::ScriptEnvironment() : at_exit(This()), function_table(This()), global_var_table(0, 0) {
  MEMORYSTATUS memstatus;
  GlobalMemoryStatus(&memstatus);
  memory_max = max(memstatus.dwAvailPhys / 4,16*1024*1024);   // Minimum 16MB, otherwise available physical memory/4, no maximum
  memory_used = 0;
  var_table = new VarTable(0, &global_var_table);
  global_var_table.Set("true", true);
  global_var_table.Set("false", false);
  global_var_table.Set("yes", true);
  global_var_table.Set("no", false);
  
  PrescanPlugins();
}

ScriptEnvironment::~ScriptEnvironment() {
  while (var_table)
    PopContext();
  LinkedVideoFrameBuffer* i = video_frame_buffers.prev;
  while (i != &video_frame_buffers) {
    LinkedVideoFrameBuffer* prev = i->prev;
    delete i;
    i = prev;
  }
}

int ScriptEnvironment::SetMemoryMax(int mem) {
  MEMORYSTATUS memstatus;
  GlobalMemoryStatus(&memstatus);
  memory_max = min(max(memory_used,mem*1024*1024),memory_used+memstatus.dwAvailPhys-5*1024*1024);
  return memory_max/(1024*1024);
}

int ScriptEnvironment::SetWorkingDir(const char * newdir) {
  return SetCurrentDirectory(newdir) ? 0 : 1;
}

void ScriptEnvironment::CheckVersion(int version) {
  if (version > AVISYNTH_INTERFACE_VERSION)
    ThrowError("Plugin was designed for a later version of Avisynth (%d)", version);
}

extern long CPUCheckForExtensions();  // in cpuaccel.cpp

long GetCPUFlags() {
  static long lCPUExtensionsAvailable = CPUCheckForExtensions();
  return lCPUExtensionsAvailable;
}

long ScriptEnvironment::GetCPUFlags() { return ::GetCPUFlags(); }

void ScriptEnvironment::AddFunction(const char* name, const char* params, ApplyFunc apply, void* user_data) {
  function_table.AddFunction(name, params, apply, user_data);
}

AVSValue ScriptEnvironment::GetVar(const char* name) {
  return var_table->Get(name);
}

bool ScriptEnvironment::SetVar(const char* name, const AVSValue& val) {
  return var_table->Set(name, val);
}

bool ScriptEnvironment::SetGlobalVar(const char* name, const AVSValue& val) {
  return global_var_table.Set(name, val);
}

const char* ScriptEnvironment::GetPluginDirectory()
{
  char* plugin_dir;
  try {
    plugin_dir = (char*)GetVar("$PluginDir$").AsString();
  }
  catch (...) {
    HKEY AvisynthKey;
    if (RegOpenKeyEx(RegRootKey, RegAvisynthKey, 0, KEY_READ, &AvisynthKey))
      return 0;
    DWORD size;
    if (RegQueryValueEx(AvisynthKey, RegPluginDir, 0, 0, 0, &size))
      return 0;
    plugin_dir = new char[size];
    if (RegQueryValueEx(AvisynthKey, RegPluginDir, 0, 0, (LPBYTE)plugin_dir, &size)) {
      delete[] plugin_dir;
      return 0;
    }
    // remove trailing backslashes
    int l = strlen(plugin_dir);
    while (plugin_dir[l-1] == '\\')
      l--;
    plugin_dir[l]=0;
    SetGlobalVar("$PluginDir$", AVSValue(plugin_dir));
  }
  return plugin_dir;
}

bool ScriptEnvironment::LoadPluginsMatching(const char* pattern)
{
  WIN32_FIND_DATA FileData;
  char file[MAX_PATH];
  char* dummy;
//  const char* plugin_dir = GetPluginDirectory();

//  strcpy(file, plugin_dir);
//  strcat(file, "\\");
//  strcat(file, pattern);
  HANDLE hFind = FindFirstFile(pattern, &FileData);
  BOOL bContinue = (hFind != INVALID_HANDLE_VALUE);
  while (bContinue) {
    // we have to use full pathnames here
    GetFullPathName(FileData.cFileName, MAX_PATH, file, &dummy);
    function_table.PrescanPluginStart(file);
    LoadPlugin(AVSValue(&AVSValue(&AVSValue(file), 1), 1), (void*)true, this);
    bContinue = FindNextFile(hFind, &FileData);
    if (!bContinue) {
      FindClose(hFind);
      return true;
    }
  }
  return false;
}

void ScriptEnvironment::PrescanPlugins()
{
  const char* plugin_dir;
  if (plugin_dir = GetPluginDirectory())
  {
    WIN32_FIND_DATA FileData;
    HANDLE hFind = FindFirstFile(plugin_dir, &FileData);
    if (hFind != INVALID_HANDLE_VALUE) {    
      FindClose(hFind);
      CWDChanger cwdchange(plugin_dir);
      function_table.StartPrescanning();
      if (LoadPluginsMatching("*.dll") | LoadPluginsMatching("*.vdf")) {  // not || because of shortcut boolean eval.
        // Unloads all plugins
        HMODULE* loaded_plugins = (HMODULE*)GetVar("$Plugins$").AsString();
        FreeLibraries(loaded_plugins, this);
      }
      function_table.StopPrescanning();

      char file[MAX_PATH];
      strcpy(file, plugin_dir);
      strcat(file, "\\*.avsi");
      hFind = FindFirstFile(file, &FileData);
      BOOL bContinue = (hFind != INVALID_HANDLE_VALUE);
      while (bContinue) {
        Import(AVSValue(&AVSValue(&AVSValue(FileData.cFileName), 1), 1), 0, this);
        bContinue = FindNextFile(hFind, &FileData);
        if (!bContinue) FindClose(hFind);
      }
    }
  }
}

PVideoFrame ScriptEnvironment::NewPlanarVideoFrame(int width, int height, int align, bool U_first) {
  int pitch = (width+align-1) / align * align;  // Y plane, width = 1 byte per pixel
//  int UVpitch = ((width>>1)+align-1) / align * align;  // UV plane, width = 1/2 byte per pixel - can't align UV planes seperately.
  int UVpitch = pitch>>1;  // UV plane, width = 1/2 byte per pixel
  int size = pitch * height + UVpitch * height;
  VideoFrameBuffer* vfb = GetFrameBuffer(size+(FRAME_ALIGN*4));
  if (!vfb)
    ThrowError("NewPlanarVideoFrame: Returned 0 size image!"); 
#ifdef _DEBUG
  {
    static const BYTE filler[] = { 0x0A, 0x11, 0x0C, 0xA7, 0xED };
    BYTE* p = vfb->GetWritePtr();
    BYTE* q = p + vfb->GetDataSize()/5*5;
    for (; p<q; p+=5) {
      p[0]=filler[0]; p[1]=filler[1]; p[2]=filler[2]; p[3]=filler[3]; p[4]=filler[4];
    }
  }
#endif
//  int offset = (-int(vfb->GetWritePtr())) & (align-1);  // align first line offset
  int offset = int(vfb->GetWritePtr()) & (FRAME_ALIGN-1);  // align first line offset
  offset = (FRAME_ALIGN - offset)%FRAME_ALIGN;
  int Uoffset, Voffset;
  if (U_first) {
    Uoffset = offset + pitch * height;
    Voffset = offset + pitch * height + UVpitch * (height>>1);
  } else {
    Voffset = offset + pitch * height;
    Uoffset = offset + pitch * height + UVpitch * (height>>1);
  }
  return new VideoFrame(vfb, offset, pitch, width, height, Uoffset, Voffset, UVpitch);
}


PVideoFrame ScriptEnvironment::NewVideoFrame(int row_size, int height, int align) {
  int pitch = (row_size+align-1) / align * align;
  int size = pitch * height;
  VideoFrameBuffer* vfb = GetFrameBuffer(size+(align*4));
#ifdef _DEBUG
  {
    static const BYTE filler[] = { 0x0A, 0x11, 0x0C, 0xA7, 0xED };
    BYTE* p = vfb->GetWritePtr();
    BYTE* q = p + vfb->GetDataSize()/5*5;
    for (; p<q; p+=5) {
      p[0]=filler[0]; p[1]=filler[1]; p[2]=filler[2]; p[3]=filler[3]; p[4]=filler[4];
    }
  }
#endif
  int offset = int(vfb->GetWritePtr()) & (FRAME_ALIGN-1);  // align first line offset
  offset = (FRAME_ALIGN - offset)%FRAME_ALIGN;
//  int offset = (-int(vfb->GetWritePtr())) & (align-1);  // align first line offset  (alignment is free here!)
  return new VideoFrame(vfb, offset, pitch, row_size, height);
}


PVideoFrame __stdcall ScriptEnvironment::NewVideoFrame(const VideoInfo& vi, int align) { 
  // Check requested pixel_type:
  switch (vi.pixel_type) {
    case VideoInfo::CS_BGR24:
    case VideoInfo::CS_BGR32:
    case VideoInfo::CS_YUY2:
    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:
      break;
    default:
      ThrowError("Filter Error: Filter attempted to create VideoFrame with invalid pixel_type.");
  }
  // If align is negative, it will be forced, if not it may be made bigger
  if (vi.IsPlanar()) { // Planar requires different math ;)
    if (align<0) {
      align *= -1;
    } else {
      align = max(align,FRAME_ALIGN);
    }
    if ((vi.height&1)||(vi.width&1))
      ThrowError("Filter Error: Attempted to request an YV12 frame that wasn't mod2 in width and height!");
    if ((vi.height&3)&&vi.IsFieldBased())
      ThrowError("Filter Error: Attempted to request a fieldbased YV12 frame that wasn't mod4 in height!");
    return ScriptEnvironment::NewPlanarVideoFrame(vi.width, vi.height, align, !vi.IsVPlaneFirst());  // If planar, maybe swap U&V
  } else {
    if ((vi.width&1)&&(vi.IsYUY2()))
      ThrowError("Filter Error: Attempted to request an YUY2 frame that wasn't mod2 in width.");
    if (align<0) {
      align *= -1;
    } else {
      align = max(align,FRAME_ALIGN);
    }
    return ScriptEnvironment::NewVideoFrame(vi.RowSize(), vi.height, align);
  }
}

bool ScriptEnvironment::MakeWritable(PVideoFrame* pvf) {
  const PVideoFrame& vf = *pvf;
  // If the frame is already writable, do nothing.
  if (vf->IsWritable()) {
    return false;
  }

  // Otherwise, allocate a new frame (using NewVideoFrame) and
  // copy the data into it.  Then modify the passed PVideoFrame
  // to point to the new buffer.
    const int row_size = vf->GetRowSize(); 
    const int height = vf->GetHeight();
    PVideoFrame dst;
    if (vf->GetPitch(PLANAR_U)) {  // we have no videoinfo, so we can only assume that it is Planar
      dst = NewPlanarVideoFrame(row_size, height, FRAME_ALIGN,false);  // Always V first on internal images
    } else {
      dst = NewVideoFrame(row_size, height, FRAME_ALIGN);
    }
    BitBlt(dst->GetWritePtr(), dst->GetPitch(), vf->GetReadPtr(), vf->GetPitch(), row_size, height);
    // Blit More planes (pitch, rowsize and height should be 0, if none is present)
    BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), vf->GetReadPtr(PLANAR_V), vf->GetPitch(PLANAR_V), vf->GetRowSize(PLANAR_V), vf->GetHeight(PLANAR_V));
    BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), vf->GetReadPtr(PLANAR_U), vf->GetPitch(PLANAR_U), vf->GetRowSize(PLANAR_U), vf->GetHeight(PLANAR_U));

    *pvf = dst;
    return true;
}


void ScriptEnvironment::AtExit(IScriptEnvironment::ShutdownFunc function, void* user_data) {
  at_exit.Add(function, user_data);
}

void ScriptEnvironment::PushContext(int level) {
  var_table = new VarTable(var_table, &global_var_table);
}

void ScriptEnvironment::PopContext() {
  var_table = var_table->Pop();
}

PVideoFrame __stdcall ScriptEnvironment::Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height) {
  return src->Subframe(rel_offset, new_pitch, new_row_size, new_height);
}

LinkedVideoFrameBuffer* ScriptEnvironment::NewFrameBuffer(int size) {
  memory_used += size;
  _RPT1(0, "Frame buffer memory used: %d\n", memory_used);
  return new LinkedVideoFrameBuffer(size);
}

LinkedVideoFrameBuffer* ScriptEnvironment::GetFrameBuffer2(int size) {
  // Plan A: are we below our memory usage?  If so, allocate a new buffer
  if (memory_used + size < memory_max)
    return NewFrameBuffer(size);
  // Plan B: look for an existing buffer of the appropriate size
  for (LinkedVideoFrameBuffer* i = video_frame_buffers.prev; i != &video_frame_buffers; i = i->prev) {
    if (i->GetRefcount() == 0 && i->GetDataSize() == size)
      return i;
  }

  // Plan C: allocate a new buffer, regardless of current memory usage  
  // FIXME: Could lead to massive allocation on memory thrashing, perhaps a plan D which deallocates all unused frames and reallocates memory would be better, if more than 2*allowed memory is used.
  return NewFrameBuffer(size);
}

VideoFrameBuffer* ScriptEnvironment::GetFrameBuffer(int size) {
  LinkedVideoFrameBuffer* result = GetFrameBuffer2(size);
  Relink(&video_frame_buffers, result, video_frame_buffers.next);
  return result;
}


int ScriptEnvironment::Flatten(const AVSValue& src, AVSValue* dst, int index, int max, const char** arg_names) {
  if (src.IsArray()) {
    const int array_size = src.ArraySize();
    for (int i=0; i<array_size; ++i) {
      if (!arg_names || arg_names[i] == 0)
        index = Flatten(src[i], dst, index, max);
    }
  } else {
    if (index < max) {
      dst[index++] = src;
    } else {
      ThrowError("Too many arguments passed to function (max. is %d)", max);
    }
  }
  return index;
}


AVSValue ScriptEnvironment::Invoke(const char* name, const AVSValue args, const char** arg_names) {
  // flatten unnamed args
  AVSValue args2[60];
  int args2_count = Flatten(args, args2, 0, 60, arg_names);

  // find matching function
  bool strict;
  AVSFunction* f = function_table.Lookup(name, args2, args2_count, &strict);
  if (!f)
    throw NotFound();

  // combine unnamed args into arrays
  AVSValue args3[60];
  const char* p = f->param_types;
  int src_index=0, dst_index=0;
  while (*p) {
    if (*p == '[') {
      p = strchr(p+1, ']');
      if (!p) break;
      p++;
    } else if (p[1] == '*' || p[1] == '+') {
      int start = src_index;
      while (src_index < args2_count && FunctionTable::SingleTypeMatch(*p, args2[src_index], strict))
        src_index++;
      args3[dst_index++] = AVSValue(&args2[start], src_index - start);
      p += 2;
    } else {
      if (src_index < args2_count)
        args3[dst_index] = args2[src_index];
      src_index++;
      dst_index++;
      p++;
    }
  }
  if (src_index < args2_count)
    ThrowError("Too many arguments to function %s", name);

  const int args3_count = dst_index;

  // copy named args
  if (args.IsArray() && arg_names) {
    const int array_size = args.ArraySize();
    for (int i=0; i<array_size; ++i) {
      if (arg_names[i]) {
        int named_arg_index = 0;
        for (const char* p = f->param_types; *p; ++p) {
          if (*p == '*' || *p == '+') {
            continue;   // without incrementing named_arg_index
          } else if (*p == '[') {
            p += 1;
            const char* q = strchr(p, ']');
            if (!q) break;
            if (strlen(arg_names[i]) == unsigned(q-p) && !strnicmp(arg_names[i], p, q-p)) {
              // we have a match
              if (args3[named_arg_index].Defined()) {
                ThrowError("Script error: the named argument \"%s\" was passed more than once to %s", arg_names[i], name);
              } else if (args[i].IsArray()) {
                ThrowError("Script error: can't pass an array as a named argument");
              } else if (args[i].Defined() && !FunctionTable::SingleTypeMatch(q[1], args[i], false)) {
                ThrowError("Script error: the named argument \"%s\" to %s had the wrong type", arg_names[i], name);
              } else {
                args3[named_arg_index] = args[i];
                goto success;
              }
            } else {
              p = q+1;
            }
          }
          named_arg_index++;
        }
        // failure
        ThrowError("Script error: %s does not have a named argument \"%s\"", name, arg_names[i]);
success:;
      }
    }
  }

  // ... and we're finally ready to make the call
  return f->apply(AVSValue(args3, args3_count), f->user_data, this);
}


bool ScriptEnvironment::FunctionExists(const char* name) {
  return function_table.Exists(name);
}

void BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
  if (GetCPUFlags() & CPUF_MMX) {
    asm_BitBlt(dstp,dst_pitch,srcp,src_pitch,row_size,height);
    return;
  }
  if (dst_pitch == src_pitch && src_pitch == row_size) {
    memcpy(dstp, srcp, src_pitch * height);
  } else {
    for (int y=height; y>0; --y) {
      memcpy(dstp, srcp, row_size);
      dstp += dst_pitch;
      srcp += src_pitch;
    }
  }
}

  /*****************************
  * Assembler bitblit by Steady
   *****************************/

#if 0

void asm_BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
	if(row_size==0 || height==0) return; //abort on goofs
	//move backwards for easier looping and to disable hardware prefetch
	const BYTE* srcStart=srcp+src_pitch*(height-1);
	BYTE* dstStart=dstp+dst_pitch*(height-1);

	if(row_size < 16) {//to small to optimize
		_asm {
			mov 	esi,srcStart	//move rows from bottom up
			mov 	edi,dstStart
			mov 	edx,row_size
			dec		edx
			mov 	ebx,height
			align 16
memoptS_rowloop:
			mov 	ecx,edx
//			rep movsb
memoptS_byteloop:
			mov		AL,[esi+ecx]
			mov		[edi+ecx],AL
			sub		ecx,1
			jnc		memoptS_byteloop
			sub 	esi,src_pitch
			sub 	edi,dst_pitch
			dec 	ebx
			jne 	memoptS_rowloop
		};
		return;
	}//end small version

	else if( (int(dstp) | row_size) & 7) {//not QW aligned
		//unaligned version makes no assumptions on alignment
		//other than pitch is QW aligned
		int preBytes;// 0-7 bytes before first QW

		_asm {
//****** initialize
			mov		esi,srcStart	//bottom row
			mov		AL,[esi]
			mov		edi,dstStart
			mov		edx,row_size
			mov		ecx,edi
			neg		ecx
			and		ecx,7		//bytes before first QWord in dest
			mov		preBytes,ecx
			mov		ebx,height

//********** loop starts here ***********

			align 16
memoptU_rowloop:
			mov		ecx,edx			//row_size
			dec		ecx					//offset to last byte in row
			add		ecx,esi			//ecx= ptr last byte in row
			and		ecx,~63			//align to first byte in cache line
memoptU_prefetchloop:
			mov		AX,[ecx]		//tried AL,AX,EAX, AX a tiny bit faster
			sub		ecx,64
			cmp		ecx,esi
			jae		memoptU_prefetchloop

//************ write *************

			movq		mm6,[esi]			//move the first unaligned bytes
			movntq	[edi],mm6			//if preBytes!=0, overlaps with QW aligned below
//************************
			mov		eax,edi
			mov		ecx,preBytes		//preBytes=offset from edi(dstp) to next QW (0-7)
			neg		eax
			and		eax,63				//eax=bytes from edi to start of next 64 byte cache line
			align	16
memoptU_prewrite8loop:   //write out odd QW's so 64bit write is cache line aligned
			cmp		ecx,eax				//start of cache line ?
			jz		memoptU_pre8done	//if not, write single QW
			movq		mm7,[esi+ecx]
			movntq	[edi+ecx],mm7
			add		ecx,8
			jmp		memoptU_prewrite8loop

			align	16
memoptU_write64loop:
			movntq	[edi+ecx-64],mm0
			movntq	[edi+ecx-56],mm1
			movntq	[edi+ecx-48],mm2
			movntq	[edi+ecx-40],mm3
			movntq	[edi+ecx-32],mm4
			movntq	[edi+ecx-24],mm5
			movntq	[edi+ecx-16],mm6
			movntq	[edi+ecx- 8],mm7
memoptU_pre8done:
			add		ecx,64
			cmp		ecx,edx		//while(offset <= row_size) do {...
			ja		memoptU_done64
			movq		mm0,[esi+ecx-64]
			movq		mm1,[esi+ecx-56]
			movq		mm2,[esi+ecx-48]
			movq		mm3,[esi+ecx-40]
			movq		mm4,[esi+ecx-32]
			movq		mm5,[esi+ecx-24]
			movq		mm6,[esi+ecx-16]
			movq		mm7,[esi+ecx- 8]
			jmp		memoptU_write64loop
memoptU_done64:

			sub			ecx,64		//went to far
			align	16
memoptU_write8loop:
			add			ecx,8		//next QW
			cmp			ecx,edx	//any QW's left in row ?
			ja			memoptU_done8
			movq		mm0,[esi+ecx-8]
			movntq	[edi+ecx-8],mm0
			jmp		memoptU_write8loop
memoptU_done8:

			movq		mm1,[esi+edx-8]
			movntq	[edi+edx-8],mm1
			sub		esi,src_pitch
			sub		edi,dst_pitch
			dec		ebx							//row counter (=height at start)
			jne		memoptU_rowloop

			sfence
			emms
		};
		return;
	}//end unaligned version

	else {//QW aligned version (fastest)
	//else dstp and row_size QW aligned - hope for the best from srcp
	//QW aligned version should generally be true when copying full rows
		_asm {
			mov		esi,srcStart	//start of bottom row
			mov		edi,dstStart
			mov		ebx,height
			mov		edx,row_size
			align	16
memoptA_rowloop:
			mov		ecx,edx	//row_size
			dec		ecx			//offset to last byte in row

//********forward routine
			add		ecx,esi
			and		ecx,~63		//align prefetch to first byte in cache line(~3-4% faster)
			align	16
memoptA_prefetchloop:
			mov		AX,[ecx]
			sub		ecx,64
			cmp		ecx,esi
			jae		memoptA_prefetchloop

			mov		eax,edi
			xor		ecx,ecx
			neg		eax
			and		eax,63		//eax=bytes from edi to start of cache line
			align	16
memoptA_prewrite8loop:   //write out odd QW's so 64bit write is cache line aligned
			cmp		ecx,eax				//start of cache line ?
			jz		memoptA_pre8done	//if not, write single QW
			movq		mm7,[esi+ecx]
			movntq	[edi+ecx],mm7
			add		ecx,8
			jmp		memoptA_prewrite8loop

			align	16
memoptA_write64loop:
			movntq	[edi+ecx-64],mm0
			movntq	[edi+ecx-56],mm1
			movntq	[edi+ecx-48],mm2
			movntq	[edi+ecx-40],mm3
			movntq	[edi+ecx-32],mm4
			movntq	[edi+ecx-24],mm5
			movntq	[edi+ecx-16],mm6
			movntq	[edi+ecx- 8],mm7
memoptA_pre8done:
			add		ecx,64
			cmp		ecx,edx
			ja		memoptA_done64  //less than 64 bytes left
			movq		mm0,[esi+ecx-64]
			movq		mm1,[esi+ecx-56]
			movq		mm2,[esi+ecx-48]
			movq		mm3,[esi+ecx-40]
			movq		mm4,[esi+ecx-32]
			movq		mm5,[esi+ecx-24]
			movq		mm6,[esi+ecx-16]
			movq		mm7,[esi+ecx- 8]
			jmp		memoptA_write64loop

memoptA_done64:
			sub		ecx,64

			align	16
memoptA_write8loop:		//less than 8 QW's left
			add		ecx,8
			cmp		ecx,edx
			ja		memoptA_done8	//no QW's left
			movq		mm7,[esi+ecx-8]
			movntq	[edi+ecx-8],mm7
			jmp		memoptA_write8loop

memoptA_done8:
			sub		esi,src_pitch
			sub		edi,dst_pitch
			dec		ebx		//row counter (height)
			jne		memoptA_rowloop

			sfence
			emms
		};
		return;
	}//end aligned version

}//end BitBlt_memopt()

#else

void asm_BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
  int bytesleft=0;
  if (row_size&15) {
    int a=(row_size+15)&(~15);
    if ((a<=src_pitch) && (a<=dst_pitch)) {
      row_size=a;
    } else {
      bytesleft=(row_size&15);
      row_size&=~15;
    }
  }
  int src_modulo = src_pitch - (row_size+bytesleft);
  int dst_modulo = dst_pitch - (row_size+bytesleft);
  if (height==0 || row_size==0) return;
  if ((GetCPUFlags() & CPUF_INTEGER_SSE)) {  // ISSE ~10-15% faster on K7 in reallife tests
    __asm {
      mov edi,[dstp]
      mov esi,[srcp]
      xor eax, eax;  // Height counter
      xor ebx, ebx;  // Row counter
      mov edx, [row_size]
new_line_sse:
      mov ecx,[bytesleft]
      cmp ebx,edx
      jge nextline_sse
      align 16
nextpixels_sse:
      prefetchnta [esi+ebx+256]  // Prefetch a few cache lines ahead (~1% faster)
      movq mm0,[esi+ebx]
      movq mm1,[esi+ebx+8]
      movntq [edi+ebx],mm0      // Is faster in reallife on K7 (~4 %)
      movntq [edi+ebx+8],mm1
      add ebx,16
      cmp ebx,edx
      jl nextpixels_sse
      align 16
nextline_sse:
      add esi,ebx
      add edi,ebx
      cmp ecx,0
      jz do_next_line_sse
      rep movsb         ; the last 1-15 bytes

      align 16
do_next_line_sse:
      add esi, [src_modulo]
      add edi, [dst_modulo]
      xor ebx, ebx;  // Row counter
      inc eax
      cmp eax,[height]
      jl new_line_sse
      sfence
    }
  } else {  // Plain MMX ~ 5% faster on K7 in real life
    __asm {
      mov edi,[dstp]
      mov esi,[srcp]
      xor eax, eax;  // Height counter
      xor ebx, ebx;  // Row counter
      mov edx, [row_size]
new_line_mmx:
      mov ecx,[bytesleft]
      cmp ebx,edx
      jge nextline_mmx
      align 16
nextpixels_mmx:
      movq mm0,[esi+ebx]
      movq mm1,[esi+ebx+8]
      movq [edi+ebx],mm0
      movq [edi+ebx+8],mm1
      add ebx,16
      cmp ebx,edx
      jl nextpixels_mmx
      align 16
nextline_mmx:
      add esi,edx
      add edi,edx
      cmp ecx,0
      jz do_next_line_mmx
      rep movsb         ; the last 1-7 bytes

      align 16
do_next_line_mmx:
      add esi, [src_modulo]
      add edi, [dst_modulo]
      xor ebx, ebx;  // Row counter
      inc eax
      cmp eax,[height]
      jl new_line_mmx
    }
  }
  __asm {emms};
}
#endif

void asm_BitBltNC(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
}

void ScriptEnvironment::BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
  if (height<0)
    ThrowError("Filter Error: Attempting to blit an image with negative height.");
  if (row_size<0)
    ThrowError("Filter Error: Attempting to blit an image with negative row size.");
  ::BitBlt(dstp, dst_pitch, srcp, src_pitch, row_size, height);
}


char* ScriptEnvironment::SaveString(const char* s, int len) {
  return string_dump.SaveString(s, len);
}


char* ScriptEnvironment::VSprintf(const char* fmt, void* val) {
  char buf[4096];
  _vsnprintf(buf, 4096, fmt, (va_list)val);
  return ScriptEnvironment::SaveString(buf);
}


char* ScriptEnvironment::Sprintf(const char* fmt, ...) {
  va_list val;
  va_start(val, fmt);
  char* result = ScriptEnvironment::VSprintf(fmt, val);
  va_end(val);
  return result;
}


void ScriptEnvironment::ThrowError(const char* fmt, ...) {
  va_list val;
  va_start(val, fmt);
  char buf[8192];
  wvsprintf(buf, fmt, val);
  va_end(val);
  throw AvisynthError(ScriptEnvironment::SaveString(buf));
}


IScriptEnvironment* __stdcall CreateScriptEnvironment(int version) {
  if (version <= AVISYNTH_INTERFACE_VERSION)
    return new ScriptEnvironment;
  else
    return 0;
}
