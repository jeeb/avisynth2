// Avisynth v1.0 beta.  Copyright 2000 Ben Rudiak-Gould.
// http://www.math.berkeley.edu/~benrg/avisynth.html

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


#include "audio.h"


/********************************************************************
***** Declare index of new filters for Avisynth's filter engine *****
********************************************************************/

AVSFunction Audio_filters[] = {
  { "DelayAudio", "cf", DelayAudio::Create },
  { "AmplifydB", "cf[]f", Amplify::Create_dB },
  { "Amplify", "cf[]f", Amplify::Create },
  { "Normalize", "c[left]f", Normalize::Create },
  { "MixAudio", "cc[clip1_factor]f[clip2_factor]f", MixAudio::Create },
  { "ResampleAudio", "ci", ResampleAudio::Create },
  { "LowPassAudio", "ci[]f", FilterAudio::Create_LowPass },
//  { "LowPassAudioALT", "ci", FilterAudio::Create_LowPassALT },
  { "HighPassAudio", "ci[]f", FilterAudio::Create_HighPass },
  { "ConvertAudioTo16bit", "c", ConvertAudio::Create_16bit },
  { "ConvertAudioTo8bit", "c", ConvertAudio::Create_8bit },
  { "ConvertAudioTo32bit", "c", ConvertAudio::Create_32bit },
  { "ConvertAudioToFloat", "c", ConvertAudio::Create_float },
  { "ConvertToMono", "c", ConvertToMono::Create },
  { "EnsureVBRMP3Sync", "c", EnsureVBRMP3Sync::Create },
  { "MergeChannels", "cc", MergeChannels::Create },
  { "GetLeftChannel", "c", GetChannel::Create_left },
  { "GetRightChannel", "c", GetChannel::Create_right },
  { "GetChannel", "ci", GetChannel::Create_n },
  { "KillAudio", "c", KillAudio::Create },
  { 0 }
};

// Note - floats should not be clipped - they will be clipped, when they are converted back to ints.
// Vdub can handle 8/16 bit, and reads 32bit, but cannot play/convert it. Floats doesn't make sense in AVI. So for now convert back to 16 bit always
// FIXME: Most int64's are often cropped to ints - count is ok to be int, but not start
 
 


AVSValue __cdecl ConvertAudio::Create_16bit(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create(args[0].AsClip(),SAMPLE_INT16,SAMPLE_INT16);
}

AVSValue __cdecl ConvertAudio::Create_8bit(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create(args[0].AsClip(),SAMPLE_INT8,SAMPLE_INT8);
}


AVSValue __cdecl ConvertAudio::Create_32bit(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create(args[0].AsClip(),SAMPLE_INT32,SAMPLE_INT32);
}

AVSValue __cdecl ConvertAudio::Create_float(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create(args[0].AsClip(),SAMPLE_FLOAT,SAMPLE_FLOAT);
}

/******************************************
 *******   Convert Audio -> Mono     ******
 *******   Supports int16 & float    ******
 *****************************************/

ConvertToMono::ConvertToMono(PClip _clip) 
  : GenericVideoFilter(ConvertAudio::Create(_clip,
  SAMPLE_INT16|SAMPLE_FLOAT, SAMPLE_FLOAT))
{
	
  vi.nchannels = 1;
  tempbuffer_size=0;
}


void __stdcall ConvertToMono::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) 
{
  int channels=vi.AudioChannels();
  if (tempbuffer_size) {
    if (tempbuffer_size<count) {
      delete[] tempbuffer;
      tempbuffer=new char[count*channels*vi.BytesPerChannelSample()];
      tempbuffer_size=count;
    }
  } else {
    tempbuffer=new char[count*channels*vi.BytesPerChannelSample()];
    tempbuffer_size=count;
  }
  child->GetAudio(tempbuffer, start, count, env);
  if (vi.SampleType()&SAMPLE_INT16) {
    signed short* samples = (signed short*)buf;
    signed short* tempsamples = (signed short*)tempbuffer;
    for (int i=0; i<count; i++) {
      int tsample=0;    
      for (int j=0;j<channels;j++) 
        tsample+=tempbuffer[i*channels+j]; // Accumulate samples
      samples[i] =(short)((tsample+(channels>>1))/channels);
    }
  } else if (vi.SampleType()&SAMPLE_FLOAT) {
    SFLOAT* samples = (SFLOAT*)buf;
    SFLOAT* tempsamples = (SFLOAT*)tempbuffer;
    SFLOAT f_channels= (SFLOAT)channels;
    for (int i=0; i<count; i++) {
      SFLOAT tsample=0.0f;    
      for (int j=0;j<channels;j++) 
        tsample+=tempsamples[i*channels+j]; // Accumulate samples
      samples[i] =(tsample/f_channels);
    }
  }
}


PClip ConvertToMono::Create(PClip clip) 
{
  if (clip->GetVideoInfo().AudioChannels()==1)
    return clip;
  else
    return new ConvertToMono(clip);
}

