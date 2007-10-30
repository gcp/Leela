#ifndef SMP_H_INCLUDED
#define SMP_H_INCLUDED

#include "config.h"

namespace SMP {

    int get_num_cpus();      

    class Mutex {
    public:         
        Mutex();    
        friend class Lock;
    private:   
        volatile long m_lock;
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
