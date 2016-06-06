#include "config.h"

#ifdef WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include "SMP.h"

SMP::Mutex::Mutex() {
#ifdef WIN32
    m_lock = 0;
#else
    pthread_spin_init(&m_lock, PTHREAD_PROCESS_PRIVATE);
#endif
}

SMP::Mutex::~Mutex() {
#ifndef WIN32
    pthread_spin_destroy(&m_lock);
#endif
}

SMP::Lock::Lock(Mutex & m) {
    m_mutex = &m;
#ifdef WIN32
    while (_InterlockedExchange(&m.m_lock, 1) != 0) {
        while (m.m_lock == 1);
    }
#else
    pthread_spin_lock(&m_mutex->m_lock);
#endif
}

SMP::Lock::~Lock() {
#ifdef WIN32
    m_mutex->m_lock = 0;
#else
    pthread_spin_unlock(&m_mutex->m_lock);
#endif
}

int SMP::get_num_cpus() {
#ifdef WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}
