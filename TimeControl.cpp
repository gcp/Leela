#include "TimeControl.h"
#include "Utils.h"

using namespace Utils;

TimeControl::TimeControl(int maintime, int byotime, int byostones)
: m_maintime(maintime), m_byotime(byotime), m_byostones(byostones) {

    m_remaining_time[0] = m_maintime;
    m_remaining_time[1] = m_maintime;
    m_stones_left[0] = m_byostones;
    m_stones_left[1] = m_byostones;
    m_inbyo[0] = false;
    m_inbyo[1] = false;
}

void TimeControl::start(int color) {
    m_times[color] = Time();
}

void TimeControl::stop(int color) {
    Time stop;
    int elapsed = Time::timediff(m_times[color], stop);
    
    assert(elapsed >= 0);    
    
    m_remaining_time[color] -= elapsed;
    
    if (m_inbyo[color]) {
        m_stones_left[color]--;
        
        assert(m_stones_left[color] >= 0);
    }
    
    /*
        time up, entering byo yomi 
    */
    if (!m_inbyo[color] && m_remaining_time[color] < 0) {
        m_remaining_time[color] = m_byotime;
        m_stones_left[color] = m_byostones;
        m_inbyo[color] = true;
    } else if (m_inbyo[color] && m_stones_left[color] == 0) {
        m_remaining_time[color] = m_byotime;        
        m_stones_left[color] = m_byostones;
    }           
}

bool TimeControl::time_forfeit(int color) {

    if (m_inbyo[color] && m_remaining_time[color] < 0) {
        return true;   
    }
    
    return false;
}

void TimeControl::display_times() {
    {        
        int rem = m_remaining_time[0] / 100;  /* centiseconds to seconds */
        int hours = rem / (60 * 60);
        rem = rem % (60 * 60);
        int minutes = rem / 60;
        rem = rem % 60;
        int seconds = rem;
        myprintf("Black time: %02d:%02d:%02d", hours, minutes, seconds);
        if (m_inbyo[0]) {
            myprintf(", %d stones left", m_stones_left[0]);
        }
        myprintf("\n");
    }
    {
        int rem = m_remaining_time[1] / 100;  /* centiseconds to seconds */
        int hours = rem / (60 * 60);
        rem = rem % (60 * 60);
        int minutes = rem / 60;
        rem = rem % 60;
        int seconds = rem;
        myprintf("White time: %02d:%02d:%02d", hours, minutes, seconds);
        if (m_inbyo[1]) {
            myprintf(", %d stones left", m_stones_left[1]);
        }
        myprintf("\n");
    }
    myprintf("\n");
}