AVSValue __cdecl ConvertToMono::Create(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create(args[0].AsClip());
}

/******************************************
 *******   Ensure VBR mp3 sync,      ******
 *******    by always reading audio  ******
 *******    sequencial.              ******             
 *****************************************/

EnsureVBRMP3Sync::EnsureVBRMP3Sync(PClip _clip) 
  : GenericVideoFilter(_clip)
{	
  last_end=0;
}


void __stdcall EnsureVBRMP3Sync::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
  signed short* samples = (signed short*)buf;

  if (start!=last_end) { // Reread!
    int offset=0;
    if (start>last_end) offset=last_end; // Skip forward only if the skipped to position is in front of last position.
    while (offset+count<start) { // Read whole blocks of 'count' samples
      child->GetAudio(samples, offset, count, env);
      offset+=count;
    } // Read until 'start'
      child->GetAudio(samples, offset, start-offset, env);  // Now we're at 'start'
      offset+=start-offset;
      if (offset!=start)
        env->ThrowError("EnsureVBRMP3Sync [Internal error]: Offset should be %i, but is %i",start,offset);
  }
  child->GetAudio(samples, start, count, env);
  last_end=start+count;
}


PClip EnsureVBRMP3Sync::Create(PClip clip) 
{
    return new EnsureVBRMP3Sync(clip);
}

AVSValue __cdecl EnsureVBRMP3Sync::Create(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create(args[0].AsClip());
}


/*******************************************
 *******   Mux two sources, so the      ****
 *******   total channels  is the same  ****
 *******   as the two clip              ****
 *******************************************/

MergeChannels::MergeChannels(PClip _child, PClip _clip, IScriptEnvironment* env) 
  : GenericVideoFilter(_child),
	tclip(_clip)
{

  clip2 = ConvertAudio::Create(tclip,vi.SampleType(),vi.SampleType());  // Clip 2 should now be same type as clip 1.
  vi2 = clip2->GetVideoInfo();

	if (vi.audio_samples_per_second!=vi2.audio_samples_per_second) 
		env->ThrowError("MergeChannels: Clips must have same sample rate! Use ResampleAudio()!");  // Could be removed for fun :)

	if (vi.SampleType()!=vi2.SampleType()) 
		env->ThrowError("MergeChannels: Clips must have same sample type! Use ConvertAudio()!");  

  clip1_channels=vi.AudioChannels();
	clip2_channels=vi2.AudioChannels();

  vi.nchannels = clip1_channels+clip2_channels;
  tempbuffer_size=0;
}


void __stdcall MergeChannels::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) 
{
  if (tempbuffer_size) {
    if (tempbuffer_size<count) {
      delete[] tempbuffer;
      tempbuffer=new signed char[count*vi.BytesPerChannelSample()*clip1_channels];
      tempbuffer2=new signed char[count*vi2.BytesPerAudioSample()];
      tempbuffer_size=count;
    }
  } else {
      tempbuffer=new signed char[count*vi.BytesPerChannelSample()*clip1_channels];
      tempbuffer2=new signed char[count*vi2.BytesPerAudioSample()];
      tempbuffer_size=count;
  }


  child->GetAudio(tempbuffer, start, count, env);
  clip2->GetAudio(tempbuffer2, start, count, env);
  char *samples=(char*) buf;
  int cs=0;
  int ct1=0;
  int ct2=0;

  for (int i=0;i<count;i++) {
    for (int j=0;j<clip1_channels;j++) 
      for (int k=0;k<vi.BytesPerChannelSample();k++) 
        samples[cs++] = tempbuffer[ct1++];
    for (j=0;j<clip1_channels;j++) 
      for (int k=0;k<vi2.BytesPerChannelSample();k++) 
        samples[cs++] = tempbuffer2[ct2++];
  }
    
}


AVSValue __cdecl MergeChannels::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  return new MergeChannels(args[0].AsClip(),args[1].AsClip(),env);
}


/***************************************************
 *******   Get left or right                 *******
 *******    channel from a stereo source     *******
 ***************************************************/



GetChannel::GetChannel(PClip _clip, int _channel) 
  : GenericVideoFilter(_clip),
		channel(_channel)
{	
  src_bps=vi.BytesPerAudioSample();
  src_cbps=vi.BytesPerChannelSample();
  vi.nchannels = 1;
  tempbuffer_size=0;
}


void __stdcall GetChannel::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) 
{
  if (tempbuffer_size) {
    if (tempbuffer_size<count) {
      delete[] tempbuffer;
      tempbuffer=new char[count*src_bps];
      tempbuffer_size=count;
    }
  } else {
    tempbuffer=new char[count*src_bps];
    tempbuffer_size=count;
  }
  child->GetAudio(tempbuffer, start, count, env);
  char* samples = (char*)buf;
  for (int i=0; i<count; i++) 
    for (int j=0;i<src_cbps;j++)
		  samples[i+j] = tempbuffer[i*src_bps+(channel*src_cbps)+j];
}


