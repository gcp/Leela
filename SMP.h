#ifndef SMP_H_INCLUDED
#define SMP_H_INCLUDED

#include "config.h"

#ifndef WIN32
#ifdef SMP
#include <pthread.h>
#endif
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
#ifdef USE_SMP
	pthread_spinlock_t m_lock;
#else   
        long m_lock;
#endif
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
