#include "config.h"
#include "SMP.h"

#include <thread>

SMP::Mutex::Mutex() {
    m_lock = false;
}

SMP::Mutex::~Mutex() {
}

SMP::Lock::Lock(Mutex & m) {
    m_mutex = &m;
    lock();
}

void SMP::Lock::lock() {
    while (m_mutex->m_lock.exchange(true, std::memory_order_acquire) == true);
}

void SMP::Lock::unlock() {
    m_mutex->m_lock.store(false, std::memory_order_release);
}

SMP::Lock::~Lock() {
    unlock();
}

int SMP::get_num_cpus() {
    return std::thread::hardware_concurrency();
}