PClip GetChannel::Create_left(PClip clip) 
{
  if (clip->GetVideoInfo().AudioChannels()==1)
    return clip;
  else
    return new GetChannel(clip,0);
}

PClip GetChannel::Create_right(PClip clip) 
{
  if (clip->GetVideoInfo().AudioChannels()==1)
    return clip;
  else
    return new GetChannel(clip,1);
}

PClip GetChannel::Create_n(PClip clip, int n) 
{
  if (clip->GetVideoInfo().AudioChannels()==1)
    return clip;
  else
    if (clip->GetVideoInfo().AudioChannels()<n) return clip;   // Should it fail instead?
    return new GetChannel(clip,n);
}

AVSValue __cdecl GetChannel::Create_left(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create_left(args[0].AsClip());
}

AVSValue __cdecl GetChannel::Create_right(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create_right(args[0].AsClip());
}

AVSValue __cdecl GetChannel::Create_n(AVSValue args, void*, IScriptEnvironment*) 
{
  return Create_n(args[0].AsClip(), args[1].AsInt(0));
}

/******************************
 *******   Kill Audio  ********
 ******************************/

KillAudio::KillAudio(PClip _clip) 
  : GenericVideoFilter(_clip)
{
  vi.audio_samples_per_second=0;
  vi.num_audio_samples=0;
}

AVSValue __cdecl KillAudio::Create(AVSValue args, void*, IScriptEnvironment*) 
{
  return new KillAudio(args[0].AsClip());
}





/******************************
 *******   Delay Audio   ******
 *****************************/

DelayAudio::DelayAudio(double delay, PClip _child)
 : GenericVideoFilter(_child), delay_samples(__int64(delay * vi.audio_samples_per_second + 0.5))
{
  vi.num_audio_samples += delay_samples;
}


void DelayAudio::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
  child->GetAudio(buf, start-delay_samples, count, env);
}


AVSValue __cdecl DelayAudio::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  return new DelayAudio(args[1].AsFloat(), args[0].AsClip());
}





/********************************
 *******   Amplify Audio   ******
 *******************************/
// FIXME: Support more volumes (for additional channels) - now only left channel is used! Use an array!

Amplify::Amplify(PClip _child, double _left_factor, double _right_factor)
  : GenericVideoFilter(ConvertAudio::Create(_child,SAMPLE_INT16|SAMPLE_FLOAT|SAMPLE_INT32,SAMPLE_FLOAT)),
    left_factor(_left_factor),
    right_factor(_right_factor) {
}


void __stdcall Amplify::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
  child->GetAudio(buf, start, count, env);
  int channels=vi.AudioChannels();
  if (vi.SampleType()==SAMPLE_INT16) {
    short* samples = (short*)buf;
    int lf_16=int(left_factor*65535.0f+0.5f);
    int rf_16=int(right_factor*65535.0f+0.5f);
    for (int i=0; i<count; ++i) {
      for (int j=0;j<channels;j++) {
        samples[i*channels+j] = Saturate(int(Int32x32To64(samples[i*channels+j],lf_16) >> 16));
      }
    } 
    return;
  }

  if (vi.SampleType()==SAMPLE_INT32) {
    int* samples = (int*)buf;
    int lf_16=int(left_factor*65535.0f+0.5f);
    int rf_16=int(left_factor*65535.0f+0.5f);
    for (int i=0; i<count; ++i) {
      for (int j=0;j<channels;j++) {
        samples[i*channels+j] = Saturate_int32(Int32x32To64(samples[i*channels+j],lf_16) >> 16);
      }
    } 
    return;
  }
  if (vi.SampleType()==SAMPLE_FLOAT) {
    SFLOAT* samples = (SFLOAT*)buf;
    for (int i=0; i<count; ++i) {
      for (int j=0;j<channels;j++) {
        samples[i*channels+j] = samples[i*channels+j]*left_factor;   // Does not saturate, as other filters do. We should saturate only on conversion.
      }
    } 
    return;
  }
}


AVSValue __cdecl Amplify::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  double left_factor = args[1].AsFloat();
  double right_factor = args[2].AsFloat(left_factor);
  return new Amplify(args[0].AsClip(), left_factor, right_factor);
}

  

AVSValue __cdecl Amplify::Create_dB(AVSValue args, void*, IScriptEnvironment* env) 
{
    double left_factor = args[1].AsFloat();
    double right_factor = args[2].AsFloat(left_factor);
    return new Amplify(args[0].AsClip(), dBtoScaleFactor(left_factor), dBtoScaleFactor(right_factor));
}


/*****************************
 ***** Normalize audio  ******
 ***** Supports int16,float******
 ******************************/
// Fixme: Maxfactor should be different on different types

