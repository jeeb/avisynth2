// Avisynth v3.0 alpha.  Copyright 2002 Ben Rudiak-Gould et al.
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


#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <windows.h>
#include <string>
#include <utility>
using namespace std;

#include "avisynth.h"


struct Argument {

  AVSType type;
  string name;

  bool optional;
  AVSValue defaut;

  Argument(AVSType _type, const string& _name) : type(_type), name(_name), optional(false) { }
  Argument(AVSType _type, const string& _name, const AVSValue _defaut)
    : type(_type), name(_name), optional(true), defaut(_default) { _ASSERTE(type == default.GetType()); }

};

typedef vector<Argument> ArgumentList;
typedef pair<string, AVSValue> NamedArg;
typedef vector<NamedArg> NamedArgVector;


class AVSFunction {

  const AVSType returnType;
  const string name;
  const ArgumentList argList;

public:
  AVSFunction(AVSType retType, const string& _name, const ArgumentList& _argList)
    : returnType(retType), name(_name), argList(_argList) { }

  AVSType GetReturnType() const { return returnType; }
  const string& GetName() const { return name; }
  const ArgumentList& GetArgumentList() const { return argList; }

  //does all type checking, reorder, replace by default
  //and throw errors if args doesn't fit in
  AVSValue operator() (const NamedArgVector& args) const;

protected:
  //called by the above, quaranteed to have args according to specified prototype
  virtual AVSValue Process(const ArgVector& args) const = 0;

};


class Plugin;

class AVSFilter : public AVSFunction {

public:
  AVSFilter(const string& _name, const ArgumentList& _argList) : AVSFunction(CLIP_T, _name, _argList) { }

  virtual const Plugin * GetHostPlugin() const; //return the avsiynth core plugin (not really a plugin, but...

};


class AVSPluginFilter : public AVSFilter {

  Plugin * host;

  typedef PClip (* ProcessFunction)(const ArgVector& args);

  ProcessFunction pf;

  //method should be called by host plugin on all its filters when load is done
  void SetProcessFunction(ProcessFunction _pf) { pf = _pf; }

public:
  AVSPluginFilter(Plugin * _host, const string& _name, const ArgumentList& _argList);

  virtual const Plugin * GetHostPlugin() const { return host; }

protected:
  virtual AVSValue Process(const ArgVector& args) const; //load host if not and call pf


};



class Plugin {

public:
  virtual const string& GetName() const = 0;
  virtual const string& GetAuthor() const = 0;
  virtual const string& GetDescription() const = 0;

  virtual bool IsLoaded() const = 0;

  virtual void Load() = 0;
  virtual void UnLoad() = 0;
};


class NativePlugin : public Plugin {

  string pluginName;
  string pluginAuthor;
  string pluginDescription;

  HMODULE plugin;

public:
  NativePlugin(const string& filename); 

  virtual const string& GetName() const { return pluginName; }
  virtual const string& GetAuthor() const { return pluginAuthor; }
  virtual const string& GetDescription() const { return pluginDescription; }

};













#endif //#define __INTERFACE_H_