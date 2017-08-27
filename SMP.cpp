#include "config.h"
#include "SMP.h"

#include <thread>

SMP::Mutex::Mutex() {
    m_lock = false;
}

bool SMP::Mutex::is_held() {
    return m_lock.load(std::memory_order_acquire);
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