Normalize::Normalize(PClip _child, double _max_factor)
  : GenericVideoFilter(ConvertAudio::Create(_child,SAMPLE_INT16|SAMPLE_FLOAT,SAMPLE_FLOAT)),
    max_factor(_max_factor)
{
  max_volume=-1.0f;
}


void __stdcall Normalize::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
  if (max_volume<0.0f) {
    int passes=vi.num_audio_samples/count;
    int num_samples=count;
    // Read samples into buffer and test them
    if (vi.SampleType()==SAMPLE_INT16) {
      short* samples = (short*)buf;
      short i_max_volume=0;
      for (int i=0;i<passes;i++) {
          child->GetAudio(buf, num_samples*i, count, env);
          for (int i=0;i<num_samples;i++) {
            i_max_volume=max(abs(samples[i]),i_max_volume);
          }      
      }    
      // Remaining samples
      int rem_samples=vi.num_audio_samples%count;
      child->GetAudio(buf, num_samples*passes, rem_samples, env);
      for (i=0;i<rem_samples;i++) {
        i_max_volume=max(abs(samples[i]),i_max_volume);
      }
      max_volume=(float)i_max_volume/32768.0f;
      max_factor=1.0f/max_volume;

    } else if (vi.SampleType()==SAMPLE_FLOAT) {  // Float 

      SFLOAT* samples = (SFLOAT*)buf;
      for (int i=0;i<passes;i++) {
          child->GetAudio(buf, num_samples*i, count, env);
          for (int i=0;i<num_samples;i++) {
            max_volume=max(fabs(samples[i]),max_volume);
          }      
      }    
      // Remaining samples
      int rem_samples=vi.num_audio_samples%count;
      rem_samples*=vi.AudioChannels();
      child->GetAudio(buf, num_samples*passes, rem_samples, env);
      for (i=0;i<rem_samples;i++) {
        max_volume=max(fabs(samples[i]),max_volume);
      }

      max_factor=1.0f/max_volume;
    }
  } 
  if (vi.SampleType()==SAMPLE_INT16) {
    int factor=(int)(max_factor*65536.0f);
    short* samples = (short*)buf;
    child->GetAudio(buf, start, count, env); 
    int channels=vi.AudioChannels();
    for (int i=0; i<count; ++i) {
      for (int j=0;j<channels;j++) {
        samples[i*channels+j] = Saturate(int(Int32x32To64(samples[i*channels+j],factor) >> 16));
      }
    } 
  } else {
    SFLOAT* samples = (SFLOAT*)buf;
    child->GetAudio(buf, start, count, env); 
    int channels=vi.AudioChannels();
    for (int i=0; i<count; ++i) {
      for (int j=0;j<channels;j++) {
        samples[i*channels+j] = samples[i*channels+j]*max_factor;
      }
    } 
  }
}


AVSValue __cdecl Normalize::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  double max_volume=args[1].AsFloat(1.0);
  return new Normalize(args[0].AsClip(), max_volume);
}


/*****************************
 ***** Mix audio  tracks ******
 ******************************/

MixAudio::MixAudio(PClip _child, PClip _clip, double _track1_factor, double _track2_factor, IScriptEnvironment* env)
  : GenericVideoFilter(ConvertAudio::Create(_child,SAMPLE_INT16|SAMPLE_FLOAT,SAMPLE_FLOAT)),
    tclip(_clip),
		track1_factor(int(_track1_factor*65536+.5)),
    track2_factor(int(_track2_factor*65536+.5)) {

		const VideoInfo& vi2 = clip->GetVideoInfo();
    clip = ConvertAudio::Create(tclip,vi.SampleType(),vi.SampleType());  // Clip 2 should now be same type as clip 1.

		if (vi.audio_samples_per_second!=vi2.audio_samples_per_second) 
			env->ThrowError("MixAudio: Clips must have same sample rate! Use ResampleAudio()!");  // Could be removed for fun :)

		if (vi.AudioChannels()!=vi2.AudioChannels()) 
			env->ThrowError("MixAudio: Clips must have same number of channels! Use ConvertToMono() or MergeChannels()!");

		tempbuffer_size=0;
	}


