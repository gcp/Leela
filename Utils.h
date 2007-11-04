#ifndef UTILS_H_DEFINED
#define UTILS_H_DEFINED

namespace Utils {
    void GUIprintf(const char *fmt, ...);
    void myprintf(const char *fmt, ...);
    void gtp_printf(int id, const char *fmt, ...);
    void gtp_fail_printf(int id, const char *fmt, ...);       
    bool input_pending();    
    bool input_causes_stop();    
}

#endif
