#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "config.h"

namespace Utils {

void myprintf(const char *fmt, ...) {
    va_list ap;      
  
    va_start(ap, fmt);      
    
    vfprintf(stderr, fmt, ap);
  
    va_end(ap);
}

void gtp_printf(int id, const char *fmt, ...) {
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

void gtp_fail_printf(int id, const char *fmt, ...) {
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

};