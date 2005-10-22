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

// ConvertPlanar (c) 2005 by Klaus Post

#ifndef __Convert_MATRIX_H__
#define __Convert_MATRIX_H__

#include "../internal.h"
#include "../core/softwire_helpers.h"


class MatrixGenerator3x3 : public  CodeGenerator
{
public:
  MatrixGenerator3x3();
  ~MatrixGenerator3x3();
protected:
  void GenerateAssembly(int width, int faction_bits, bool upper32_ones, IScriptEnvironment* env);
  void GeneratePacker(int width, IScriptEnvironment* env);
  void GenerateUnPacker(int width, IScriptEnvironment* env);
  DynamicAssembledCode assembly;
  DynamicAssembledCode unpacker;
  DynamicAssembledCode packer;
  BYTE* dyn_src;
  BYTE* dyn_dest;
  BYTE* dyn_matrix;
  __int64 pre_add, post_add;
  __int64 rounder;
  int src_pixel_step;
  int dest_pixel_step;
  const BYTE** unpck_src;
  BYTE** unpck_dst;
private:
  int last_pix;
  __int64* aligned_rounder;
};



#endif
