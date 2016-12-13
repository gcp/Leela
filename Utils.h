#ifndef UTILS_H_DEFINED
#define UTILS_H_DEFINED

#ifndef _CONSOLE
#include <wx/wx.h>
#include <wx/event.h>
#endif

#include <string>

#ifdef _MSC_VER
#define ASSUME_ALIGNED(p, n) \
__assume((reinterpret_cast<std::size_t>(p) & ((n) - 1)) == 0)
#else
#define ASSUME_ALIGNED(p, n) \
(p) = static_cast<__typeof__(p)>(__builtin_assume_aligned((p), (n)))
#endif

namespace Utils {
#ifndef _CONSOLE
    void setGUIQueue(wxEvtHandler * evt, int evt_type);
    void setAnalysisQueue(wxEvtHandler * evt, int evt_type);
#endif
    void GUIprintf(const char *fmt, ...);
    void AnalyzeGUI(void* data);

    void myprintf(const char *fmt, ...);
    void gtp_printf(int id, const char *fmt, ...);
    void gtp_fail_printf(int id, const char *fmt, ...);
    void log_input(std::string input);
    bool input_pending();
    bool input_causes_stop();

    template<class T>
    bool is_aligned(T* ptr, size_t alignment) {
        return (uintptr_t(ptr) & (alignment - 1)) == 0;
    }
}

#endif
