// ZenLib::Thread - Thread functions
// Copyright (C) 2007-2009 Jerome Martinez, Zen@MediaArea.net
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#include "ZenLib/Conf_Internal.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#include "ZenLib/Thread.h"
#include <iostream>
#include <ZenLib/Ztring.h>
#include <ZenLib/CriticalSection.h>
//---------------------------------------------------------------------------

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ZENLIB_USEWX
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#ifdef ZENLIB_USEWX
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#include <wx/thread.h>
//---------------------------------------------------------------------------

namespace ZenLib
{

class ThreadEntry : public wxThread
{
public :
    ThreadEntry(Thread* Th_) : wxThread(wxTHREAD_JOINABLE)
        {Th=Th_;};
    void* Entry() {Th->Entry(); return NULL;}
private :
    Thread* Th;
};

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
Thread::Thread()
{
    ThreadPointer=(void*)new ThreadEntry(this);
    ((ThreadEntry*)ThreadPointer)->Create();
}

//---------------------------------------------------------------------------
Thread::~Thread()
{
    delete (ThreadEntry*)ThreadPointer;
}

//***************************************************************************
// Main Entry
//***************************************************************************

void Thread::Entry()
{
}

//***************************************************************************
// Control
//***************************************************************************

void Thread::Run()
{
    ((ThreadEntry*)ThreadPointer)->Resume();
}

void Thread::Pause()
{
    ((ThreadEntry*)ThreadPointer)->Pause();
}

void Thread::Stop()
{
    ((ThreadEntry*)ThreadPointer)->Delete();
}

bool Thread::IsRunning()
{
    return ((ThreadEntry*)ThreadPointer)->IsRunning();
}

//***************************************************************************
// Communicating
//***************************************************************************

void Thread::Sleep(size_t Millisecond)
{
    ((ThreadEntry*)ThreadPointer)->Sleep((unsigned long)Millisecond);
}

void Thread::Yield()
{
    ((ThreadEntry*)ThreadPointer)->Yield();
}

} //Namespace

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// WINDOWS
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#else //ZENLIB_USEWX
#ifdef WINDOWS
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#undef __TEXT
#include <windows.h>
#undef Yield
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//Config

#ifndef _MT
    #define _MT //Must have this symbol defined to get _beginthread/_endthread declarations
#endif

#ifdef __BORLANDC__
    #if !defined(__MT__)
        #define __MT__ //-tWM in the IDE is not always set
    #endif

    #if !defined(__MFC_COMPAT__)
        #define __MFC_COMPAT__ // Needed to know about _beginthreadex etc..
    #endif
#endif //__BORLANDC__


#if  defined(__VISUALC__) || \
    (defined(__GNUG__) && defined(__MSVCRT__)) || \
     defined(__WATCOMC__) || \
     defined(__MWERKS__)
    //(defined(__BORLANDC__) && (__BORLANDC__ >= 0x500))

    #ifndef __WXWINCE__
        #define USING_BEGINTHREAD //Using _beginthreadex() instead of CreateThread() if possible (better, because of Win32 API has problems with memory leaks in C library)
    #endif
#endif

#ifdef USING_BEGINTHREAD
    #include <process.h>
    typedef unsigned THREAD_RETVAL;     //The return type        of the thread function entry point
    #define THREAD_CALLCONV __stdcall   //The calling convention of the thread function entry point
#else
    // the settings for CreateThread()
    typedef DWORD THREAD_RETVAL;
    #define THREAD_CALLCONV WINAPI
#endif
//---------------------------------------------------------------------------

