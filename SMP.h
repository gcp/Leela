#ifndef SMP_H_INCLUDED
#define SMP_H_INCLUDED

#include "config.h"
#include <atomic>

namespace SMP {
    int get_num_cpus();

    class Mutex {
    public:
        Mutex();
        ~Mutex();
        friend class Lock;
    private:
        std::atomic<bool> m_lock;
    };

    class Lock {
    public:
        explicit Lock(Mutex & m);
        ~Lock();
        void unlock();
    private:
        Mutex * m_mutex;
    };
}

#endif
