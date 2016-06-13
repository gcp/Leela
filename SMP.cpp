#include "config.h"
#include "SMP.h"

SMP::Mutex::Mutex() {
    m_lock = false;
}

SMP::Mutex::~Mutex() {
}

SMP::Lock::Lock(Mutex & m) {
    m_mutex = &m;
    while (m_mutex->m_lock.exchange(true, boost::memory_order_acquire) == true);
}

void SMP::Lock::unlock() {
    m_mutex->m_lock.store(false, boost::memory_order_release);
}

SMP::Lock::~Lock() {
    unlock();
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
