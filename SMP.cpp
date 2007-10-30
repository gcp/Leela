#include "config.h"

#ifdef WIN32
#include <windows.h>
#include <intrin.h>
#endif

#include "SMP.h"

SMP::Mutex::Mutex() {
    m_lock = 0;
}

SMP::Lock::Lock(Mutex & m) {
    m_mutex = &m;

    while (_InterlockedExchange(&m.m_lock, 1) != 0) {
        while (m.m_lock == 1);
    }    
}

SMP::Lock::~Lock() {    
    m_mutex->m_lock = 0;    
}

int SMP::get_num_cpus() {
#ifdef WIN32
    SYSTEM_INFO sysinfo;    
    GetSystemInfo(&sysinfo);    
    return sysinfo.dwNumberOfProcessors;    
#else
    return 1;
#endif
}
