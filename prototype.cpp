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


#include "prototype.h"
#include <algorithm>






bool Argument::Match(const SimpleArgument& arg, bool exact) const
{ 
  //arg.name must be empty or the same if we are optional
  return  ( arg.GetName().empty() || (optional && name == arg.GetName()) ) &&
    //if not exact the only automatic cast is int to float
    (exact ? type == arg.GetType() : type == FLOAT_T && arg.GetType() == INT_T);
}





void Prototype::AddArgument(AVSType type, const string& name)
{
  if ( ! argList.empty() && argList.back().IsOptional())
    throw InternalError("non optional argument cannot follow an optional one");
  try { Add(Argument(type, name)); }
  //if exception we change its type to internal
  catch(ScriptError ex) { throw InternalError(ex.err_msg); }
}

void Prototype::AddOptionalArgument(const AVSValue& defaultValue, const string& name)
{
  try { Add(Argument(defaultValue, name)); }
  //if exception we change its type to internal
  catch(ScriptError ex) { throw InternalError(ex.err_msg); }
}

bool Prototype::Match(const SimplePrototype& prototype, bool exact) const
{
  int s = prototype.size();
  if ( s > size() )
    return false;           //too much args: can't match

  int i = 0;
  //while matching...
  while( i < s && argList[i].Match(prototype[i], exact) ) { ++i; }
  if ( i == s) 
    return true;  //everything match: cool

  if ( ! argList[i].IsOptional() ) 
    return false;          //the next must be optional (and all the further next too then)

  //if here we have matched all the fixed args, now the optional
  int pos;
  //while not at end, and name found,  and matching...
  while ( i < s && ( pos = IndexOf(prototype[i]) ) != -1 && argList[pos].Match(prototype[i], exact) ) { ++i; }
  //if the while don't stopped on that, failed to match
  return i == s;

}


int Prototype::IndexOf(const SimpleArgument& arg) const
{
  vector<Argument>::const_iterator it = find(argList.begin(), argList.end(), arg);
  return it == argList.end() ? -1 : it - argList.begin();
}



  