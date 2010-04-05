#ifndef UTILS_H_DEFINED
#define UTILS_H_DEFINED

#ifndef _CONSOLE
#include <wx/wx.h>
#include <wx/event.h>
#endif

namespace Utils {  
#ifndef _CONSOLE
    void setGUIQueue(wxEvtHandler * evt, int evt_type);    
#endif    
    void GUIprintf(const char *fmt, ...);        
    
    void myprintf(const char *fmt, ...);
    void gtp_printf(int id, const char *fmt, ...);
    void gtp_fail_printf(int id, const char *fmt, ...);       
    bool input_pending();    
    bool input_causes_stop();        
}

#endif
