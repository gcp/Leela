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
#ifdef USE_SMP
#ifdef WIN32
    m_lock = 0;
#else
    pthread_spin_init(&m_lock, PTHREAD_PROCESS_PRIVATE);
#endif
#endif
}

SMP::Mutex::~Mutex() {
#ifdef USE_SMP
#ifndef WIN32
    pthread_spin_destroy(&m_lock);
#endif
#endif
}

SMP::Lock::Lock(Mutex & m) {
    m_mutex = &m;
#ifdef USE_SMP
#ifdef WIN32
    while (_InterlockedExchange(&m.m_lock, 1) != 0) {
        while (m.m_lock == 1);
    }
#else
    pthread_spin_lock(&m_mutex->m_lock);
#endif
#endif
}

void SMP::Lock::unlock() {
#ifdef USE_SMP
#ifdef WIN32
    m_mutex->m_lock = 0;
#else
    pthread_spin_unlock(&m_mutex->m_lock);
#endif
#endif
}

SMP::Lock::~Lock() {
    unlock();
}

int SMP::get_num_cpus() {
#ifdef USE_SMP
#ifdef WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
#else
    return 1;
#endif
}
