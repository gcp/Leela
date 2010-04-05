#include <ctime>
#include <cstdlib>

#include "config.h"

#include "Timing.h"


int Time::timediff (Time start, Time end) {
    int diff;
    
#ifdef GETTICKCOUNT
    diff = ((end.m_time-start.m_time)+5)/10;
#elif (defined(GETTIMEOFDAY))
    diff = ((end.m_time.tv_sec-start.m_time.tv_sec)*100 
           + (end.m_time.tv_usec-start.m_time.tv_usec)/10000);
#else
    diff = (100*(int) difftime (end.m_time, start.m_time));
#endif

    return (abs(diff));
}

Time::Time(void) {

#if defined (GETTICKCOUNT)
    m_time = (int)(GetTickCount());
#elif defined(GETTIMEOFDAY)  
    struct timeval tmp;  
    gettimeofday(&tmp, NULL);  
    m_time = tmp;
#else
    m_time = time (0);
#endif

}
