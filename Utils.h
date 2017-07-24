#ifndef UTILS_H_DEFINED
#define UTILS_H_DEFINED

#ifndef _CONSOLE
#include <wx/wx.h>
#include <wx/event.h>
#endif

#include <string>
#include <atomic>
#include "config.h"
#include "ThreadPool.h"

#ifdef _MSC_VER
#define ASSUME_ALIGNED(p, n) \
__assume((reinterpret_cast<std::size_t>(p) & ((n) - 1)) == 0)
#else
#define ASSUME_ALIGNED(p, n) \
(p) = static_cast<__typeof__(p)>(__builtin_assume_aligned((p), (n)))
#endif

extern ThreadPool thread_pool;

namespace Utils {
#ifndef _CONSOLE
    void setGUIQueue(wxEvtHandler * evt, int evt_type);
    void setAnalysisQueue(wxEvtHandler * evt, int a_evt_type, int m_evt_type);
#endif
    void GUIprintf(const char *fmt, ...);
    void GUIAnalysis(void* data);
    void GUIBestMoves(void* data);

    void myprintf(const char *fmt, ...);
    void gtp_printf(int id, const char *fmt, ...);
    void gtp_fail_printf(int id, const char *fmt, ...);
    void log_input(std::string input);
    bool input_pending();
    bool input_causes_stop();

    template<class T>
    void atomic_add(std::atomic<T> &f, T d) {
        T old = f.load();
        while (!f.compare_exchange_weak(old, old + d));
    }

    template<class T>
    bool is_aligned(T* ptr, size_t alignment) {
        return (uintptr_t(ptr) & (alignment - 1)) == 0;
    }

    inline uint32 rotl32(const uint32 x, const int k) {
	    return (x << k) | (x >> (32 - k));
    }

    inline uint16 pattern_hash(uint32 h) {
        constexpr uint32 c1 = 0xc54be620;
        constexpr uint32 c2 = 0x7766d421;
        constexpr uint32 s1 = 26;
        constexpr uint32 s2 = 20;
        h  = Utils::rotl32(h, 11) ^ ((h + c1) >> s1);
        h *= c2;
        h ^= h >> s2;
        return h & 8191;
    }
}

#endif
