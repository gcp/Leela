#ifndef UTILS_H_DEFINED
#define UTILS_H_DEFINED

namespace Utils {

void myprintf(const char *fmt, ...);
void gtp_printf(int id, const char *fmt, ...);
void gtp_fail_printf(int id, const char *fmt, ...);

}

#endif
