#ifndef CONFIG_INCLUDED
#define CONFIG_INCLUDED

/*  Timing code. Define one or none of:
 *
 *  GETTICKCOUNT, GETTIMEOFDAY
 */
#ifdef _WIN32
#define GETTICKCOUNT 
#undef HAVE_SELECT
#else
#define GETTIMEOFDAY
#endif

/* Hard limits */

#define NAME            "Leela"
//#define VERSION         "0.3.14. I will resign when I am lost. If you are sure you are winning but I haven't resigned yet, the status of some groups is not yet clear to me. I will pass out the game when I am won. You can download a free version at http://www.sjeng.org/leela"
#define VERSION         "0.3.14 A1"

/* Features */
//#define USE_PONDER
//#define USE_SMP

/* Integer types */

typedef int int32;
typedef short int16;
typedef signed char int8;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

/* Data type definitions */

#ifdef _WIN32
typedef __int64 int64 ;
typedef unsigned __int64 uint64;
#else
typedef long long int int64 ;
typedef  unsigned long long int uint64;
#endif

#if (_MSC_VER >= 1400) /* VC8+ Disable all deprecation warnings */
    #pragma warning(disable : 4996)     
#endif /* VC8+ */

#ifdef GETTICKCOUNT
    typedef int rtime_t;
#else
    #if defined(GETTIMEOFDAY)
        #include <sys/time.h>        
        #include <time.h>
        typedef struct timeval rtime_t;
    #else
        typedef time_t rtime_t;
    #endif
#endif

#endif
