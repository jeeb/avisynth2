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


#ifndef __AVSFUNCTION_H__
#define __AVSFUNCTION_H__

#include "interface.h" 
#include "windows.h


class Plugin;

//base class for all function in avisynth
//each function accessible in scripts has its own instance
//(except some special functions who are done by the parser)
class AVSFunction {

  const AVSType returnType;
  const string name;

public:
  AVSFunction(AVSType retType, const string& _name) : returnType(retType), name(_name) { }

  AVSType GetReturnType() const { return returnType; }
  const string& GetName() const { return name; }
  virtual const ArgumentList& GetArgumentList() const = 0;

  //return the plugin this funciton depends
  //(for internals, there is a special plugin representing the core)
  virtual const Plugin * GetMotherPlugin() const = 0; 
  virtual Plugin * GetMotherPlugin() = 0;  // ??
  
  //small useful test
  bool IsFilter() const { return returnType == CLIP_T; }

  //useful for determining priority in overload resolution
  virtual bool IsUserFunction() const { return false; }
  virtual bool IsCoreFunction() const { return false; }

  //method used to list accepted colorspaces
  virtual bool AcceptColorSpace(VideoInfo::ColorSpace) const = 0;

  //does all type checking, reorder, replace by defaults
  //and throw errors if args doesn't fit in
  AVSValue operator() (const NamedArgVector& args);

protected:
  //called by the above, quaranteed to have args according to specified prototype
  virtual AVSValue Process(const ArgVector& args) = 0;

};


//base class for plugin
class Plugin {

public:
  virtual const string& GetName() const = 0;
  virtual const string& GetAuthor() const = 0;
  virtual const string& GetDescription() const = 0;

  virtual bool IsLoaded() const = 0;

  virtual void Load() = 0;
  virtual void UnLoad() = 0;
};




/*******************************************************************************************/
/***************************** CorePlugin and CoreFunction *********************************/
/*******************************************************************************************/

class InternalFunction;

class CorePlugin : public Plugin {

  //private contructor 
  CorePlugin() { }

  //returns the sole instance of the core
  static CorePlugin * GetCore();
    
  //method called by InternalFunction to register themselves as a part of the core
  void RegisterFunction(InternalFunction * funct);

  friend InternalFunction;

public:
  virtual const string& GetName() const;
  virtual const string& GetAuthor() const;
  virtual const string& GetDescription() const;

  //the core is always loaded
  virtual bool IsLoaded() const { return true; }
  //so these have no effect
  virtual void Load() { }
  virtual void UnLoad() { }

};

//common superclass for user-defined function and core function
class InternalFunction : public AVSFunction {  
  
public:
  //construct and register itself at the core
  InternalFunction(AVSType retType, const string& _name)
    : AVSFunction(retType, _name) { CorePlugin::GetCore()->RegisterFunction(this); }

  virtual const Plugin * GetMotherPlugin() const { return CorePlugin::GetCore(); }
  virtual Plugin * GetMotherPlugin() { return CorePlugin::GetCore(); }
 
};

//function defined by user
//how they work has yet to be determined
class UserFunction : public InternalFunction {

public:
  UserFunction(AVSType retType, const string& _name) : InternalFunction(retType, _name) { }
    
  virtual bool IsUserFunction() const { return true; }

  //user defined function have not the possibility to select colorspace
  //they accept everything (and eventually complain after)
  virtual bool AcceptColorSpace(VideoInfo::ColorSpace) const { return true; }

};

//function of the core
//for each function of the core, there should be one instance of it
class CoreFunction : public InternalFunction {

public:
  CoreFunction(AVSType retType, const string& _name) : InternalFunction(retType, _name) { }

  virtual bool IsCoreFunction() const { return true; }

};

//convenience subclass of the above (economize writing CLIP_T all the time)
class CoreFilter : public CoreFunction {

public:
  CoreFilter(const string& _name) : CoreFunction(CLIP_T, _name) { }

};



/*********************************************************************************************/
/************************** NativePlugin and PluginFunction **********************************/
/*********************************************************************************************/

class PluginFunction;

class NativePlugin : public Plugin {

  string pluginName;
  string pluginAuthor;
  string pluginDescription;

  HMODULE plugin;
  bool loaded;

  void RegisterFunction(PluginFunction * funct);

public:
  //do all the work, link to get filled with plugin function and ....
  NativePlugin(const string& filename); 

  virtual const string& GetName() const { return pluginName; }
  virtual const string& GetAuthor() const { return pluginAuthor; }
  virtual const string& GetDescription() const { return pluginDescription; }

  virtual bool IsLoaded() const { return loaded; }

  virtual void Load();
  virtual void UnLoad();
};


typedef AVSValue (* ProcessFunction)(const ArgVector& args);

class PluginFunction : public AVSFunction {

  NativePlugin * mother;

  ProcessFunction pf;

  //method should be called by mother plugin on all its filters when load is done
  void SetProcessFunction(ProcessFunction _pf) { pf = _pf; }
  friend NativePlugin;

public:
  //register the self as a function of mother
  PluginFunction(NativePlugin * _mother, AVSType retType, const string& _name, const ArgumentList& _argList);

  virtual const Plugin * GetMotherPlugin() const { return mother; }
  virtual Plugin * GetMotherPlugin() { return mother; }

protected:
  virtual AVSValue Process(const ArgVector& args) const { if (! mother->IsLoaded()) mother->Load(); return pf(args); }
   
};

//exist just for convenience of not repeating CLIP_T all the time
class PluginFilter : public PluginFunction {

public:
  PluginFilter(NativePlugin * _mother, const string& _name, const ArgumentList& _argList)
    : PluginFunction(_mother, CLIP_T, _name, _argList) { }

};














#endif // #define __AVSFUNCTION_H__