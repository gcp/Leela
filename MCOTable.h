#ifndef MCOTABLE_H_INCLUDED
#define MCOTABLE_H_INCLUDED

#include <array>
#include <atomic>
#include "FastBoard.h"
#include "Playout.h"
#include "SMP.h"

class MCOwnerTable {
public:
    /*
        return the global TT
    */
    static MCOwnerTable * get_MCO();
    void clear();

    /*
        update_blackowns corresponding entry
    */
    void update_owns(Playout::bitboard_t & blacksq,
                     bool blackwon, float board_score);

    float get_blackown(const int color, const int vertex) const;
    int get_blackown_i(const int color, const int vertex) const;
    float get_criticality_f(const int vertex) const;
    float get_board_score() const;
    bool is_primed() const;

private:
    MCOwnerTable();

    std::array<std::atomic<int>, FastBoard::MAXSQ> m_mcblackowner;
    std::array<std::atomic<int>, FastBoard::MAXSQ> m_mcwinowner;
    std::atomic<int> m_mcsimuls;
    std::atomic<int> m_blackwins;
    std::atomic<double> m_blackscore;
};

#endif
