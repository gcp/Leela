#include "config.h"

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef WIN32
#include <windows.h>
#endif

#include "Utils.h"

bool Utils::input_causes_stop() {
    char c;

    c = std::cin.get();
    std::cin.unget();
    
    return true;          
}

bool Utils::input_pending(void) {
#ifdef HAVE_SELECT
    FD_ZERO(&read_fds);
    FD_SET(0,&read_fds);
    timeout.tv_sec = timeout.tv_usec = 0;
    select(1,&read_fds,NULL,NULL,&timeout);
    if (FD_ISSET(0,&read_fds)) {
        return input_causes_stop();            
    } else {
        return false;
    }
#else
    static int init = 0, pipe;
    static HANDLE inh;
    DWORD dw;
    
    if (!init) {
        init = 1;
        inh = GetStdHandle(STD_INPUT_HANDLE);
        pipe = !GetConsoleMode(inh, &dw);
        if (!pipe) {
            SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
            FlushConsoleInputBuffer(inh);
        }
    }

    if (pipe) {
        if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) {
            myprintf("Nothing at other end - exiting\n");                        
            exit(EXIT_FAILURE);                
        }                         

        if (dw) {                    
            return input_causes_stop();
        } else {
            return false;
        }
    } else {
        if (!GetNumberOfConsoleInputEvents(inh, &dw)) {
            myprintf("Nothing at other end - exiting\n");                     
            exit(EXIT_FAILURE);                
        }
        
        if (dw <= 1) {
            return false;
        } else {
            return input_causes_stop();
        }
    }    
#endif
    return false;
}

#ifndef _CONSOLE
#include <wx/wx.h>
#endif

void Utils::myprintf(const char *fmt, ...) {
    va_list ap;      
  
    va_start(ap, fmt);      

#ifdef _CONSOLE    
    vfprintf(stderr, fmt, ap);
#else
    ::wxLogMessage(fmt, ap);
#endif  
  
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
