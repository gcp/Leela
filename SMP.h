#ifndef SMP_H_INCLUDED
#define SMP_H_INCLUDED

#include "config.h"

#ifndef WIN32
#include <pthread.h>
#endif

namespace SMP {
    int get_num_cpus();      

    class Mutex {
    public:         
        Mutex();   
        ~Mutex();
        friend class Lock;
    private:
#ifdef WIN32
        volatile long m_lock;
#else
        pthread_spinlock_t m_lock;
#endif
    };

    class Lock {
    public:
        explicit Lock(Mutex & m);
        ~Lock();        
    private:
        Mutex * m_mutex;    
    };
}

#endif
