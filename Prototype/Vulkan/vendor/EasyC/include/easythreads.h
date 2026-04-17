#ifndef EASYTHREADS_H
#define EASYTHREADS_H

#ifdef __linux__

#include <pthread.h>

typedef pthread_t EZ_THREAD;
typedef pthread_mutex_t EZ_MUTEX;
typedef pthread_cond_t EZ_COND;

#define EZ_THREAD_RETURN_TYPE void*
#define EZ_THREAD_PARAMETER_TYPE void*
#define EZ_CREATE_THREAD(thread, func, parameters) pthread_create(&thread, NULL, (void* (*)(void*))func, parameters)
#define EZ_WAIT_THREAD(thread) pthread_join(thread, NULL)
#define EZ_CREATE_MUTEX(mutex) pthread_mutex_init(&mutex, NULL);
#define EZ_LOCK_MUTEX(mutex) pthread_mutex_lock(&mutex)
#define EZ_RELEASE_MUTEX(mutex) pthread_mutex_unlock(&mutex)
#define EZ_CREATE_COND(cond) pthread_cond_init(&cond, NULL);
#define EZ_WAIT_COND(cond, mutex) pthread_cond_wait(&cond, &mutex)
#define EZ_SIGNAL_COND(cond) pthread_cond_signal(&cond)
#define EZ_BROADCAST_COND(cond) pthread_cond_broadcast(&cond)

#elif _WIN32

// basic cleanup to avoid windows lib bloat
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // VK_*
#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS      // SM_*
#define NOMENUS           // MF_*
#define NOICONS           // IDI_*
#define NOKEYSTATES       // MK_*
#define NOSYSCOMMANDS     // SC_*
#define NORASTEROPS       // Binary and Tertiary raster ops
#define NOSHOWWINDOW      // SW_*
#define OEMRESOURCE       // OEM Resource values
#define NOATOM            // Atom Manager routines
#define NOCLIPBOARD       // Clipboard routines
#define NOCOLOR           // Screen colors
#define NOCTLMGR          // Control and Dialog routines
#define NODRAWTEXT        // DrawText() and DT_*
#define NOGDI             // All GDI defines and routines
#define NOKERNEL          // All KERNEL defines and routines
#define NOUSER            // All USER defines and routines
#define NOMB              // MB_* and MessageBox()
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // typedef METAFILEPICT
#define NOMSG             // typedef MSG and associated routines
#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // SB_* and scrolling routines
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // Sound driver routines
#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
#define NOWH              // SetWindowsHook and WH_*
#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOHELP            // Help engine interface.
#define NOPROFILER        // Profiler interface.
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NOMCX             // Modem Configuration Extensions

#include <windows.h>

typedef HANDLE EZ_THREAD;
typedef CRITICAL_SECTION EZ_MUTEX;
typedef CONDITION_VARIABLE EZ_COND;

#define EZ_THREAD_RETURN_TYPE DWORD WINAPI
#define EZ_THREAD_PARAMETER_TYPE LPVOID
#define EZ_CREATE_THREAD(thread, func, parameters) { thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, (LPVOID)parameters, 0, NULL); }
#define EZ_WAIT_THREAD(thread) WaitForSingleObject(thread, INFINITE)
#define EZ_CREATE_MUTEX(mutex) InitializeCriticalSection(&mutex);
#define EZ_LOCK_MUTEX(mutex) EnterCriticalSection(&mutex)
#define EZ_RELEASE_MUTEX(mutex) LeaveCriticalSection(&mutex)
#define EZ_CREATE_COND(cond) InitializeConditionVariable(&cond);
#define EZ_WAIT_COND(cond, mutex) SleepConditionVariableCS(&cond, &mutex, INFINITE);
#define EZ_SIGNAL_COND(cond) WakeConditionVariable(&cond)
#define EZ_BROADCAST_COND(cond) WakeAllConditionVariable(&cond)

#else
#error Unsupported operating system detected!
#endif

#endif
