// Avisynth v2.6.  Copyright 2005 Ben Rudiak-Gould et al.
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


#ifndef __CLIPLOCALSTORAGE_H__
#define __CLIPLOCALSTORAGE_H__


//For ClipLocalStorage


class IClipLocalStorage {
protected:
  friend class PClipLocalStorage;
  long refcnt;
  virtual void __stdcall AddRef() { InterlockedIncrement(&refcnt); }
  virtual void __stdcall Release() {
	  if (!InterlockedDecrement(&refcnt))
		  delete this; }
  IClipLocalStorage():refcnt(1){}
public:
	virtual __stdcall ~IClipLocalStorage(){};
	virtual void* __stdcall GetValue()=0;
	virtual void __stdcall SetValue(void* value)=0;
};

class PClipLocalStorage {
protected:
  void Set(IClipLocalStorage* x) {
    if (x) x->AddRef();
    if (p) p->Release();
    p=x;
  }
	IClipLocalStorage* p;
public:
	PClipLocalStorage(IScriptEnvironment* env):p(env->AllocClipLocalStorage()){}
	PClipLocalStorage():p(0){}
	void operator=(IClipLocalStorage* x) { Set(x); }
    void operator=(const PClipLocalStorage& x) { Set(x.p); }

    IClipLocalStorage* operator->() const { return p; }

  // for conditional expressions
  operator void*() const { return p; }
  bool operator!() const { return !p; }

  ~PClipLocalStorage() { 
	  if (p) p->Release();}
};

#endif //__CLIPLOCALSTORAGE_H__
