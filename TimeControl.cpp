#include <cassert>
#include "TimeControl.h"
#include "Utils.h"

using namespace Utils;

TimeControl::TimeControl(int boardsize, int maintime, int byotime,
                         int byostones, int byoperiods)
    : m_maintime(maintime),
      m_byotime(byotime),
      m_byostones(byostones),
      m_byoperiods(byoperiods) {

    reset_clocks();
    set_boardsize(boardsize);
}

void TimeControl::reset_clocks() {
    m_remaining_time[0] = m_maintime;
    m_remaining_time[1] = m_maintime;
    m_stones_left[0] = m_byostones;
    m_stones_left[1] = m_byostones;
    m_periods_left[0] = m_byoperiods;
    m_periods_left[1] = m_byoperiods;
    m_inbyo[0] = m_maintime <= 0;
    m_inbyo[1] = m_maintime <= 0;
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
        if (m_byostones) {
            m_stones_left[color]--;
        } else if (m_byoperiods) {
            if (elapsed > m_byotime) {
                m_periods_left[color]--;
            }
        }
    }

    /*
        time up, entering byo yomi
    */
    if (!m_inbyo[color] && m_remaining_time[color] <= 0) {
        m_remaining_time[color] = m_byotime;
        m_stones_left[color] = m_byostones;
        m_periods_left[color] = m_byoperiods;
        m_inbyo[color] = true;
    } else if (m_inbyo[color] && m_byostones && m_stones_left[color] <= 0) {
        // reset byoyomi time and stones
        m_remaining_time[color] = m_byotime;
        m_stones_left[color] = m_byostones;
    } else if (m_inbyo[color] && m_byoperiods) {
        m_remaining_time[color] = m_byotime;
    }
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
            if (m_byostones) {
                myprintf(", %d stones left", m_stones_left[0]);
            } else if (m_byoperiods) {
                myprintf(", %d period(s) of %d seconds left",
                         m_periods_left[0], m_byotime / 100);
            }
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
            if (m_byostones) {
                myprintf(", %d stones left", m_stones_left[1]);
            } else if (m_byoperiods) {
                myprintf(", %d period(s) of %d seconds left",
                         m_periods_left[1], m_byotime / 100);
            }
        }
        myprintf("\n");
    }
    myprintf("\n");
}

int TimeControl::max_time_for_move(int color) {
    /*
        always keep a 2 second margin for net hiccups
    */
    static const int BUFFER_CENTISECS = 200;

    int timealloc = 0;

    /*
        no byo yomi (absolute), easiest
    */
    if (m_byotime == 0) {
        timealloc = (m_remaining_time[color] - BUFFER_CENTISECS)
                    / m_moves_expected;
    } else if (m_byotime != 0) {
        /*
          no periods or stones set means
          infinite time = 1 month
        */
        if (m_byostones == 0 && m_byoperiods == 0) {
            return 31 * 24 * 60 * 60 * 100;
        }

        /*
          byo yomi and in byo yomi
        */
        if (m_inbyo[color]) {
            if (m_byostones) {
                timealloc = (m_remaining_time[color] - BUFFER_CENTISECS) / m_byostones;
            } else {
                assert(m_byperiods);
                // Just pretend we have a flat time remaining.
                int remaining = m_byotime * (m_periods_left[color] - 1);
                timealloc = (remaining - BUFFER_CENTISECS) / m_moves_expected;
                // Add back the guaranteed extra seconds
                timealloc += m_byotime - BUFFER_CENTISECS;
            }
        } else {
            /*
              byo yomi time but not in byo yomi yet
            */
            if (m_byostones) {
                int byo_extra = m_byotime / m_byostones;
                int total_time = m_remaining_time[color] + byo_extra;
                timealloc = (total_time - BUFFER_CENTISECS) / m_moves_expected;
                // Add back the guaranteed extra seconds
                timealloc += byo_extra - BUFFER_CENTISECS;
            } else {
                assert(m_byoperiods);
                int byo_extra = m_byotime * (m_periods_left[color] - 1);
                int total_time = m_remaining_time[color] + byo_extra;
                timealloc = (total_time - BUFFER_CENTISECS) / m_moves_expected;
                // Add back the guaranteed extra seconds
                timealloc += m_byotime - BUFFER_CENTISECS;
            }
        }
    }

    timealloc = std::max<int>(timealloc, 0);
    timealloc = std::min<int>(timealloc, m_remaining_time[color]);
    return timealloc;
}

void TimeControl::adjust_time(int color, int time, int stones) {
    m_remaining_time[color] = time;
    if (stones) {
        m_inbyo[color] = true;
    }
    if (m_byostones) {
        m_stones_left[color] = stones;
    } else {
        // KGS extension
        assert(m_byoperiods);
        m_periods_left[color] = stones;
    }
}

void TimeControl::set_boardsize(int boardsize) {
    // Note this is constant as we play, so it's fair
    // to underestimate quite a bit.
    // was: bs^2 / 4 and then later 1 / z  * (3 / 2)
    m_moves_expected = (boardsize * boardsize) / 6;
}

int TimeControl::get_remaining_time(int color) {
    return m_remaining_time[color];
}