void __stdcall MixAudio::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
  if (tempbuffer_size) {
    if (tempbuffer_size<count) {
      delete[] tempbuffer;
      tempbuffer=new signed char[count*vi.BytesPerAudioSample()];
      tempbuffer_size=count;
    }
  } else {
    tempbuffer=new signed char[count*vi.BytesPerAudioSample()];
    tempbuffer_size=count;
  }

  child->GetAudio(buf, start, count, env);
  clip->GetAudio(tempbuffer, start, count, env);
  int channels=vi.AudioChannels();

  if (vi.SampleType()&SAMPLE_INT16) {
    short* samples = (short*)buf;
    short* clip_samples = (short*)tempbuffer;
    for (int i=0; i<count; ++i) {
      for (int j=0;j<channels;j++) {
        samples[i*channels+j] = Saturate( int(Int32x32To64(samples[i*channels+j],track1_factor) >> 16) +
          int(Int32x32To64(clip_samples[i*channels+j],track2_factor) >> 16) );
      }
    }
  } else if (vi.SampleType()&SAMPLE_FLOAT) {
    SFLOAT* samples = (SFLOAT*)buf;
    SFLOAT* clip_samples = (SFLOAT*)tempbuffer;
    float t1factor=(float)track1_factor/65536.0f;
    float t2factor=(float)track2_factor/65536.0f;
    for (int i=0; i<count; ++i) {
      for (int j=0;j<channels;j++) {
        samples[i*channels+j]=(samples[i*channels+j]*t1factor) + (clip_samples[i*channels+j]*t2factor);
      }
    }
  }
}



AVSValue __cdecl MixAudio::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  double track1_factor = args[2].AsFloat(0.5);
  double track2_factor = args[3].AsFloat(1.0-track1_factor);
  return new MixAudio(args[0].AsClip(),args[1].AsClip(), track1_factor, track2_factor,env);
}

  
/**********************************************
 *******   Lowpass and highpass filter  *******
 *******                                *******
 *******   Type : biquad,               *******
 *******          tweaked butterworth   *******
 *******  Source: musicdsp.org          *******
 *******          Posted by Patrice Tarrabia **
 *******  Adapted by Klaus Post         *******
 ***********************************************/

// FIXME: Support float
FilterAudio::FilterAudio(PClip _child, int _cutoff, float _rez, int _lowpass)
  : GenericVideoFilter(ConvertAudio::Create(_child,SAMPLE_INT16|SAMPLE_FLOAT,SAMPLE_FLOAT)),
    cutoff(_cutoff),
    rez(_rez), 
    lowpass(_lowpass) {
      l_vibrapos = 0;
      l_vibraspeed = 0;
      r_vibrapos = 0;
      r_vibraspeed = 0;
      lastsample=-1;
      tempbuffer_size=0;
} 


