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


#ifndef __PROTOTYPE_H__
#define __PROTOTYPE_H__


#include <vector>
#include <string>
#include <algorithm>
using namespace std;

#include "internal.h"


/**********************************************************************************************/
/********************************* Arguments classes ******************************************/
/**********************************************************************************************/

//used by the parser when searching for avs functions
class SimpleArgument {

  static inline AVSType TypeCheck(AVSType type) {
    if (type == VOID_T)
      throw InternalError("void is not a valid argument type");
    return type;
  } 

  static inline const string& NameCheck(const string& name) {
    //test if a valid identifier  (the empty string is allowed here)
    //throw error if not
    return name;
  }

protected:
  AVSType type;
  string name;

public:  
  SimpleArgument(AVSType _type, const string& _name = "")
    : type(TypeCheck(_type)), name(NameCheck(_name)) { }  

  AVSType GetType() const { return type; }
  const string& GetName() const { return name; }

  //identity of name is sufficient for collision
  bool operator ==(const SimpleArgument& other) const { return name == other.name; }

};



//used by the parser to pass args to functions
class ValuedArgument : public SimpleArgument {

  static inline const AVSValue& DefinedCheck(const AVSValue& value) {
    if (! value.Defined() )
      throw InternalError("cannot take an undefined AVSValue as value");
    return value;
  }

  AVSValue value;

protected:
  //constructor for non optional instances of the subclass Argument
  ValuedArgument(AVSType type, const string& name = "") : SimpleArgument(type, name), value(type) { }
    
public:
  ValuedArgument(const AVSValue& _value, const string& name = "")
    : SimpleArgument(_value.GetType(), name), value(DefinedCheck(_value)) { }  

  const AVSValue& GetValue() const { return value; }

  operator AVSValue() const { return GetValue(); }

};



//used in avs functions to describe their prototype
class Argument : public ValuedArgument {

  static inline const string& NameCheck(const string& name) {
    if ( name.empty() )
      throw InternalError("the empty string is not a valid argument name");
    return name;
  }

  bool optional;

public:
  Argument(AVSType type, const string& name) : ValuedArgument(type, NameCheck(name)), optional(false) { }
  Argument(const AVSValue _default, const string& name)
    : ValuedArgument(_default, NameCheck(name)), optional(true) { }

  bool IsOptional() const { return optional; }
  const AVSValue& GetDefault() const { return GetValue(); }

  //test concordance
  bool Match(const SimpleArgument& arg, bool exact = false) const;

};


/***********************************************************************************************/
/*********************************** Prototype classes *****************************************/
/***********************************************************************************************/

template <class Arg> class templatePrototype {

protected:
  typedef vector<Arg> ArgList;
  
  ArgList argList;

  templatePrototype() { }
  templatePrototype(const templatePrototype<Arg>& other) : argList(other.argList) { }

  //method to add an arg (and test name conditions)
  void Add(const Arg& arg) {
    //only the empty string is allowed as duplicate name
    if (! arg.GetName().empty() && find(argList.begin(), argList.end(), arg) != argList.end() )
      throw ScriptError(arg.GetName() + "  is already used in this prototype");
    //if arg name == "" and precedent arg is named
    if ( arg.GetName().empty() && ! argList.empty() && ! argList.back().GetName().empty() )
      throw ScriptError("cannot have an named arg followed by an unnamed one");    
    argList.push_back(arg);
  }

public:
  int size() const { return argList.size(); }
  const Arg& operator [ ] (int i) const { return argList[i]; }


};



//used by the parser when searching for functions
class SimplePrototype : public templatePrototype<SimpleArgument> {

public:
  SimplePrototype() { }

  void AddArg(AVSType type) { Add(SimpleArgument(type)); }
  void AddNamedArg(AVSType type, const string& name) { Add(SimpleArgument(type, name)); }

};


//used by the parser to pass args to functions
class NamedArgVector : public templatePrototype<ValuedArgument> {
  
public:
  NamedArgVector() { }

  void AddArg(const AVSValue& value) { Add(ValuedArgument(value)); }
  void AddNamedArg(const AVSValue& value, const string& name) { Add(ValuedArgument(value, name)); }
  
};


//used by avs function as prototype description
class Prototype : public templatePrototype<Argument> {

public:
  Prototype() { }
  Prototype(const Prototype& other) : templatePrototype<Argument>(other) { }

  void AddArgument(AVSType type, const string& name);
  void AddOptionalArgument(const AVSValue& defaultValue, const string& name);

  //test of concordance
  bool Match(const SimplePrototype& prototype, bool exact) const;
  //returns -1 for not found
  int IndexOf(const SimpleArgument& arg) const;

};







#endif //#define __PROTOTYPE_H__