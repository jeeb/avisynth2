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


#include "interface.h"
#include <algorithm>


void Prototype::AddArg(AVSType type) 
{
  if (! args.empty() && ! args.back().GetName().empty())
    throw ScriptError("named argument followed by an unnamed one");
  args.push_back(PrototypeArgument(type, ""));
}

void Prototype::AddNamedArg(AVSType type, const string& name)
{
  _ASSERTE(! name.empty() );  //if so the parser is going nuts

  if( find(args.begin(), args.end(), name) != args.end() )
    throw ScriptError("duplicate named argument in function call");
  args.push_back(PrototypeArgument(type, name));
}


bool Argument::Match(const PrototypeArgument& arg, bool exact) const
{ 
  //arg.name must be empty or the same if we are optional
  return  ( arg.GetName().empty() || (optional && name == arg.GetName()) ) &&
    //if not exact the only automatic cast is int to float
    (exact ? type == arg.GetType() : type == FLOAT_T && arg.GetType() == INT_T);
}



void ArgumentList::UnicityCheck(const string& name)
{
  if (find(argList.begin(), argList.end(), name) != argList.end() )
    throw InternalError(name + "is already used in this prototype");
}

void ArgumentList::AddArgument(AVSType type, const string& name)
{
  if ( ! argList.empty() && argList.back().IsOptional())
    throw InternalError("non optional argument cannot follow an optional one");
  UnicityCheck(name);
  argList.push_back(Argument(type, name));
}

void ArgumentList::AddOptionalArgument(const string& name, const AVSValue& defaut)
{
  UnicityCheck(name);
  argList.push_back(Argument(name, defaut));
}

bool ArgumentList::Match(const Prototype& prototype, bool exact) const
{
  int s = prototype.size();
  if ( s > argList.size() )
    return false;           //too much args: can't match

  int i = 0;
  while( i < s && argList[i].Match(prototype[i], exact) ) { ++i; }
  if ( i == s) 
    return true;  //everything match: cool

  if ( ! argList[i].IsOptional() ) 
    return false;          //the next must be optional (and all the further next too then)

  //if here we have matched all the fixed args, now the optional
  while ( i < s && GetArgByName(prototype[i].GetName()).Match(prototype[i], exact) ) { ++i; }
  //if the while don't stopped on that, failed to match
  return i == s;

}


const Argument& ArgumentList::GetArgByName(const string& name) const
{
  vector<Argument>::const_iterator it = find(argList.begin(), argList.end(), name);
  return it == argList.end() ? Argument::illegal : *it;
}