void __stdcall FilterAudio::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) 
{
  if (lowpass<2) {    
    //Algorithm 1: Lowpass is only used in ALT-mode
    
    if (tempbuffer_size) {
      if (tempbuffer_size<(count*2)) {
        delete[] tempbuffer;
        tempbuffer=new signed short[4+count*2];
        tempbuffer_size=count;
      }
    } else {
      tempbuffer=new signed short[4+count*2];
      tempbuffer_size=count;
    }
    child->GetAudio(tempbuffer, start-2, count+2, env);
    float a1,a2,a3,b1,b2,c; 
    if (lowpass) {
      c = 1.0 / tan(PI * (float)cutoff / (float)vi.audio_samples_per_second);
      a1 = 1.0 / ( 1.0 + rez * c + c * c);
      a2 = 2* a1;
      a3 = a1;
      b1 = 2.0 * ( 1.0 - c*c) * a1;
      b2 = ( 1.0 - rez * c + c * c) * a1; 
    } else {
      c = tan(PI * (float)cutoff / (float)vi.audio_samples_per_second);
      a1 = 1.0 / ( 1.0 + rez * c + c * c);
      a2 = -2*a1;
      a3 = a1;
      b1 = 2.0 * ( c*c - 1.0) * a1;
      b2 = ( 1.0 - rez * c + c * c) * a1;  
    }
    short* samples = (short*)buf;
    if (vi.AudioChannels()==2) {
      
      if (lastsample!=start) {  // Streaming has just started here.
        last_1=tempbuffer[3];
        last_2=tempbuffer[2];
        last_3=tempbuffer[1];
        last_4=tempbuffer[0]; 
      }
      unsigned int i=0;
      tempbuffer+=4;
      samples[0]=Saturate((int)(a1 * (float)tempbuffer[i*2] + a2 * (float)tempbuffer[i*2-2] + a3 * (float)tempbuffer[i*2-4] - b1*(float)last_2 - b2*(float)last_4));
      samples[1]=Saturate((int)(a1 * (float)tempbuffer[i*2+1] + a2 * (float)tempbuffer[i*2-2+1] + a3 * (float)tempbuffer[i*2-4+1] - b1*(float)last_1 - b2*(float)last_3+0.5f));
      i++;
      samples[2]=Saturate((int)(a1 * (float)tempbuffer[i*2] + a2 * (float)tempbuffer[i*2-2] + a3 * (float)tempbuffer[i*2-4] - b1*(float)samples[i*2-2] - b2*(float)last_2+0.5f));
      samples[3]=Saturate((int)(a1 * (float)tempbuffer[i*2+1] + a2 * (float)tempbuffer[i*2-2+1] + a3 * (float)tempbuffer[i*2-4+1] - b1*(float)samples[i*2-2+1] - b2*(float)last_1+0.5f));
      i++;
      for (; i<count; ++i) { 
        samples[i*2]=Saturate((int)(a1 * (float)tempbuffer[i*2] + a2 * (float)tempbuffer[i*2-2] + a3 * (float)tempbuffer[i*2-4] - b1*(float)samples[i*2-2] - b2*(float)samples[i*2-4]+0.5f));
        samples[i*2+1]=Saturate((int)(a1 * (float)tempbuffer[i*2+1] + a2 * (float)tempbuffer[i*2-2+1] + a3 * (float)tempbuffer[i*2-4+1] - b1*(float)samples[i*2-2+1] - b2*(float)samples[i*2-4+1]+0.5f));
      }
      last_1=samples[count*2-1];
      last_2=samples[count*2-2];
      last_3=samples[count*2-3];
      last_4=samples[count*2-4];
      lastsample=start+count;
    } 
    else if (vi.AudioChannels()==1) { 
      if (lastsample!=start) {
        last_1=tempbuffer[1];
        last_2=tempbuffer[0];
      }
      tempbuffer+=2;
      unsigned int i=0;
      samples[i]=Saturate((int)(a1 * (float)tempbuffer[i] + a2 * (float)tempbuffer[i-1] + a3 * (float)tempbuffer[i-2] - b1*(float)last_1 - b2*(float)last_2+0.5f));
      i++;
      samples[i]=Saturate((int)(a1 * (float)tempbuffer[i] + a2 * (float)tempbuffer[i-1] + a3 * (float)tempbuffer[i-2] - b1*(float)samples[0] - b2*(float)last_1+0.5f));
      i++;
      for (; i<count; ++i) {
         samples[i]=Saturate((int)(a1 * (float)tempbuffer[i] + a2 * (float)tempbuffer[i-1] + a3 * (float)tempbuffer[i-2] - b1*(float)samples[i-1] - b2*(float)samples[i-2]+0.5f));
      }
      last_1=samples[count-1];
      last_2=samples[count-2];
      lastsample=start+count;
    }
    
    
  } else {
    //Algorithm 2: 
    // Only lowpass, but doesn't seem to amplify as much
    
    if (start==0) {
      l_vibrapos = 0;
      l_vibraspeed = 0;
      r_vibrapos = 0;
      r_vibraspeed = 0;
    }
    child->GetAudio(buf, start, count, env);
    float amp = 0.9f; 
    float w = 2.0*PI*(float)cutoff/(float)vi.audio_samples_per_second; // Pole angle
    float q = 1.0-w/(2.0*(amp+0.5/(1.0+w))+w-2.0); // Pole magnitude
    float r = q*q;
    float c = r+1.0-2.0*cos(w)*q;
    float temp;
    short* samples = (short*)buf;
    if (vi.AudioChannels()==2) {
      for (unsigned int i=0; i<count; ++i) { 
        
        // Accelerate vibra by signal-vibra, multiplied by lowpasscutoff 
        l_vibraspeed += ((float)samples[i*2] - l_vibrapos) * c;
        r_vibraspeed += ((float)samples[i*2+1] - r_vibrapos) * c;
        
        // Add velocity to vibra's position 
        l_vibrapos += l_vibraspeed;
        r_vibrapos += r_vibraspeed;
        
        // Attenuate/amplify vibra's velocity by resonance 
        l_vibraspeed *= r;
        r_vibraspeed *= r;
        
        // Check clipping 
        temp = l_vibrapos;
        if (temp > 32767.0) {
          temp = 32767.0;
        } else if (temp < -32768.0) temp = -32768.0;
        
        // Store new value  
        samples[i*2] = (short)temp; 

        temp = r_vibrapos;
        if (temp > 32767.0) {
          temp = 32767.0;
        } else if (temp < -32768.0) temp = -32768.0;
        
        // Store new value  
        samples[i*2+1] = (short)temp; 
      }
    } else if (vi.AudioChannels()==1) {
      for (unsigned int i=0; i<count; ++i) { 
        
        // Accelerate vibra by signal-vibra, multiplied by lowpasscutoff 
        l_vibraspeed += ((float)samples[i] - l_vibrapos) * c;
        
        // Add velocity to vibra's position 
        l_vibrapos += l_vibraspeed;
        
        // Attenuate/amplify vibra's velocity by resonance 
        l_vibraspeed *= r;
        
        // Check clipping 
        temp = l_vibrapos;
        if (temp > 32767.0) {
          temp = 32767.0;
        } else if (temp < -32768.0) temp = -32768.0;
        
        // Store new value  
        samples[i] = (short)temp; 
      }
    }
  }
}


AVSValue __cdecl FilterAudio::Create_LowPass(AVSValue args, void*, IScriptEnvironment* env) 
{
  int cutoff = args[1].AsInt();
  float rez = args[2].AsFloat(0.2);
  return new FilterAudio(args[0].AsClip(), cutoff, rez,2);
}


