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


//I am not too happy with this name, so it is bound to change
#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <vector>
#include <string>
using namespace std;

#include "avisynth.h"


class PrototypeArgument {

  inline static AVSType TypeCheck(AVSType type) {
    if (type == VOID_T)
      throw InternalError("void is not a valid argument type");
    return type;
  }

protected:
  //constructor for a special illegal value of the Argument subclass
  PrototypeArgument() : type(VOID_T), name("illegal") { }

  AVSType type;
  string name;

public:
  PrototypeArgument(AVSType _type, const string& _name)
    : type(TypeCheck(_type)), name(_name) { }  //name check is not done at this level

  AVSType GetType() const { return type; }
  const string& GetName() const { return name; }

  //identity of name is sufficient for collision
  bool operator ==(const string& _name) const { return name == _name; }
};



class Prototype {

  typedef vector<PrototypeArgument> ArgVector;

  ArgVector args;

public:
  Prototype() { }

  void AddArg(AVSType type);
  void AddNamedArg(AVSType type, const string& name);

  int size() const { return args.size(); }

  const PrototypeArgument& operator [ ](int i) const { return args[i]; }
};


class Argument : public PrototypeArgument {

  inline static const string& NameCheck(const string& name) {
    if ( name.empty() )
      throw InternalError("the empty string is not a valid argument name");
    //test if it's acceptable as an identifier...
    return name;
  }

  inline static const AVSValue& DefaultCheck(const AVSValue& defaut) {
    if (! defaut.Defined() )
      throw InternalError("an undefined AVSValue is not a valid argument default");
    return defaut;
  }

  //constructor for a special illegal value assured to never match anything
  Argument() : PrototypeArgument(), optional(true) { }

  bool optional;
  AVSValue defaultValue;

public:
  Argument(AVSType type, const string& name)
    : PrototypeArgument(type, NameCheck(name)), optional(false) { }
  Argument(const string& _name, const AVSValue _default)
    : PrototypeArgument(_default.GetType(), NameCheck(name)), defaultValue(DefaultCheck(_default)), optional(true) { }

  bool IsOptional() const { return optional; }
  const AVSValue& GetDefault() const { return defaultValue; }


  bool Match(const PrototypeArgument& arg, bool exact) const;


  //that's the special illegal value, used as a not found value by GetArgByName in ArgumentList
  static const Argument illegal; // = Argument();  
};


class ArgumentList {

  vector<Argument> argList;

  //used to check that you don't duplicate arg names
  void UnicityCheck(const string& name);

public:
  ArgumentList() { }

  void AddArgument(AVSType type, const string& name);    
  void AddOptionalArgument(const string& name, const AVSValue& defaut);


  bool Match(const Prototype& prototype, bool exact) const;

  int size() const { return argList.size(); }
  const Argument& operator [ ](int i) const { return argList[i]; }

  const Argument& GetArgByName(const string& name) const;
};

typedef pair<string, AVSValue> NamedArg;
typedef vector<NamedArg> NamedArgVector;










#endif //#define __INTERFACE_H_