#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef WIN32
#include <windows.h>
#endif

#include "Utils.h"

int Utils::get_num_cpus() {
#ifdef WIN32
    SYSTEM_INFO sysinfo;    
    GetSystemInfo(&sysinfo);    
    return sysinfo.dwNumberOfProcessors;    
#else
    return 1;
#endif
}

void Utils::myprintf(const char *fmt, ...) {
    va_list ap;      
  
    va_start(ap, fmt);      
    
    vfprintf(stderr, fmt, ap);
  
    va_end(ap);
}

void Utils::gtp_printf(int id, const char *fmt, ...) {
    va_list ap;  
  
    if (id != -1) {
        fprintf(stdout, "=%d ", id);
    } else {
        fprintf(stdout, "= ");
    }        
  
    va_start(ap, fmt);  
    
    vfprintf(stdout, fmt, ap);
    printf("\n\n");
  
    va_end(ap);
}

void Utils::gtp_fail_printf(int id, const char *fmt, ...) {
    va_list ap;  
      
    if (id != -1) { 
        fprintf(stdout, "?%d ", id);
    } else {
        fprintf(stdout, "? ");
    }
  
    va_start(ap, fmt);       
        
    vfprintf(stdout, fmt, ap);
    printf("\n\n");                   
  
    va_end(ap);
}
