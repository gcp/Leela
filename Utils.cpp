#include "config.h"

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <thread>
#include <mutex>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/select.h>
#endif

#include "Utils.h"
#include "GTP.h"

Utils::ThreadPool thread_pool;

bool Utils::input_causes_stop() {
    return true;
}

bool Utils::input_pending(void) {
#ifdef HAVE_SELECT
    fd_set read_fds;
    struct timeval timeout;
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
static wxEvtHandler * GUIQ = nullptr;
static wxEvtHandler * ANALQ = nullptr;
static int GUIQ_T = 0;
static int ANALQ_T_ANAL = 0;
static int ANALQ_T_MOVES = 0;
std::mutex GUImutex;

void Utils::setGUIQueue(wxEvtHandler * evt, int evt_type) {
    std::lock_guard<std::mutex> guard(GUImutex);
    GUIQ = evt;
    GUIQ_T = evt_type;
}

void Utils::setAnalysisQueue(wxEvtHandler * evt, int an_evt_type,
                                                 int mv_evt_type) {
    std::lock_guard<std::mutex> guard(GUImutex);
    ANALQ = evt;
    ANALQ_T_ANAL = an_evt_type;
    ANALQ_T_MOVES = mv_evt_type;
}
#else
std::mutex IOmutex;
#endif

void Utils::GUIAnalysis(void* data) {
#ifndef _CONSOLE
    std::lock_guard<std::mutex> guard(GUImutex);
    if (ANALQ != NULL) {
        wxCommandEvent* myevent = new wxCommandEvent(ANALQ_T_ANAL);
        myevent->SetClientData(data);
        ::wxQueueEvent(ANALQ, myevent);
    }
#endif
}

void Utils::GUIBestMoves(void* data) {
#ifndef _CONSOLE
    std::lock_guard<std::mutex> guard(GUImutex);
    if (ANALQ != NULL) {
        wxCommandEvent* myevent = new wxCommandEvent(ANALQ_T_MOVES);
        myevent->SetClientData(data);
        ::wxQueueEvent(ANALQ, myevent);
    }
#endif
}

void Utils::GUIprintf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
#ifndef _CONSOLE
    std::lock_guard<std::mutex> guard(GUImutex);
    if (GUIQ != nullptr) {
        char buffer[512];
        vsprintf_s(buffer, 512, fmt, ap);
        wxCommandEvent* myevent = new wxCommandEvent(GUIQ_T);
        myevent->SetString(wxString(buffer));
        ::wxQueueEvent(GUIQ, myevent);
    }
#endif
    va_end(ap);
}

void Utils::myprintf(const char *fmt, ...) {
    if (cfg_quiet) return;
#ifdef _CONSOLE
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        va_start(ap, fmt);
        vfprintf(cfg_logfile_handle, fmt, ap);
        va_end(ap);
    }
#endif
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
    va_end(ap);
    printf("\n\n");

#ifdef _CONSOLE
    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        if (id != -1) {
            fprintf(cfg_logfile_handle, "=%d ", id);
        } else {
            fprintf(cfg_logfile_handle, "= ");
        }
        va_start(ap, fmt);
        vfprintf(cfg_logfile_handle, fmt, ap);
        va_end(ap);
        fprintf(cfg_logfile_handle, "\n\n");
    }
#endif
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
    va_end(ap);
    printf("\n\n");

#ifdef _CONSOLE
    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        if (id != -1) {
            fprintf(cfg_logfile_handle, "?%d ", id);
        } else {
            fprintf(cfg_logfile_handle, "? ");
        }
        va_start(ap, fmt);
        vfprintf(cfg_logfile_handle, fmt, ap);
        va_end(ap);
        fprintf(cfg_logfile_handle, "\n\n");
    }
#endif
}

void Utils::log_input(std::string input) {
#ifdef _CONSOLE
    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        fprintf(cfg_logfile_handle, ">>%s\n", input.c_str());
    }
#endif
}