AVSValue __cdecl FilterAudio::Create_HighPass(AVSValue args, void*, IScriptEnvironment* env) 
{
  int cutoff = args[1].AsInt();
  float rez = args[2].AsFloat(0.2);
  return new FilterAudio(args[0].AsClip(), cutoff, rez,0);
}

AVSValue __cdecl FilterAudio::Create_LowPassALT(AVSValue args, void*, IScriptEnvironment* env) 
{
  int cutoff = args[1].AsInt();
  float rez = 0.0f;
  return new FilterAudio(args[0].AsClip(), cutoff, rez,1);
}
  


/********************************
 *******   Resample Audio   ******
 *******************************/

// FIXME: Support float

ResampleAudio::ResampleAudio(PClip _child, int _target_rate, IScriptEnvironment* env)
  : GenericVideoFilter(ConvertAudio::Create(_child,SAMPLE_INT16|SAMPLE_FLOAT,SAMPLE_FLOAT)), target_rate(_target_rate)
{
  if ((target_rate==vi.audio_samples_per_second)||(vi.audio_samples_per_second==0)) {
		skip_conversion=true;
		return;
	} 
	skip_conversion=false;
	factor = double(target_rate) / vi.audio_samples_per_second;
  srcbuffer=0;

  vi.num_audio_samples = MulDiv(vi.num_audio_samples, target_rate, vi.audio_samples_per_second);
  vi.audio_samples_per_second = target_rate;

  // generate filter coefficients
  makeFilter(Imp, &LpScl, Nwing, 0.90, 9);
  Imp[Nwing] = 0; // for "interpolation" beyond last coefficient

  /* Calc reach of LP filter wing & give some creeping room */
  Xoff = int(((Nmult+1)/2.0) * max(1.0,1.0/factor)) + 10;

  // Attenuate resampler scale factor by 0.95 to reduce probability of clipping
  LpScl = int(LpScl * 0.95);
  /* Account for increased filter gain when using factors less than 1 */
  if (factor < 1)
    LpScl = int(LpScl*factor + 0.5);

  double dt = 1.0/factor;            /* Output sampling period */
  dtb = int(dt*(1<<Np) + 0.5);
  double dh = min(double(Npc), factor*Npc);  /* Filter sampling period */
  dhb = int(dh*(1<<Na) + 0.5);
}


void __stdcall ResampleAudio::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
  if (skip_conversion) {
		child->GetAudio(buf, start, count, env);
		return;
	}
  __int64 src_start = __int64(start / factor * (1<<Np) + 0.5);
  __int64 src_end = __int64((start+count) / factor * (1<<Np) + 0.5);
  const int source_samples = int(src_end>>Np)-int(src_start>>Np)+2*Xoff+1;
  const int source_bytes = vi.BytesFromAudioSamples(source_samples);
  if (!srcbuffer || source_bytes > srcbuffer_size) 
  {
    delete[] srcbuffer;
    srcbuffer = new short[source_bytes>>1];
    srcbuffer_size = source_bytes;
  }
  child->GetAudio(srcbuffer, int(src_start>>Np)-Xoff, source_samples, env);

  int pos = (int(src_start) & Pmask) + (Xoff << Np);
  int pos_end = int(src_end) - (int(src_start) & ~Pmask) + (Xoff << Np);
  short* dst = (short*)buf;

  if (vi.AudioChannels()==1) {
    while (pos < pos_end) 
    {
      short* Xp = &srcbuffer[pos>>Np];
      int v = FilterUD(Xp, (short)(pos&Pmask), -1);  /* Perform left-wing inner product */
      v += FilterUD(Xp+1, (short)((-pos)&Pmask), 1);  /* Perform right-wing inner product */
      v >>= Nhg;      /* Make guard bits */
      v *= LpScl;     /* Normalize for unity filter gain */
      *dst++ = IntToShort(v,NLpScl);   /* strip guard bits, deposit output */
      pos += dtb;       /* Move to next sample by time increment */
    }
  }
  else if (vi.AudioChannels()==2) {
    while (pos < pos_end) 
    {
      short* Xp = &srcbuffer[(pos>>Np)*2];
      int v = FilterUD(Xp, (short)(pos&Pmask), -2);
      v += FilterUD(Xp+2, (short)((-pos)&Pmask), 2);
      v >>= Nhg;
      v *= LpScl;
      *dst++ = IntToShort(v,NLpScl);
      int w = FilterUD(Xp+1, (short)(pos&Pmask), -2);
      w += FilterUD(Xp+3, (short)((-pos)&Pmask), 2);
      w >>= Nhg;
      w *= LpScl;
      *dst++ = IntToShort(w,NLpScl);
      pos += dtb;
    }
  }
}

  
AVSValue __cdecl ResampleAudio::Create(AVSValue args, void*, IScriptEnvironment* env) 
{
  return new ResampleAudio(args[0].AsClip(), args[1].AsInt(), env);
}


