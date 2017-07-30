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
        bool is_held();
        friend class Lock;
    private:
        std::atomic<bool> m_lock;
    };

    class Lock {
    public:
        explicit Lock(Mutex & m);
        ~Lock();
        void lock();
        void unlock();
    private:
        Mutex * m_mutex;
    };
}

#endif