namespace ZenLib
{

//---------------------------------------------------------------------------
THREAD_RETVAL THREAD_CALLCONV Thread_Start(void *param)
{
    ((Thread*)param)->Entry();

    ((Thread*)param)->Internal_Exit();

    return 1;
}

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
Thread::Thread()
{
    State=State_New;

    #ifdef USING_BEGINTHREAD
        #ifdef __WATCOMC__
            const unsigned stksize=10240; //Watcom is reported to not like 0 stack size (which means "use default")
        #else
            const unsigned stksize=0; //Default
        #endif //__WATCOMC__

        ThreadPointer=(void*)_beginthreadex (NULL, stksize, Thread_Start, this, CREATE_SUSPENDED, NULL);
    #else
        ThreadPointer=(void*)CreateThread   (NULL,       0, Thread_Start, this, CREATE_SUSPENDED, NULL);
    #endif //USING_BEGINTHREAD
}

//---------------------------------------------------------------------------
Thread::~Thread()
{
    CloseHandle((HANDLE)ThreadPointer);
}

//***************************************************************************
// Control
//***************************************************************************

//---------------------------------------------------------------------------
Thread::returnvalue Thread::Run()
{
    ResumeThread((HANDLE)ThreadPointer);

    C.Enter();
    State=State_Running;
    C.Leave();
    return Ok;
}

//---------------------------------------------------------------------------
Thread::returnvalue Thread::Pause()
{
    SuspendThread((HANDLE)ThreadPointer);

    C.Enter();
    State=State_Paused;
    C.Leave();
    return Ok;
}

//---------------------------------------------------------------------------
Thread::returnvalue Thread::Terminate()
{
    C.Enter();

    if (State!=State_Running)
    {
        C.Leave();
        return IsNotRunning;
    }

    State=State_Terminating;

    C.Leave();
    return Ok;
}

//---------------------------------------------------------------------------
Thread::returnvalue Thread::Kill()
{
    //TerminateThread((HANDLE)ThreadPointer, 1);

    C.Enter();
    State=State_Exited;
    C.Leave();
    return Ok;
}

//***************************************************************************
// Status
//***************************************************************************

bool Thread::IsRunning()
{
    C.Enter();
    bool ToReturn=State==State_Running;
    C.Leave();
    return ToReturn;
}

//***************************************************************************
// Main Entry
//***************************************************************************

void Thread::Entry()
{
}

//***************************************************************************
// Communicating
//***************************************************************************

//---------------------------------------------------------------------------
void Thread::Sleep(size_t Millisecond)
{
    ::Sleep((DWORD)Millisecond);
}

//---------------------------------------------------------------------------
void Thread::Yield()
{
    ::Sleep(0);
}

//***************************************************************************
// Internal
//***************************************************************************

//---------------------------------------------------------------------------
Thread::returnvalue Thread::Internal_Exit()
{
    C.Enter();

    if (State!=State_Running
     && State!=State_Terminating)
    {
        C.Leave();
        return IsNotRunning;
    }

    State=State_Exited;

    C.Leave();
    return Ok;
}

} //Namespace

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// UNIX
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#else //WINDOWS
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#include <pthread.h>
#include <unistd.h>
#ifdef _POSIX_PRIORITY_SCHEDULING
    #include <sched.h>
#endif //_POSIX_PRIORITY_SCHEDULING
//---------------------------------------------------------------------------

namespace ZenLib
{

//---------------------------------------------------------------------------
void *Thread_Start(void *param)
{
    ((Thread*)param)->Entry();
    ((Thread*)param)->Internal_Exit();

    return NULL;
}

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
Thread::Thread()
{
    ThreadPointer=NULL;

    State=State_New;
}

//---------------------------------------------------------------------------
Thread::~Thread()
{
    Terminate();
    Kill();
}

//***************************************************************************
// Main Entry
//***************************************************************************

void Thread::Entry()
{
}

//***************************************************************************
// Control
//***************************************************************************

Thread::returnvalue Thread::Run()
{
    if (ThreadPointer==NULL)
    {
        //Configuration
        pthread_attr_t Attr;
        pthread_attr_init(&Attr);

        pthread_attr_setdetachstate(&Attr, PTHREAD_CREATE_DETACHED);

        pthread_create((pthread_t*)&ThreadPointer, &Attr, Thread_Start, (void*)this);

        C.Enter();
        State=State_Running;
        C.Leave();
    }

    return Ok;
}

Thread::returnvalue Thread::Pause()
{
    return Ok;
}

Thread::returnvalue Thread::Terminate()
{
    C.Enter();

    if (State!=State_Running)
    {
        C.Leave();
        return IsNotRunning;
    }

    State=State_Terminating;

    C.Leave();
    return Ok;
}

Thread::returnvalue Thread::Kill()
{
    return Ok;
}

//***************************************************************************
// Status
//***************************************************************************

bool Thread::IsRunning()
{
    C.Enter();
    bool ToReturn=State==State_Running;
    C.Leave();
    return ToReturn;
}

//***************************************************************************
// Communicating
//***************************************************************************

void Thread::Sleep(size_t)
{
}

void Thread::Yield()
{
    #ifdef _POSIX_PRIORITY_SCHEDULING
        sched_yield();
    #endif //_POSIX_PRIORITY_SCHEDULING
}

//***************************************************************************
// Internal
//***************************************************************************

Thread::returnvalue Thread::Internal_Exit()
{
    C.Enter();

    if (State!=State_Running
     && State!=State_Terminating)
    {
        C.Leave();
        return IsNotRunning;
    }

    State=State_Exited;

    C.Leave();
    return Ok;
}

} //Namespace

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#endif //WINDOWS
#endif //ZENLIB_USEWX
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

