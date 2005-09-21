// Avisynth v2.6.  Copyright 2002 Ben Rudiak-Gould et al.
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

#include "stdafx.h"

#include "MT.h"

extern DWORD TlsIndex;  // in main.cpp

AVSFunction MT_filters[] = {
  { "Distributor", "c", Distributor::Create_Distributor },                    
  { 0 }
};

AVSValue __cdecl Distributor::Create_Distributor(AVSValue args, void*, IScriptEnvironment* env){
	return new Distributor(args[0].AsClip(),env);
}

Distributor::Distributor(PClip clip, IScriptEnvironment* env):GenericVideoFilter(clip),thread(0),nthreads(env->GetMTMode(true)),_env(env),lastframe(-1)
{
	TlsSetValue(TlsIndex,0);
	thread=new Thread[nthreads];
	for(unsigned int i=0;i<nthreads;i++)
	{
		thread[i].distributor=this;
		thread[i].thread=CreateThread(NULL,NULL,getframes,&thread[i],NULL,NULL);
	}
}

Distributor::~Distributor()
{
    delete[] thread;
}

DWORD Distributor::getframes(LPVOID param){
	Thread* current_thread=(Thread*)param;
	Distributor* _this=current_thread->distributor;
	IScriptEnvironment** penv=&_this->_env;
	TlsSetValue(TlsIndex,new ScriptEnvironmentTLS());
	while(true)
	{
		WaitForSingleObject(current_thread->begin,INFINITE);
		_RPT2(0,"Thread %d generating frame %d\n",GetCurrentThreadId(),current_thread->n);
		if(current_thread->endthread)
			break;
		current_thread->result=_this->child->GetFrame(current_thread->n,*penv);
		_RPT2(0,"Thread %d finished generating frame %d\n",GetCurrentThreadId(),current_thread->n);
		current_thread->status=Distributor::Thread::idle;
		SetEvent(current_thread->done);
	}
	delete((ScriptEnvironmentTLS*) TlsGetValue(TlsIndex));
	ExitThread(0);
}

void Distributor::AssignFramesToThread(int n){
	int nextframe=n;
	for(unsigned int i=0;i<nthreads;i++)
	{
		if(thread[i].n<=n&&thread[i].status==Thread::idle)
			thread[i].n=nextframe++;
	}
}

PVideoFrame Distributor::GetFrameForward(int n, IScriptEnvironment* env)
{
PVideoFrame result=0;
	int in=-1;
	int nmax=n;
	for(unsigned int i=0;i<nthreads;i++)
	{
		if(thread[i].n>nmax)
			nmax=thread[i].n;
		if(thread[i].n==n)
			if(thread[i].status==Thread::idle){
				result=thread[i].result;
				
			}
			else
				in=i;
	}
	_RPT2(0,"Distributor status loop1: result:%x, in:%d,\n",result,in);
	int nextframe=nmax+1;
	if(result==0&&in==-1)
		in=-2;
	
	for(unsigned int i=0;i<nthreads;++i==in?i++:i)
	{
		if(thread[i].n<=n&&thread[i].status==Thread::idle){
			WaitForSingleObject(thread[i].done,INFINITE);
			thread[i].status=Thread::running; 
			if(in!=-2)
				thread[i].n=nextframe++;
			else{
				thread[i].n=n;
				in=i;
			}
			_RPT2(0,"Distributor loop2 Thread %d recruted to generate frame %d\n",i,thread[i].n);
			SetEvent(thread[i].begin);}
	}
	_RPT1(0,"Distributor status loop2: in:%d\n",in);
	if(in!=-1)
	{
		WaitForSingleObject(thread[in].done,INFINITE);
		SetEvent(thread[in].done);
		result=thread[in].result;
	}
	_RPT1(0,"Distributor end status: result:%x\n",result);
	return result;
}

PVideoFrame Distributor::GetFrameBackward(int n, IScriptEnvironment* env)
{
	PVideoFrame result=0;
	int in=-1;
	int nmin=n;
	for(unsigned int i=0;i<nthreads;i++)
	{
		if(thread[i].n<nmin)
			nmin=thread[i].n;
		if(thread[i].n==n)
			if(thread[i].status==Thread::idle){
				result=thread[i].result;
			}
			else
				in=i;
	}
	_RPT2(0,"Distributor status loop1: result:%x, in:%d,\n",result,in);
	int nextframe=nmin-1;
	if(result==0&&in==-1)
		in=-2;
	
	for(unsigned int i=0;i<nthreads;++i==in?i++:i)
	{
		if(thread[i].n>=n&&thread[i].status==Thread::idle){
			WaitForSingleObject(thread[i].done,INFINITE);
			thread[i].status=Thread::running; 
			if(in!=-2)
				thread[i].n=nextframe--;
			else{
				thread[i].n=n;
				in=i;
			}
			_RPT2(0,"Distributor loop2 Thread %d recruted to generate frame %d\n",i,thread[i].n);
			SetEvent(thread[i].begin);}
	}
	_RPT1(0,"Distributor status loop2: in:%d\n",in);
	if(in!=-1)
	{
		WaitForSingleObject(thread[in].done,INFINITE);
		SetEvent(thread[in].done);
		result=thread[in].result;
	}
	_RPT1(0,"Distributor end status: result:%x\n",result);
	return result;
}

PVideoFrame Distributor::GetFrame(int n,IScriptEnvironment* env)
{
	_RPT1(0,"Distributor generating frame %d\n",n);
	int prevframe=lastframe;
	lastframe=n;
	if(prevframe<=n)
		return GetFrameForward(n,env);
	else
		return GetFrameBackward(n,env);
		
}