int ResampleAudio::FilterUD(short *Xp, short Ph, short Inc) 
{
  int v=0;
  unsigned Ho = (Ph*(unsigned)dhb)>>Np;
  unsigned End = Nwing;
  if (Inc > 0)        /* If doing right wing...              */
  {               /* ...drop extra coeff, so when Ph is  */
    End--;          /*    0.5, we don't do too many mult's */
    if (Ph == 0)        /* If the phase is zero...           */
      Ho += dhb;        /* ...then we've already skipped the */
  }               /*    first sample, so we must also  */
              /*    skip ahead in Imp[] and ImpD[] */
  while ((Ho>>Na) < End) {
    int t = Imp[Ho>>Na];      /* Get IR sample */
    int a = Ho & Amask;   /* a is logically between 0 and 1 */
    t += ((int(Imp[(Ho>>Na)+1])-t)*a)>>Na; /* t is now interp'd filter coeff */
    t *= *Xp;     /* Mult coeff by input sample */
    if (t & 1<<(Nhxn-1))  /* Round, if needed */
      t += 1<<(Nhxn-1);
    t >>= Nhxn;       /* Leave some guard bits, but come back some */
    v += t;           /* The filter output */
    Ho += dhb;        /* IR step */
    Xp += Inc;        /* Input signal step. NO CHECK ON BOUNDS */
  }
  return(v);
}









/********************************
 *******   Helper methods *******
 ********************************/

double Izero(double x)
{
   double sum, u, halfx, temp;
   int n;

   sum = u = n = 1;
   halfx = x/2.0;
   do {
      temp = halfx/(double)n;
      n += 1;
      temp *= temp;
      u *= temp;
      sum += u;
      } while (u >= IzeroEPSILON*sum);
   return(sum);
}


void LpFilter(double c[], int N, double frq, double Beta, int Num)
{
   double IBeta, temp, inm1;
   int i;

   /* Calculate ideal lowpass filter impulse response coefficients: */
   c[0] = 2.0*frq;
   for (i=1; i<N; i++) {
       temp = PI*(double)i/(double)Num;
       c[i] = sin(2.0*temp*frq)/temp; /* Analog sinc function, cutoff = frq */
   }

   /* 
    * Calculate and Apply Kaiser window to ideal lowpass filter.
    * Note: last window value is IBeta which is NOT zero.
    * You're supposed to really truncate the window here, not ramp
    * it to zero. This helps reduce the first sidelobe. 
    */
   IBeta = 1.0/Izero(Beta);
   inm1 = 1.0/((double)(N-1));
   for (i=1; i<N; i++) {
       temp = (double)i * inm1;
       c[i] *= Izero(Beta*sqrt(1.0-temp*temp)) * IBeta;
   }
}


/* ERROR return codes:
 *    0 - no error
 *    1 - Nwing too large (Nwing is > MAXNWING)
 *    2 - Froll is not in interval [0:1)
 *    3 - Beta is < 1.0
 *
 */

int makeFilter( short Imp[], int *LpScl, unsigned short Nwing, double Froll, double Beta)
{
   static const int MAXNWING = 8192;
   static double ImpR[MAXNWING];

   double DCgain, Scl, Maxh;
   short Dh;
   int i;

   if (Nwing > MAXNWING)                      /* Check for valid parameters */
      return(1);
   if ((Froll<=0) || (Froll>1))
      return(2);
   if (Beta < 1)
      return(3);

   /* 
    * Design Kaiser-windowed sinc-function low-pass filter 
    */
   LpFilter(ImpR, (int)Nwing, 0.5*Froll, Beta, Npc); 

   /* Compute the DC gain of the lowpass filter, and its maximum coefficient
    * magnitude. Scale the coefficients so that the maximum coeffiecient just
    * fits in Nh-bit fixed-point, and compute LpScl as the NLpScl-bit (signed)
    * scale factor which when multiplied by the output of the lowpass filter
    * gives unity gain. */
   DCgain = 0;
   Dh = Npc;                       /* Filter sampling period for factors>=1 */
   for (i=Dh; i<Nwing; i+=Dh)
      DCgain += ImpR[i];
   DCgain = 2*DCgain + ImpR[0];    /* DC gain of real coefficients */

   for (Maxh=i=0; i<Nwing; i++)
      Maxh = max(Maxh, fabs(ImpR[i]));

   Scl = ((1<<(Nh-1))-1)/Maxh;     /* Map largest coeff to 16-bit maximum */
   *LpScl = int(fabs((1<<(NLpScl+Nh))/(DCgain*Scl)));

   /* Scale filter coefficients for Nh bits and convert to integer */
   if (ImpR[0] < 0)                /* Need pos 1st value for LpScl storage */
      Scl = -Scl;
   for (i=0; i<Nwing; i++)         /* Scale & round them */
      Imp[i] = int(ImpR[i] * Scl + 0.5);

   return(0);
}
