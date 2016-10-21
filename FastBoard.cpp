#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include <boost/tr1/array.hpp>

#include "config.h"

#include "FastBoard.h"
#include "Utils.h"
#include "Matcher.h"
#include "MCOTable.h"
#include "Random.h"

using namespace Utils;

const std::tr1::array<int, 2> FastBoard::s_eyemask = {
    4 * (1 << (NBR_SHIFT * BLACK)),
    4 * (1 << (NBR_SHIFT * WHITE))
};

const std::tr1::array<FastBoard::square_t, 4> FastBoard::s_cinvert = {
    WHITE, BLACK, EMPTY, INVAL
};

int FastBoard::get_boardsize(void) {
    return m_boardsize;
}

int FastBoard::get_vertex(int x, int y) {
    assert(x >= 0 && x < MAXBOARDSIZE);
    assert(y >= 0 && y < MAXBOARDSIZE);
    assert(x >= 0 && x < m_boardsize);
    assert(y >= 0 && y < m_boardsize);

    int vertex = ((y + 1) * (get_boardsize() + 2)) + (x + 1);

    assert(vertex >= 0 && vertex < m_maxsq);

    return vertex;
}

std::pair<int, int> FastBoard::get_xy(int vertex) {
    std::pair<int, int> xy;

    //int vertex = ((y + 1) * (get_boardsize() + 2)) + (x + 1);
    int x = (vertex % (get_boardsize() + 2)) - 1;
    int y = (vertex / (get_boardsize() + 2)) - 1;

    assert(x >= 0 && x < get_boardsize());
    assert(y >= 0 && y < get_boardsize());

    xy.first  = x;
    xy.second = y;

    assert(get_vertex(x, y) == vertex);

    return xy;
}

FastBoard::square_t FastBoard::get_square(int vertex) {
    assert(vertex >= 0 && vertex < MAXSQ);
    assert(vertex >= 0 && vertex < m_maxsq);    

    return m_square[vertex];
}

void FastBoard::set_square(int vertex, FastBoard::square_t content) {
    assert(vertex >= 0 && vertex < MAXSQ);
    assert(vertex >= 0 && vertex < m_maxsq);
    assert(content >= BLACK && content <= INVAL);

    m_square[vertex] = content;
}
    
FastBoard::square_t FastBoard::get_square(int x, int y) {        
    return get_square(get_vertex(x,y));
}

void FastBoard::set_square(int x, int y, FastBoard::square_t content) {    
    set_square(get_vertex(x, y), content);
}

int FastBoard::rotate_vertex(int vertex, int symmetry) {
    assert(symmetry >= 0 && symmetry <= 7);
    std::pair<int, int> xy = get_xy(vertex);
    int x = xy.first;
    int y = xy.second;
    int newx;
    int newy;

    if (symmetry == 0) {
        newx = x;
        newy = y;
    } else if (symmetry == 1) {
        newx = m_boardsize - x - 1;
        newy = y;
    } else if (symmetry == 2) {
        newx = x;
        newy = m_boardsize - y - 1;
    } else if (symmetry == 3) {
        newx = m_boardsize - x - 1;
        newy = m_boardsize - y - 1;
    } else if (symmetry == 4) {
        newx = y;
        newy = x;
    } else if (symmetry == 5) {
        newx = m_boardsize - y - 1;
        newy = x;
    } else if (symmetry == 6) {
        newx = y;
        newy = m_boardsize - x - 1;
    } else  {
        assert(symmetry == 7);
        newx = m_boardsize - y - 1;
        newy = m_boardsize - x - 1;
    }

    return get_vertex(newx, newy);
}

void FastBoard::reset_board(int size) {  
    m_boardsize = size;
    m_maxsq = (size + 2) * (size + 2);
    m_tomove = BLACK;
    m_prisoners[BLACK] = 0;
    m_prisoners[WHITE] = 0;    
    m_totalstones[BLACK] = 0;    
    m_totalstones[WHITE] = 0;    
    m_empty_cnt = 0;        

    m_dirs[0] = -size-2;
    m_dirs[1] = +1;
    m_dirs[2] = +size+2;
    m_dirs[3] = -1;    
        
    m_extradirs[0] = -size-2-1;
    m_extradirs[1] = -size-2;
    m_extradirs[2] = -size-2+1;    
    m_extradirs[3] = -1;
    m_extradirs[4] = +1;
    m_extradirs[5] = +size+2-1;
    m_extradirs[6] = +size+2;
    m_extradirs[7] = +size+2+1;        
    
    for (int i = 0; i < m_maxsq; i++) {
        m_square[i]     = INVAL;        
        m_neighbours[i] = 0;        
        m_parent[i]     = MAXSQ;        
    }      
            
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {        
            int vertex = get_vertex(i, j);            
                                    
            m_square[vertex]          = EMPTY;            
            m_empty_idx[vertex]       = m_empty_cnt;
            m_empty[m_empty_cnt++]    = vertex;            
            
            if (i == 0 || i == size - 1) {
                m_neighbours[vertex] += (1 << (NBR_SHIFT * BLACK))
                                      | (1 << (NBR_SHIFT * WHITE));
                m_neighbours[vertex] +=  1 << (NBR_SHIFT * EMPTY);                
            } else {
                m_neighbours[vertex] +=  2 << (NBR_SHIFT * EMPTY);                
            }
            
            if (j == 0 || j == size - 1) {
                m_neighbours[vertex] += (1 << (NBR_SHIFT * BLACK))
                                      | (1 << (NBR_SHIFT * WHITE));
                m_neighbours[vertex] +=  1 << (NBR_SHIFT * EMPTY); 
            } else {
                m_neighbours[vertex] +=  2 << (NBR_SHIFT * EMPTY);
            }
        }
    }         
    
    m_parent[MAXSQ] = MAXSQ;    
    m_libs[MAXSQ]   = 16384;
    m_next[MAXSQ]   = MAXSQ;                        
}

bool FastBoard::is_suicide(int i, int color) {        
    if (count_pliberties(i)) {
        return false;
    }
          
    bool connecting = false;
        
    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
                             
        int libs = m_libs[m_parent[ai]];
        if (get_square(ai) == color) {
            if (libs > 1) {
                // connecting to live group = never suicide                    
                return false;
            }   
            connecting = true;
        } else {
            if (libs <= 1) {
                // killing neighbor = never suicide
                return false;
            }
        }                            
    }
    
    add_neighbour(i, color);
    
    bool opps_live = true;
    bool ours_die = true;
    
    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
                 
        int libs = m_libs[m_parent[ai]];
        
        if (libs == 0 && get_square(ai) != color) {  
            opps_live = false;                                   
        } else if (libs != 0 && get_square(ai) == color) {
            ours_die = false;   
        }        
    }  
    
    remove_neighbour(i, color);  
    
    if (!connecting) {
        return opps_live;
    } else {
        return opps_live && ours_die;
    }           
}

int FastBoard::count_pliberties(const int i) {          
    return count_neighbours(EMPTY, i);
}

// count neighbours of color c at vertex v
int FastBoard::count_neighbours(const int c, const int v) {
    assert(c == WHITE || c == BLACK || c == EMPTY);
    return (m_neighbours[v] >> (NBR_SHIFT * c)) & 7;
}

int FastBoard::fast_ss_suicide(const int color, const int i)  {    
    int eyeplay = (m_neighbours[i] & s_eyemask[!color]);
    
    if (!eyeplay) return false;         
        
    if (m_libs[m_parent[i - 1              ]] <= 1) return false;
    if (m_libs[m_parent[i + 1              ]] <= 1) return false;
    if (m_libs[m_parent[i + m_boardsize + 2]] <= 1) return false;
    if (m_libs[m_parent[i - m_boardsize - 2]] <= 1) return false;
    
    return true;
}

void FastBoard::add_neighbour(const int i, const int color) {       
    assert(color == WHITE || color == BLACK || color == EMPTY);                                                          

    std::tr1::array<int, 4> nbr_pars;
    int nbr_par_cnt = 0;    

    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];

        m_neighbours[ai] += (1 << (NBR_SHIFT * color)) - (1 << (NBR_SHIFT * EMPTY));                               
        
        bool found = false;
        for (int i = 0; i < nbr_par_cnt; i++) {
            if (nbr_pars[i] == m_parent[ai]) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_libs[m_parent[ai]]--;
            nbr_pars[nbr_par_cnt++] = m_parent[ai];
        }        
    }
}

void FastBoard::remove_neighbour(const int i, const int color) {         
    assert(color == WHITE || color == BLACK || color == EMPTY);        
    
    std::tr1::array<int, 4> nbr_pars;
    int nbr_par_cnt = 0;    

    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
                           
        m_neighbours[ai] += (1 << (NBR_SHIFT * EMPTY))  
                          - (1 << (NBR_SHIFT * color));                                            
        
        bool found = false;
        for (int i = 0; i < nbr_par_cnt; i++) {
            if (nbr_pars[i] == m_parent[ai]) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_libs[m_parent[ai]]++;
            nbr_pars[nbr_par_cnt++] = m_parent[ai];
        }        
    }     
}

int FastBoard::remove_string_fast(int i) {
    int pos = i;
    int removed = 0;
    int color = m_square[i]; 

    assert(color == WHITE || color == BLACK || color == EMPTY);
  
    do {       
        assert(m_square[pos] == color);

        m_square[pos]  = EMPTY;              
        m_parent[pos]  = MAXSQ;            
        m_totalstones[color]--;    
        
        remove_neighbour(pos, color); 
        
        m_empty_idx[pos]     = m_empty_cnt;
        m_empty[m_empty_cnt] = pos;            
        m_empty_cnt++;
                        
        removed++;
        pos = m_next[pos];
    } while (pos != i);    

    return removed;
}

std::vector<bool> FastBoard::calc_reach_color(int col) {        
    std::vector<bool> bd(m_maxsq);
    std::vector<bool> last(m_maxsq);
        
    std::fill(bd.begin(), bd.end(), false);
    std::fill(last.begin(), last.end(), false);
        
    /* needs multi pass propagation, slow */
    do {
        last = bd;
        for (int i = 0; i < m_boardsize; i++) {
            for (int j = 0; j < m_boardsize; j++) {            
                int vertex = get_vertex(i, j);
                /* colored field, spread */
                if (m_square[vertex] == col) {
                    bd[vertex] = true;
                    for (int k = 0; k < 4; k++) {                    
                        if (m_square[vertex + m_dirs[k]] == EMPTY) {
                            bd[vertex + m_dirs[k]] = true;
                        }                        
                    }
                } else if (m_square[vertex] == EMPTY && bd[vertex]) {               
                    for (int k = 0; k < 4; k++) {                    
                        if (m_square[vertex + m_dirs[k]] == EMPTY) {
                            bd[vertex + m_dirs[k]] = true;
                        }                        
                    }
                }
            }                    
        }        
    } while (last != bd);  
    
    return bd;
}


// Needed for scoring passed out games not in MC playouts
float FastBoard::area_score(float komi) {
    
    std::vector<bool> white = calc_reach_color(WHITE);
    std::vector<bool> black = calc_reach_color(BLACK);

    float score = -komi;

    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize; j++) {  
            int vertex = get_vertex(i, j);
            
//            assert(!(white[vertex] && black[vertex]));
//            assert(!(white[vertex] && m_square[vertex] == BLACK));
//            assert(!(black[vertex] && m_square[vertex] == WHITE));
            
            if (white[vertex] && !black[vertex]) {
                score -= 1.0f;
            } else if (black[vertex] && !white[vertex]) {
                score += 1.0f;
            }
        }
    }        
    
    return score;
}   

int FastBoard::estimate_mc_score(float komi) {    
    int wsc, bsc;        

    bsc = m_totalstones[BLACK];       
    wsc = m_totalstones[WHITE];   

    return bsc-wsc-((int)komi)+1;
}

float FastBoard::final_mc_score(float komi) {    
    int wsc, bsc;        
    int maxempty = m_empty_cnt;
    
    bsc = m_totalstones[BLACK];       
    wsc = m_totalstones[WHITE];
        
    for (int v = 0; v < maxempty; v++) {  
        int i = m_empty[v];            
        
        assert(m_square[i] == EMPTY);
            
        int allblack = ((m_neighbours[i] >> (NBR_SHIFT * BLACK)) & 7) == 4;            
        int allwhite = ((m_neighbours[i] >> (NBR_SHIFT * WHITE)) & 7) == 4;                                    
        
        if (allwhite) {
            wsc++;
        } else if (allblack) {
            bsc++;
        }        
    }
         
    return (float)(bsc)-((float)(wsc)+komi);
}

FastBoard FastBoard::remove_dead() {
    FastBoard tmp = *this;
    
    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize;j++) {
            int vtx = get_vertex(i, j);
            float mcown = MCOwnerTable::get_MCO()->get_blackown(BLACK, vtx);
            
            if (m_square[vtx] == BLACK && mcown < 0.20f) {
                tmp.set_square(vtx, EMPTY);
            } else if (m_square[vtx] == WHITE && mcown > 0.80f) {
                tmp.set_square(vtx, EMPTY);
            }
        }
    }
    
    return tmp;
}

std::vector<int> FastBoard::influence(void) {
    return remove_dead().run_bouzy(5, 21);
}

std::vector<int> FastBoard::moyo(void) {        
    return remove_dead().run_bouzy(5, 10);
}

std::vector<int> FastBoard::area(void) {    
    return remove_dead().run_bouzy(4, 0);
}

std::vector<int> FastBoard::run_bouzy(int dilat, int eros) {
    int i;
    int d, e;
    int goodsec, badsec;
    std::vector<int> tmp(m_maxsq);
    std::vector<int> influence(m_maxsq);
    
    /* init stones */
    for (i = 0; i < m_maxsq; i++) {
        if (m_square[i] == BLACK) {
            influence[i] = 128;
        } else if (m_square[i] == WHITE) {
            influence[i] = -128;
        } else {
            influence[i] = 0;
        }
    }
    
    tmp = influence;
        
    for (d = 0; d < dilat; d++) {    
        for (i = 0; i < m_maxsq; i++) {
            if (get_square(i) == INVAL) continue;
            if (influence[i] >= 0) {
                goodsec = badsec = 0;
                for (int k = 0; k < 4; k++) {
                    int sq = i + m_dirs[k];
                    if (get_square(sq) != INVAL && !badsec) {
                        if      (influence[sq] > 0) goodsec++;
                        else if (influence[sq] < 0) badsec++;
                    }
                }
                if (!badsec) 
                    tmp[i] += goodsec;
             }
             if (influence[i] <= 0) {
                goodsec = badsec = 0;
                for (int k = 0; k < 4; k++) {
                    int sq = i + m_dirs[k];
                    if (get_square(sq) != INVAL && !badsec) {
                        if      (influence[sq] < 0) goodsec++;
                        else if (influence[sq] > 0) badsec++;
                    }
                }
                if (!badsec) 
                    tmp[i] -= goodsec;
            }
        }
        influence = tmp;
    }
    
    for (e = 0; e < eros; e++) {        
        for (i = 0; i < m_maxsq; i++) {
            if (get_square(i) == INVAL) continue;
            if (influence[i] > 0) {
                badsec = 0;
                for (int k = 0; k < 4; k++) {
                    int sq = i + m_dirs[k];
                    if (get_square(sq) != INVAL && !badsec) {
                        if (influence[sq] <= 0) badsec++;
                    }
                }                                                            
                tmp[i] -= badsec;
                if (tmp[i] < 0) tmp[i] = 0;
            } else if (influence[i] < 0) {
                badsec = 0;
                for (int k = 0; k < 4; k++) {
                    int sq = i + m_dirs[k];
                    if (get_square(sq) != INVAL && !badsec) {
                        if (influence[sq] >= 0) badsec++;
                    }
                }       
                tmp[i] += badsec;
                if (tmp[i] > 0) tmp[i] = 0;                    
            }
        }        
        influence = tmp;
    }
    
    return influence;        
}

void FastBoard::display_map(std::vector<int> influence) {
    int i, j;            
    
    for (j = m_boardsize-1; j >= 0; j--) {
        for (i = 0; i < m_boardsize; i++) {
            int infl = influence[get_vertex(i,j)];
            if (infl > 0) {
                if (get_square(i, j) ==  BLACK) {
                    myprintf("X ");
                } else if (get_square(i, j) == WHITE) {
                    myprintf("w ");
                } else {
                    myprintf("x ");
                }                               
            } else if (infl < 0) {
                if (get_square(i, j) ==  BLACK) {
                    myprintf("b ");
                } else if (get_square(i, j) == WHITE) {
                    myprintf("O ");
                } else {
                    myprintf("o ");
                }   
            } else {
                myprintf(". ");
            }            
        }
        myprintf("\n");
    }
}

int FastBoard::eval(float komi) {
    int tmp = 0;       
        
    /* 2/3 3/7 4/13 alternate: 5/10 moyo 4/0 area */    
    std::vector<int> influence = run_bouzy(5, 21);

    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize; j++) {
            int vertex = get_vertex(i, j);
            if (influence[vertex] < 0) {
                tmp--; 
	    } else if (influence[vertex] > 0) {
                tmp++;            
	    }
	}
    }		            
   
    if (m_tomove == WHITE) {	
        tmp -= (int)komi;			
    }	            
    
    if (m_tomove == WHITE) {
        tmp = -tmp;
    }        
   
    return tmp;
}

void FastBoard::display_board(int lastmove) {
    int boardsize = get_boardsize();     

    myprintf("\n   ");
    for (int i = 0; i < boardsize; i++) {
        if (i < 25) {
            myprintf("%c ", (('a' + i < 'i') ? 'a' + i : 'a' + i + 1));
        } else {
            myprintf("%c ", (('A' + (i-25) < 'I') ? 'A' + (i-25) : 'A' + (i-25) + 1));
        }
    }
    myprintf("\n");
    for (int j = boardsize-1; j >= 0; j--) {        
        myprintf("%2d", j+1);        
        if (lastmove == get_vertex(0, j))
            myprintf("(");        
        else
            myprintf(" ");
        for (int i = 0; i < boardsize; i++) {
            if (get_square(i,j) == WHITE) {
                myprintf("O");
            } else if (get_square(i,j) == BLACK)  {
                myprintf("X");
            } else if (starpoint(boardsize, i, j)) {
                myprintf("+");        
            } else {
                myprintf(".");    
            }            
            if (lastmove == get_vertex(i, j)) myprintf(")");
            else if (i != boardsize-1 && lastmove == get_vertex(i, j)+1) myprintf("(");            
            else myprintf(" ");
        }
        myprintf("%2d\n", j+1);
    }
    myprintf("   ");
    for (int i = 0; i < boardsize; i++) {
         if (i < 25) {
            myprintf("%c ", (('a' + i < 'i') ? 'a' + i : 'a' + i + 1));
        } else {
            myprintf("%c ", (('A' + (i-25) < 'I') ? 'A' + (i-25) : 'A' + (i-25) + 1));
        }
    }
    myprintf("\n\n");
}

void FastBoard::display_liberties(int lastmove) {
    int boardsize = get_boardsize();
           
    myprintf("   ");                        
    for (int i = 0; i < boardsize; i++) {
        myprintf("%c ", (('a' + i < 'i') ? 'a' + i : 'a' + i + 1));
    }
    myprintf("\n");
    for (int j = boardsize-1; j >= 0; j--) {        
        myprintf("%2d", j+1);        
        if (lastmove == get_vertex(0,j) )
            myprintf("(");        
        else
            myprintf(" ");
        for (int i = 0; i < boardsize; i++) {
            if (get_square(i,j) == WHITE) {
                int libs = m_libs[m_parent[get_vertex(i,j)]];
                if (libs > 9) { libs = 9; };
                myprintf("%1d", libs);                
            } else if (get_square(i,j) == BLACK)  {
                int libs = m_libs[m_parent[get_vertex(i,j)]];
                if (libs > 9) { libs = 9; };
                myprintf("%1d", libs);                
            } else if (starpoint(boardsize, i, j)) {
                myprintf("+");    
            } else {
                myprintf(".");    
            }            
            if (lastmove == get_vertex(i, j)) myprintf(")");
            else if (i != boardsize-1 && lastmove == get_vertex(i, j)+1) myprintf("(");            
            else myprintf(" ");
        }
        myprintf("%2d\n", j+1);
    }
    myprintf("\n\n");
    
    myprintf("   ");        
    for (int i = 0; i < boardsize; i++) {
        myprintf("%c ", (('a' + i < 'i') ? 'a' + i : 'a' + i + 1));
    }    
    myprintf("\n");
    for (int j = boardsize-1; j >= 0; j--) {        
        myprintf("%2d", j+1);        
        if (lastmove == get_vertex(0,j) )
            myprintf("(");        
        else
            myprintf(" ");
        for (int i = 0; i < boardsize; i++) {
            if (get_square(i,j) == WHITE) {
                int id = m_parent[get_vertex(i,j)]; 
                myprintf("%2d", id);                
            } else if (get_square(i,j) == BLACK)  {
                int id = m_parent[get_vertex(i,j)]; 
                myprintf("%2d", id);               
            } else if (starpoint(boardsize, i, j)) {
                myprintf("+ ");    
            } else {
                myprintf(". ");    
            }            
            if (lastmove == get_vertex(i, j)) myprintf(")");
            else if (i != boardsize-1 && lastmove == get_vertex(i, j)+1) myprintf("(");            
            else myprintf(" ");
        }
        myprintf("%2d\n", j+1);
    }    
    myprintf("\n\n");    
}

void FastBoard::merge_strings(const int ip, const int aip) {            
    assert(ip != MAXSQ && aip != MAXSQ);

    /* merge stones */    
    m_stones[ip] += m_stones[aip];
    
    /* loop over stones, update parents */           
    int newpos = aip;    

    do {       
        // check if this stone has a liberty        
        for (int k = 0; k < 4; k++) {
            int ai = newpos + m_dirs[k];
            // for each liberty, check if it is not shared        
            if (m_square[ai] == EMPTY) {                
                // find liberty neighbors                
                bool found = false;                
                for (int kk = 0; kk < 4; kk++) {
                    int aai = ai + m_dirs[kk];
                    // friendly string shouldn't be ip
                    // ip can also be an aip that has been marked                    
                    if (m_parent[aai] == ip) {
                        found = true;
                        break;
                    }                    
                }                                
                
                if (!found) {
                    m_libs[ip]++;
                }
            }
        }
        
        m_parent[newpos] = ip;         
        newpos = m_next[newpos];
    } while (newpos != aip);                    
        
    /* merge stings */
    int tmp = m_next[aip];
    m_next[aip] = m_next[ip];
    m_next[ip] = tmp;                
}

int FastBoard::update_board_eye(const int color, const int i) {             
    m_square[i]  = (square_t)color;    
    m_next[i]    = i;     
    m_parent[i]  = i;    
    m_libs[i]    = 0;
    m_stones[i]  = 1;
    m_totalstones[color]++;           
    
    add_neighbour(i, color); 

    int captured_sq;    
    int captured_stones = 0;

    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];

        assert(ai >= 0 && ai <= m_maxsq);
       
        if (m_libs[m_parent[ai]] <= 0) {
            int this_captured    = remove_string_fast(ai);
            captured_sq          = ai;    
            captured_stones     += this_captured;        
        }    
    }               

    /* move last vertex in list to our position */    
    int lastvertex               = m_empty[--m_empty_cnt];
    m_empty_idx[lastvertex]      = m_empty_idx[i];
    m_empty[m_empty_idx[i]]      = lastvertex;   
    
    m_prisoners[color] += captured_stones;
    
    // possibility of ko
    if (captured_stones == 1) {
        return captured_sq;
    }                   
    
    return -1;
}

/*    
    returns ko square or suicide tag
*/    
int FastBoard::update_board_fast(const int color, const int i) {                        
    assert(m_square[i] == EMPTY);    
    assert(color == WHITE || color == BLACK);
    
    /* did we play into an opponent eye? */    
    int eyeplay = (m_neighbours[i] & s_eyemask[!color]);  
       
    // because we check for single stone suicide, we know
    // its a capture, and it might be a ko capture         
    if (eyeplay) {          
        return update_board_eye(color, i);        
    }        
    
    m_square[i]  = (square_t)color;    
    m_next[i]    = i;     
    m_parent[i]  = i;
    m_libs[i]    = count_pliberties(i);       
    m_stones[i]  = 1;
    m_totalstones[color]++;                                    
        
    add_neighbour(i, color); 
        
    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
        
        if (m_square[ai] > WHITE) continue; 
        
        assert(ai >= 0 && ai <= m_maxsq);
                                       
        if (m_square[ai] == !color) {
            if (m_libs[m_parent[ai]] <= 0) {                
                m_prisoners[color] += remove_string_fast(ai);
            }
        } else if (m_square[ai] == color) {                                                    
            int ip  = m_parent[i];
            int aip = m_parent[ai];                    
            
            if (ip != aip) {
                if (m_stones[ip] >= m_stones[aip]) {                
                    merge_strings(ip, aip);                                                        
                } else {
                    merge_strings(aip, ip);                                                        
                }
            }
        }        
    }                   
    
    /* move last vertex in list to our position */    
    int lastvertex               = m_empty[--m_empty_cnt];
    m_empty_idx[lastvertex]      = m_empty_idx[i];
    m_empty[m_empty_idx[i]]      = lastvertex;                      
        
    assert(m_libs[m_parent[i]] >= 0);        

    /* check whether we still live (i.e. detect suicide) */    
    if (m_libs[m_parent[i]] == 0) {                                               
        remove_string_fast(i);                
    } 
        
    return -1;
}

bool FastBoard::is_eye(const int color, const int i) {
    /* check for 4 neighbors of the same color */
    int ownsurrounded = (m_neighbours[i] & s_eyemask[color]);
    
    // if not, it can't be an eye 
    // this takes advantage of borders being colored
    // both ways
    if (!ownsurrounded) {
        return false;
    }      
    
    // 2 or more diagonals taken
    // 1 for side groups                                
    int colorcount[4];
    
    colorcount[BLACK] = 0;
    colorcount[WHITE] = 0;
    colorcount[INVAL] = 0;

    colorcount[m_square[i - 1 - m_boardsize - 2]]++;
    colorcount[m_square[i + 1 - m_boardsize - 2]]++;
    colorcount[m_square[i - 1 + m_boardsize + 2]]++;
    colorcount[m_square[i + 1 + m_boardsize + 2]]++;

    if (colorcount[INVAL] == 0) {
        if (colorcount[!color] > 1) {
            return false;
        }
    } else {
        if (colorcount[!color]) {
            return false;
        }
    }                        
                               
    return true;    
}

// predict if we have 2 solid eyes after executing move
bool FastBoard::predict_solid_eye(const int move, const int color, const int i) {
    /* check for 4 neighbors of the same color */
    int ownsurrounded = count_neighbours(color, i);
        
    if (ownsurrounded < 3) {
        return false;
    } else if (ownsurrounded < 4 && move == PASS) {
        return false;
    }        
    
    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
        int sq = m_square[ai];
        if (sq != color && sq != INVAL && ai != move) {
            return false;
        }
    }
    
    // 2 or more diagonals taken
    // 1 for side groups                                
    int colorcount[4];
    
    colorcount[BLACK] = 0;
    colorcount[WHITE] = 0;
    colorcount[INVAL] = 0;

    colorcount[m_square[i - 1 - m_boardsize - 2]]++;
    colorcount[m_square[i + 1 - m_boardsize - 2]]++;
    colorcount[m_square[i - 1 + m_boardsize + 2]]++;
    colorcount[m_square[i + 1 + m_boardsize + 2]]++;

    // enemies are flaws    
    int flaws = colorcount[!color];     
                                                             
    // in addition to the above valid diagonals should be secure
    // this means they can't "just" be empty but must be 
    // ours-taken or empty-secure
    int pos;
    pos = i - 1 - m_boardsize - 2;    
    if (m_square[pos] == EMPTY && pos != move) {    
        if (count_neighbours(color, pos) < 4) {
            flaws++;
        }
    }
    pos = i + 1 - m_boardsize - 2;    
    if (m_square[pos] == EMPTY && pos != move) {    
        if (count_neighbours(color, pos) < 4) {
            flaws++;
        }
    }
    pos = i - 1 + m_boardsize + 2;    
    if (m_square[pos] == EMPTY && pos != move) {    
        if (count_neighbours(color, pos) < 4) {
            flaws++;
        }
    }
    pos = i + 1 + m_boardsize + 2;    
    if (m_square[pos] == EMPTY && pos != move) {    
        if (count_neighbours(color, pos) < 4) {
            flaws++;
        }
    }   
    
    if (colorcount[INVAL] == 0) {
        if (flaws > 1) {
            return false;
        }
    } else {
        if (flaws) {
            return false;
        }        
    }    
                               
    return true;    
}

bool FastBoard::no_eye_fill(const int i) {             
    return !is_eye(m_tomove, i);
}

std::string FastBoard::move_to_text(int move) {    
    std::ostringstream result;
    
    int column = move % (m_boardsize + 2);
    int row = move / (m_boardsize + 2);
    
    column--;
    row--;
    
    assert(move == FastBoard::PASS || move == FastBoard::RESIGN || (row >= 0 && row < m_boardsize));
    assert(move == FastBoard::PASS || move == FastBoard::RESIGN || (column >= 0 && column < m_boardsize));
    
    if (move >= 0 && move <= m_maxsq) {
        if (column <= 24) {
            result << static_cast<char>(column < 8 ? 'a' + column : 'a' + column + 1);
            result << (row + 1);        
        } else {            
            column -= 25;
            result << static_cast<char>(column < 8 ? 'A' + column : 'A' + column + 1);
            result << (row + 1);        
        }
    } else if (move == FastBoard::PASS) {
        result << "pass";
    } else if (move == FastBoard::RESIGN) {
	result << "resign";
    } else {
	result << "error";
    }
    	
    return result.str();
}

std::string FastBoard::move_to_text_sgf(int move) {    
    std::ostringstream result;
    
    int column = move % (m_boardsize + 2);
    int row = move / (m_boardsize + 2);
    
    column--;
    row--;
    
    assert(move == FastBoard::PASS || move == FastBoard::RESIGN || (row >= 0 && row < m_boardsize));
    assert(move == FastBoard::PASS || move == FastBoard::RESIGN || (column >= 0 && column < m_boardsize));

    // SGF inverts rows
    row = m_boardsize - row - 1;
    
    if (move >= 0 && move <= m_maxsq) {
        if (column <= 25) {
            result << static_cast<char>('a' + column);
        } else {            
            result << static_cast<char>('A' + column - 26);
        }
        if (row <= 25) {
            result << static_cast<char>('a' + row);        
        } else {
            result << static_cast<char>('A' + row - 26);        
        }
    } else if (move == FastBoard::PASS) {
        result << "tt";
    } else if (move == FastBoard::RESIGN) {
	result << "tt";
    } else {
	result << "error";
    }
    	
    return result.str();
}

int FastBoard::text_to_move(std::string move) {
    if (move.size() == 0 || move == "pass") {
        return FastBoard::PASS;
    }
    if (move == "resign") {
        return FastBoard::RESIGN;
    }

    char c1 = tolower(move[0]);
    int x = c1 - 'a';
    // There is no i in ...
    assert(x != 8);
    if (x > 8) x--;
    std::string remainder = move.substr(1);
    int y = std::stoi(remainder) - 1;

    int vtx = get_vertex(x, y);

    return vtx;
}

bool FastBoard::starpoint(int size, int point) {
    int stars[3];
    int points[2];
    int hits = 0;
    
    if (size % 2 == 0 || size < 9) {
        return false;
    }
    
    stars[0] = size >= 13 ? 3 : 2;
    stars[1] = size / 2;
    stars[2] = size - 1 - stars[0];
    
    points[0] = point / size;
    points[1] = point % size;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            if (points[i] == stars[j]) {
                hits++;
            }
        }
    }
    
    return hits >= 2;
}

bool FastBoard::starpoint(int size, int x, int y) {
    return starpoint(size, y * size + x);
}

int FastBoard::get_prisoners(int side) {
    assert(side == WHITE || side == BLACK);

    return m_prisoners[side];
}

bool FastBoard::black_to_move() {
    return m_tomove == BLACK;
}

int FastBoard::get_to_move() {
    return m_tomove;
}

void FastBoard::set_to_move(int tomove) {
    m_tomove = tomove;
}

int FastBoard::get_groupid(int vertex) {
    assert(m_square[vertex] == WHITE || m_square[vertex] == BLACK);
    assert(m_parent[vertex] == m_parent[m_parent[vertex]]);

    return m_parent[vertex];
}

std::vector<int> FastBoard::get_string_stones(int vertex) {
    int start = m_parent[vertex];

    std::vector<int> res;    
    res.reserve(m_stones[start]);    
    
    int newpos = start;
    
    do {       
        assert(m_square[newpos] == m_square[vertex]);
        res.push_back(newpos);
        newpos = m_next[newpos];
    } while (newpos != start);   
        
    return res;
}

std::string FastBoard::get_string(int vertex) {
    std::string result;
    
    int start = m_parent[vertex];
    int newpos = start;
    
    do {                           
        result += move_to_text(newpos) + " "; 
        newpos = m_next[newpos];
    } while (newpos != start);   
    
    // eat last space
    result.resize(result.size() - 1);
    
    return result;
}

bool FastBoard::fast_in_atari(int vertex) {        
//    assert(m_square[vertex] < EMPTY);
    
    int par = m_parent[vertex];
    int lib = m_libs[par];
    
    return lib == 1;       
}

// check if string is in atari, returns 0 if not,
// single liberty if it is
int FastBoard::in_atari(int vertex) {        
    if (m_libs[m_parent[vertex]] > 1) {
        return false;
    }

    int pos = vertex;            
    
    assert(m_square[vertex] < EMPTY);
  
    do {                               
        if (count_pliberties(pos)) {                    
            for (int k = 0; k < 4; k++) {
                int ai = pos + m_dirs[k];                                                
                if (m_square[ai] == EMPTY) { 
                    return ai;                                                      
                }
            }                    
        }
        
        pos = m_next[pos];
    } while (pos != vertex);    
    
    assert(false);
    
    return false;
}

// loop over a string and try to kill neighbors
void FastBoard::kill_neighbours(int vertex, movelist_t & moves, size_t & movecnt) {
    int scolor = m_square[vertex];            
    int kcolor = !scolor;
    int pos = vertex;

    std::tr1::array<int, 4> nbr_list;
    int nbr_cnt = 0;
          
    do {                               
        assert(m_square[pos] == scolor);
            
        for (int k = 0; k < 4; k++) {
            int ai = pos + m_dirs[k];                                
            
            if (m_square[ai] == kcolor) { 
                int par = m_parent[ai];
                int lib = m_libs[par];
                
                if (lib <= 1 && nbr_cnt < 4) {				
		    bool found = false;
		    for (int i = 0; i < nbr_cnt; i++) {
			    if (nbr_list[i] == par) {
				    found = true;
			    }
		    }
		    if (!found) {
			    int atari = in_atari(ai);                                        
			    assert(m_square[atari] == EMPTY);
			    moves[movecnt++] = atari;                   
			    nbr_list[nbr_cnt++] = par;
		    }
                }
            }
        }                        
        
        pos = m_next[pos];
    } while (pos != vertex);                                                    
}                                         

int FastBoard::saving_size(int color, int vertex) {                        
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        
        if (m_square[ai] == color) {        
            int par = m_parent[ai];
            int lib = m_libs[par];
            
            if (lib <= 1) {                
                int atari = in_atari(ai);
                                
                if (!self_atari(color, atari)) {
                    return string_size(ai);  
                }                                                         
            }
        } 
    }
    
    return 0;        
}

// look for a neighbors of vertex with "color" that are critical,
// and add moves that save them to work
// vertex is sure to be filled with !color
void FastBoard::save_critical_neighbours(int color, int vertex, movelist_t & moves, size_t & movecnt) {
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        
        if (m_square[ai] == color) {        
            int par = m_parent[ai];
            int lib = m_libs[par];
            
            if (lib <= 1) {                
                int atari = in_atari(ai);
                                
                size_t startsize = movecnt;
                                
                // find saving moves for atari square "atari"                
                // we can save by either
                // 1) playing in the atari if it increases liberties,
                //    i.e. it is not self-atari 
                // 2) capturing an opponent, which means that he should
                //    also be in atari                                                        
                if (!self_atari(color, atari)) {                        
                    moves[movecnt++] = atari; 
                } 
                
                kill_neighbours(ai, moves, movecnt);     
                
                // saving moves failed, add this to critical points
                if (movecnt == startsize) {                    
                    m_critical.push_back(atari);
                }            
            }
        } 
    }        
}

int FastBoard::get_dir(int i) {
    return m_dirs[i];
}

int FastBoard::get_extra_dir(int i) {
    return m_extradirs[i];
}

bool FastBoard::kill_or_connect(int color, int vertex) {                        
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];        
        int sq = get_square(ai);                              
        int libs = m_libs[m_parent[ai]];
        
        if ((libs <= 1 && sq == !color) || (libs >= 3 && sq == color)) {
            return true;
        }              
    }          
    
    return false;       
}

template <int N> 
void FastBoard::add_string_liberties(int vertex, 
                                     std::tr1::array<int, N> & nbr_libs, 
                                     int & nbr_libs_cnt) {
    int pos = vertex;
#ifndef NDEBUG
    int color = m_square[pos];
#endif

    do {
        assert(m_square[pos] == color);

        if (count_pliberties(pos)) {        
            // look for empties near this stone
            for (int k = 0; k < 4; k++) {
                int ai = pos + m_dirs[k];                                
                
                if (m_square[ai] == EMPTY) {                                
                    bool found = false;
                    
                    for (int i = 0; i < nbr_libs_cnt; i++) {
                        if (nbr_libs[i] == ai) {
                            found = true;
                            break;
                        }
                    }
                                        
                    // not in list yet, so add
                    if (!found) {
                        nbr_libs[nbr_libs_cnt++] = ai; 
                        
                        // more than 2 liberties means we are not critical
                        if (nbr_libs_cnt >= N) {
                            return;
                        }  
                    }
                }
            }                    
        }
        
        pos = m_next[pos];
    } while (pos != vertex);    
}

// check whether this move is a self-atari
bool FastBoard::self_atari(int color, int vertex) {    
    assert(get_square(vertex) == FastBoard::EMPTY);
    
    // 1) count new liberties, if we add 2 or more we're safe                
    if (count_pliberties(vertex) >= 2) {        
        return false;                
    }
    
    // 2) if we kill an enemy, or connect to safety, we're good 
    // as well    
    if (kill_or_connect(color, vertex)) {        
        return false;
    }
    
    // any neighbor by itself has at most 2 liberties now,
    // and we can have at most one empty neighbor
    // 3) if we don't connect at all, we're dead
    if (count_neighbours(color, vertex) == 0) {
        return true;
    }
    
    // 4) we only add at most 1 liberty, and we removed 1, so check if 
    // the sum of friendly neighbors had 2 or less that might have 
    // become one (or less, in which case this is multi stone suicide)
    
    // list of all liberties, this never gets big             
    std::tr1::array<int, 3> nbr_libs;
    int nbr_libs_cnt = 0;
    
    // add the vertex we play in to the liberties list
    nbr_libs[nbr_libs_cnt++] = vertex;           
    
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        
        if (get_square(ai) == FastBoard::EMPTY) {
            bool found = false;
                    
            for (int i = 0; i < nbr_libs_cnt; i++) {
                if (nbr_libs[i] == ai) {
                    found = true;
                    break;
                }
            }
                                        
            // not in list yet, so add
            if (!found) {
                if (nbr_libs_cnt > 1) return false;
                nbr_libs[nbr_libs_cnt++] = ai;
            }                
        } else if (get_square(ai) == color) {        
            int par = m_parent[ai];
            int lib = m_libs[par];
            
            // we already know this neighbor does not have a large 
            // number of liberties, and to contribute, he must have
            // more liberties than just the one that is "vertex"
            if (lib > 1) {
                add_string_liberties<3>(ai, nbr_libs, nbr_libs_cnt);
                if (nbr_libs_cnt > 2) {            
                    return false;
                }
            }            
        }                
    }

    // if we get here, there are no more than 2 liberties,
    // and we just removed 1 of those (since we added the play square
    // to the list), so it must be an auto-atari    
    return true;
}

int FastBoard::get_pattern_fast(const int sq) {
    const int size = m_boardsize;
    
    return (m_square[sq - size - 2 - 1] << 14)
         | (m_square[sq - size - 2]     << 12)
         | (m_square[sq - size - 2 + 1] << 10)
         | (m_square[sq - 1]            <<  8)
         | (m_square[sq + 1]            <<  6)
         | (m_square[sq + size + 2 - 1] <<  4)
         | (m_square[sq + size + 2]     <<  2)
         | (m_square[sq + size + 2 + 1] <<  0);   
}

int FastBoard::get_pattern_fast_augment(const int sq) {
    const int size = m_boardsize;
    int sqs0, sqs1, sqs2, sqs3, sqs4, sqs5, sqs6, sqs7;
    int lib0, lib1, lib2, lib3;
    int res;
    
    sqs0 = m_square[sq - size - 2 - 1];
    sqs1 = m_square[sq - size - 2];
    sqs2 = m_square[sq - size - 2 + 1];
    sqs3 = m_square[sq - 1];
    sqs4 = m_square[sq + 1];
    sqs5 = m_square[sq + size + 2 - 1];
    sqs6 = m_square[sq + size + 2];
    sqs7 = m_square[sq + size + 2 + 1];
    
    res =  (sqs0 << 14)
         | (sqs1 << 12)
         | (sqs2 << 10)
         | (sqs3 <<  8)
         | (sqs4 <<  6)
         | (sqs5 <<  4)
         | (sqs6 <<  2)
         | (sqs7 <<  0);            
    
    lib0 = fast_in_atari(sq - size - 2);    
    lib1 = fast_in_atari(sq - 1);    
    lib2 = fast_in_atari(sq + 1);    
    lib3 = fast_in_atari(sq + size + 2);
    
    res |= (lib0 << 19 | lib1 << 18 | lib2 << 17 | lib3 << 16);
    
    return res;
}

int FastBoard::get_pattern3(const int sq, bool invert) {
    int sqs0, sqs1, sqs2, sqs3, sqs4, sqs5, sqs6, sqs7;
    const int size = m_boardsize;
    
    sqs0 = m_square[sq - size - 2 - 1];
    sqs1 = m_square[sq - size - 2];
    sqs2 = m_square[sq - size - 2 + 1];
    sqs3 = m_square[sq - 1];
    sqs4 = m_square[sq + 1];
    sqs5 = m_square[sq + size + 2 - 1];
    sqs6 = m_square[sq + size + 2];
    sqs7 = m_square[sq + size + 2 + 1];
    
    /* color symmetry */
    if (invert) {
        sqs0 = s_cinvert[sqs0];
        sqs1 = s_cinvert[sqs1];
        sqs2 = s_cinvert[sqs2];
        sqs3 = s_cinvert[sqs3];
        sqs4 = s_cinvert[sqs4];
        sqs5 = s_cinvert[sqs5];
        sqs6 = s_cinvert[sqs6];
        sqs7 = s_cinvert[sqs7];    
    }  
    
    /*
        012
        3 4
        567
    */            
    int idx1 = (sqs0 << 14) | (sqs1 << 12) | (sqs2 << 10) | (sqs3 <<  8)
             | (sqs4 <<  6) | (sqs5 <<  4) | (sqs6 <<  2) | (sqs7 <<  0);

    int idx2 = (sqs5 << 14) | (sqs3 << 12) | (sqs0 << 10) | (sqs6 <<  8)
             | (sqs1 <<  6) | (sqs7 <<  4) | (sqs4 <<  2) | (sqs2 <<  0);
             
    int idx3 = (sqs7 << 14) | (sqs6 << 12) | (sqs5 << 10) | (sqs4 <<  8)
             | (sqs3 <<  6) | (sqs2 <<  4) | (sqs1 <<  2) | (sqs0 <<  0);
             
    int idx4 = (sqs2 << 14) | (sqs4 << 12) | (sqs7 << 10) | (sqs1 <<  8)
             | (sqs6 <<  6) | (sqs0 <<  4) | (sqs3 <<  2) | (sqs5 <<  0);
    /*
        035
        1 6
        247
    */                  
    int idx5 = (sqs0 << 14) | (sqs3 << 12) | (sqs5 << 10) | (sqs1 <<  8)
             | (sqs6 <<  6) | (sqs2 <<  4) | (sqs4 <<  2) | (sqs7 <<  0);
             
    int idx6 = (sqs2 << 14) | (sqs1 << 12) | (sqs0 << 10) | (sqs4 <<  8)
             | (sqs3 <<  6) | (sqs7 <<  4) | (sqs6 <<  2) | (sqs5 <<  0);

    int idx7 = (sqs7 << 14) | (sqs4 << 12) | (sqs2 << 10) | (sqs6 <<  8)
             | (sqs1 <<  6) | (sqs5 <<  4) | (sqs3 <<  2) | (sqs0 <<  0);
             
    int idx8 = (sqs5 << 14) | (sqs6 << 12) | (sqs7 << 10) | (sqs3 <<  8)
             | (sqs4 <<  6) | (sqs0 <<  4) | (sqs1 <<  2) | (sqs2 <<  0);
             
    idx1 = std::min(idx1, idx2);
    idx3 = std::min(idx3, idx4);
    idx5 = std::min(idx5, idx6);
    idx7 = std::min(idx7, idx8);
    
    idx1 = std::min(idx1, idx3);
    idx5 = std::min(idx5, idx7);
    
    idx1 = std::min(idx1, idx5);                  
          
    return idx1;                   
}

int FastBoard::get_pattern3_augment(const int sq, bool invert) {
    int sqs0, sqs1, sqs2, sqs3, sqs4, sqs5, sqs6, sqs7;
    int lib0, lib1, lib2, lib3;
    const int size = m_boardsize;        
    
    sqs0 = m_square[sq - size - 2 - 1];
    sqs1 = m_square[sq - size - 2];
    sqs2 = m_square[sq - size - 2 + 1];
    sqs3 = m_square[sq - 1];
    sqs4 = m_square[sq + 1];
    sqs5 = m_square[sq + size + 2 - 1];
    sqs6 = m_square[sq + size + 2];
    sqs7 = m_square[sq + size + 2 + 1];
    if (sqs1 == WHITE || sqs1 == BLACK) {
        lib0 = in_atari(sq - size - 2) != 0;
    } else {
        lib0 = 0;
    }
    if (sqs3 == WHITE || sqs3 == BLACK) {
        lib1 = in_atari(sq - 1) != 0;
    } else {
        lib1 = 0;
    }
    if (sqs4 == WHITE || sqs4 == BLACK) {
        lib2 = in_atari(sq + 1) != 0;
    } else {
        lib2 = 0;
    }
    if (sqs6 == WHITE || sqs6 == BLACK) {
        lib3 = in_atari(sq + size + 2) != 0;
    } else {
        lib3 = 0;
    }
    
    /* color symmetry */
    if (invert) {        
        sqs0 = s_cinvert[sqs0];
        sqs1 = s_cinvert[sqs1];
        sqs2 = s_cinvert[sqs2];
        sqs3 = s_cinvert[sqs3];
        sqs4 = s_cinvert[sqs4];
        sqs5 = s_cinvert[sqs5];
        sqs6 = s_cinvert[sqs6];
        sqs7 = s_cinvert[sqs7];        
    }  
    
    /*
        012       0
        3 4      1 2
        567       3
    */            
    int idx1 = (sqs0 << 14) | (sqs1 << 12) | (sqs2 << 10) | (sqs3 <<  8)
             | (sqs4 <<  6) | (sqs5 <<  4) | (sqs6 <<  2) | (sqs7 <<  0);
    idx1 |= (lib0 << 19 | lib1 << 18 | lib2 << 17 | lib3 << 16);             

    int idx2 = (sqs5 << 14) | (sqs3 << 12) | (sqs0 << 10) | (sqs6 <<  8)
             | (sqs1 <<  6) | (sqs7 <<  4) | (sqs4 <<  2) | (sqs2 <<  0);
    idx2 |= (lib1 << 19 | lib3 << 18 | lib0 << 17 | lib2 << 16);               
             
    int idx3 = (sqs7 << 14) | (sqs6 << 12) | (sqs5 << 10) | (sqs4 <<  8)
             | (sqs3 <<  6) | (sqs2 <<  4) | (sqs1 <<  2) | (sqs0 <<  0);
    idx3 |= (lib3 << 19 | lib2 << 18 | lib1 << 17 | lib0 << 16);               
             
    int idx4 = (sqs2 << 14) | (sqs4 << 12) | (sqs7 << 10) | (sqs1 <<  8)
             | (sqs6 <<  6) | (sqs0 <<  4) | (sqs3 <<  2) | (sqs5 <<  0);
    idx4 |= (lib2 << 19 | lib0 << 18 | lib3 << 17 | lib1 << 16);               
    
    /*
        035    1
        1 6   0 3
        247    2
    */                  
    int idx5 = (sqs0 << 14) | (sqs3 << 12) | (sqs5 << 10) | (sqs1 <<  8)
             | (sqs6 <<  6) | (sqs2 <<  4) | (sqs4 <<  2) | (sqs7 <<  0);
    idx5 |= (lib1 << 19 | lib0 << 18 | lib3 << 17 | lib2 << 16);               
             
    int idx6 = (sqs2 << 14) | (sqs1 << 12) | (sqs0 << 10) | (sqs4 <<  8)
             | (sqs3 <<  6) | (sqs7 <<  4) | (sqs6 <<  2) | (sqs5 <<  0);
    idx6 |= (lib0 << 19 | lib2 << 18 | lib1 << 17 | lib3 << 16);               

    int idx7 = (sqs7 << 14) | (sqs4 << 12) | (sqs2 << 10) | (sqs6 <<  8)
             | (sqs1 <<  6) | (sqs5 <<  4) | (sqs3 <<  2) | (sqs0 <<  0);
    idx7 |= (lib2 << 19 | lib3 << 18 | lib0 << 17 | lib1 << 16);               
             
    int idx8 = (sqs5 << 14) | (sqs6 << 12) | (sqs7 << 10) | (sqs3 <<  8)
             | (sqs4 <<  6) | (sqs0 <<  4) | (sqs1 <<  2) | (sqs2 <<  0);
    idx8 |= (lib3 << 19 | lib1 << 18 | lib2 << 17 | lib0 << 16);               
             
    idx1 = std::min(idx1, idx2);
    idx3 = std::min(idx3, idx4);
    idx5 = std::min(idx5, idx6);
    idx7 = std::min(idx7, idx8);
    
    idx1 = std::min(idx1, idx3);
    idx5 = std::min(idx5, idx7);
    
    idx1 = std::min(idx1, idx5);                  
          
    return idx1;                   
}

int FastBoard::get_pattern3_augment_spec(const int sq, int libspec, bool invert) {
    int sqs0, sqs1, sqs2, sqs3, sqs4, sqs5, sqs6, sqs7;
    int lib0, lib1, lib2, lib3;
    const int size = m_boardsize;        
    
    sqs0 = m_square[sq - size - 2 - 1];
    sqs1 = m_square[sq - size - 2];
    sqs2 = m_square[sq - size - 2 + 1];
    sqs3 = m_square[sq - 1];
    sqs4 = m_square[sq + 1];
    sqs5 = m_square[sq + size + 2 - 1];
    sqs6 = m_square[sq + size + 2];
    sqs7 = m_square[sq + size + 2 + 1];
    
    lib3 = libspec & 1;
    libspec >>= 1;
    lib2 = libspec & 1;
    libspec >>= 1;
    lib1 = libspec & 1;   
    libspec >>= 1; 
    lib0 = libspec & 1;
    
    /* color symmetry */
    if (invert) {
        sqs0 = s_cinvert[sqs0];
        sqs1 = s_cinvert[sqs1];
        sqs2 = s_cinvert[sqs2];
        sqs3 = s_cinvert[sqs3];
        sqs4 = s_cinvert[sqs4];
        sqs5 = s_cinvert[sqs5];
        sqs6 = s_cinvert[sqs6];
        sqs7 = s_cinvert[sqs7];     
    }  
    
    /*
        012       0
        3 4      1 2
        567       3
    */            
    int idx1 = (sqs0 << 14) | (sqs1 << 12) | (sqs2 << 10) | (sqs3 <<  8)
             | (sqs4 <<  6) | (sqs5 <<  4) | (sqs6 <<  2) | (sqs7 <<  0);
    idx1 |= (lib0 << 19 | lib1 << 18 | lib2 << 17 | lib3 << 16);             

    int idx2 = (sqs5 << 14) | (sqs3 << 12) | (sqs0 << 10) | (sqs6 <<  8)
             | (sqs1 <<  6) | (sqs7 <<  4) | (sqs4 <<  2) | (sqs2 <<  0);
    idx2 |= (lib1 << 19 | lib3 << 18 | lib0 << 17 | lib2 << 16);               
             
    int idx3 = (sqs7 << 14) | (sqs6 << 12) | (sqs5 << 10) | (sqs4 <<  8)
             | (sqs3 <<  6) | (sqs2 <<  4) | (sqs1 <<  2) | (sqs0 <<  0);
    idx3 |= (lib3 << 19 | lib2 << 18 | lib1 << 17 | lib0 << 16);               
             
    int idx4 = (sqs2 << 14) | (sqs4 << 12) | (sqs7 << 10) | (sqs1 <<  8)
             | (sqs6 <<  6) | (sqs0 <<  4) | (sqs3 <<  2) | (sqs5 <<  0);
    idx4 |= (lib2 << 19 | lib0 << 18 | lib3 << 17 | lib1 << 16);               
    
    /*
        035    1
        1 6   0 3
        247    2
    */                  
    int idx5 = (sqs0 << 14) | (sqs3 << 12) | (sqs5 << 10) | (sqs1 <<  8)
             | (sqs6 <<  6) | (sqs2 <<  4) | (sqs4 <<  2) | (sqs7 <<  0);
    idx5 |= (lib1 << 19 | lib0 << 18 | lib3 << 17 | lib2 << 16);               
             
    int idx6 = (sqs2 << 14) | (sqs1 << 12) | (sqs0 << 10) | (sqs4 <<  8)
             | (sqs3 <<  6) | (sqs7 <<  4) | (sqs6 <<  2) | (sqs5 <<  0);
    idx6 |= (lib0 << 19 | lib2 << 18 | lib1 << 17 | lib3 << 16);               

    int idx7 = (sqs7 << 14) | (sqs4 << 12) | (sqs2 << 10) | (sqs6 <<  8)
             | (sqs1 <<  6) | (sqs5 <<  4) | (sqs3 <<  2) | (sqs0 <<  0);
    idx7 |= (lib2 << 19 | lib3 << 18 | lib0 << 17 | lib1 << 16);               
             
    int idx8 = (sqs5 << 14) | (sqs6 << 12) | (sqs7 << 10) | (sqs3 <<  8)
             | (sqs4 <<  6) | (sqs0 <<  4) | (sqs1 <<  2) | (sqs2 <<  0);
    idx8 |= (lib3 << 19 | lib1 << 18 | lib2 << 17 | lib0 << 16);               
             
    idx1 = std::min(idx1, idx2);
    idx3 = std::min(idx3, idx4);
    idx5 = std::min(idx5, idx6);
    idx7 = std::min(idx7, idx8);
    
    idx1 = std::min(idx1, idx3);
    idx5 = std::min(idx5, idx7);
    
    idx1 = std::min(idx1, idx5);                  
          
    return idx1;                   
}

// invert = invert colors because white is to move
// extend = fill in 4 most extended squares with inval
int FastBoard::get_pattern4(const int sq, bool invert) {          
    const int size = m_boardsize;
    std::tr1::array<square_t, 12> sqs;        
        
    sqs[1]  = m_square[sq - (size + 2) - 1];
    sqs[2]  = m_square[sq - (size + 2)];
    sqs[3]  = m_square[sq - (size + 2) + 1];
    
    sqs[5]  = m_square[sq - 1];
    sqs[6]  = m_square[sq + 1];
    
    sqs[8]  = m_square[sq + (size + 2) - 1];
    sqs[9]  = m_square[sq + (size + 2)];
    sqs[10] = m_square[sq + (size + 2) + 1];    
    
    if (sqs[2] == INVAL) {
        sqs[0] = INVAL;
    } else {
        sqs[0] = m_square[sq - 2*(size + 2)];
    }
    
    if (sqs[5] == INVAL) {
        sqs[4] = INVAL;
    } else {
        sqs[4] = m_square[sq - 2];
    }       
    
    if (sqs[6] == INVAL) {
        sqs[7] = INVAL;
    } else {
        sqs[7] = m_square[sq + 2];
    }
    
    if (sqs[9] == INVAL) {
        sqs[11] = INVAL;
    } else {
        sqs[11] = m_square[sq + 2*(size + 2)];
    } 
    
    /* color symmetry */
    if (invert) {
        for (size_t i = 0; i < sqs.size(); i++) {
            sqs[i] = s_cinvert[sqs[i]];
        }
    }
  
    /*  
          0        4        b   
         123      851      a98  
        45 67    b9 20    76 54 
         89a      a63      321  
          b        7        0   
    */    
    int idx1, idx2, idx3, idx4, idx5, idx6, idx7, idx8;
                                            
    idx1 =  (sqs[ 0] << 22) | (sqs[ 1] << 20) | (sqs[ 2] << 18) | (sqs[ 3] << 16)
          | (sqs[ 4] << 14) | (sqs[ 5] << 12) | (sqs[ 6] << 10) | (sqs[ 7] <<  8)
          | (sqs[ 8] <<  6) | (sqs[ 9] <<  4) | (sqs[10] <<  2) | (sqs[11] <<  0);                                          

    idx2 =  (sqs[ 4] << 22) | (sqs[ 8] << 20) | (sqs[ 5] << 18) | (sqs[ 1] << 16)
          | (sqs[11] << 14) | (sqs[ 9] << 12) | (sqs[ 2] << 10) | (sqs[ 0] <<  8)
          | (sqs[10] <<  6) | (sqs[ 6] <<  4) | (sqs[ 3] <<  2) | (sqs[ 7] <<  0);
         
    idx3 =  (sqs[11] << 22) | (sqs[10] << 20) | (sqs[ 9] << 18) | (sqs[ 8] << 16)
          | (sqs[ 7] << 14) | (sqs[ 6] << 12) | (sqs[ 5] << 10) | (sqs[ 4] <<  8)
          | (sqs[ 3] <<  6) | (sqs[ 2] <<  4) | (sqs[ 1] <<  2) | (sqs[ 0] <<  0);         
 
    idx4 =  (sqs[ 7] << 22) | (sqs[ 3] << 20) | (sqs[ 6] << 18) | (sqs[10] << 16)
          | (sqs[ 0] << 14) | (sqs[ 2] << 12) | (sqs[ 9] << 10) | (sqs[11] <<  8)
          | (sqs[ 1] <<  6) | (sqs[ 5] <<  4) | (sqs[ 8] <<  2) | (sqs[ 4] <<  0);            
    /*          
          4
         158 
        02 9b
         36a
          7
    */          

    idx5 =  (sqs[ 4] << 22) | (sqs[ 1] << 20) | (sqs[ 5] << 18) | (sqs[ 8] << 16)
          | (sqs[ 0] << 14) | (sqs[ 2] << 12) | (sqs[ 9] << 10) | (sqs[11] <<  8)
          | (sqs[ 3] <<  6) | (sqs[ 6] <<  4) | (sqs[10] <<  2) | (sqs[ 7] <<  0);            

    idx6 =  (sqs[ 0] << 22) | (sqs[ 3] << 20) | (sqs[ 2] << 18) | (sqs[ 1] << 16)
          | (sqs[ 7] << 14) | (sqs[ 6] << 12) | (sqs[ 5] << 10) | (sqs[ 4] <<  8)
          | (sqs[10] <<  6) | (sqs[ 9] <<  4) | (sqs[ 8] <<  2) | (sqs[11] <<  0);            

    idx7 =  (sqs[ 7] << 22) | (sqs[10] << 20) | (sqs[ 6] << 18) | (sqs[ 3] << 16)
          | (sqs[11] << 14) | (sqs[ 9] << 12) | (sqs[ 2] << 10) | (sqs[ 0] <<  8)
          | (sqs[ 8] <<  6) | (sqs[ 5] <<  4) | (sqs[ 1] <<  2) | (sqs[ 4] <<  0);            
          
    idx8 =  (sqs[11] << 22) | (sqs[ 8] << 20) | (sqs[ 9] << 18) | (sqs[10] << 16)
          | (sqs[ 4] << 14) | (sqs[ 5] << 12) | (sqs[ 6] << 10) | (sqs[ 7] <<  8)
          | (sqs[ 1] <<  6) | (sqs[ 2] <<  4) | (sqs[ 3] <<  2) | (sqs[ 0] <<  0);    
          
    idx1 = std::min(idx1, idx2);
    idx3 = std::min(idx3, idx4);
    idx5 = std::min(idx5, idx6);
    idx7 = std::min(idx7, idx8);
    
    idx1 = std::min(idx1, idx3);
    idx5 = std::min(idx5, idx7);
    
    idx1 = std::min(idx1, idx5);                  
          
    return idx1;          
}

// invert = invert colors because white is to move
// extend = fill in most extended squares with inval
uint64 FastBoard::get_pattern5(const int sq, bool invert, bool extend) {          
    const int size = m_boardsize;
    std::tr1::array<uint64, 20> sqs;
    
    /*
     XXX        012
    XXXXX      34567
    XX XX      89 ab
    XXXXX      cdefg
     XXX        hij
    */
    
    if (extend) {
        sqs[ 0] = INVAL;
        sqs[ 1] = INVAL;
        sqs[ 2] = INVAL;        
        sqs[ 3] = INVAL;
        sqs[ 7] = INVAL;
        sqs[ 8] = INVAL;
        sqs[11] = INVAL;
        sqs[12] = INVAL;
        sqs[16] = INVAL;
        sqs[17] = INVAL;
        sqs[18] = INVAL;
        sqs[19] = INVAL;        
    } else {
        sqs[ 0] = m_square[sq - 2*(size + 2) - 1];
        sqs[ 1] = m_square[sq - 2*(size + 2)];
        sqs[ 2] = m_square[sq - 2*(size + 2) + 1];
        
        sqs[ 3] = m_square[sq - (size + 2) - 2];
        sqs[ 7] = m_square[sq - (size + 2) + 2];
        
        sqs[ 8] = m_square[sq - 2];
        sqs[11] = m_square[sq + 2];
        
        sqs[12] = m_square[sq + (size + 2) - 2];
        sqs[16] = m_square[sq + (size + 2) + 2];
        
        sqs[17] = m_square[sq + 2*(size + 2) - 1];
        sqs[18] = m_square[sq + 2*(size + 2)];
        sqs[19] = m_square[sq + 2*(size + 2) + 1];
    }
        
    sqs[ 4] = m_square[sq - (size + 2) - 1];
    sqs[ 5] = m_square[sq - (size + 2)];
    sqs[ 6] = m_square[sq - (size + 2) + 1];
    
    sqs[ 9] = m_square[sq - 1];
    sqs[10] = m_square[sq + 1];
    
    sqs[13] = m_square[sq + (size + 2) - 1];
    sqs[14] = m_square[sq + (size + 2)];
    sqs[15] = m_square[sq + (size + 2) + 1];
    
    
    /* color symmetry */
    if (invert) {
        for (size_t i = 0; i < sqs.size(); i++) {
            sqs[i] = s_cinvert[(square_t)sqs[i]];
        }
    }
  
    /*  
        012     a = 10  b = 11 c = 12 d = 13 e = 14 f = 15 g = 16
       34567    h = 17  i = 18 j = 19
       89 ab    
       cdefg    
        hij      
    */    
    uint64 idx1, idx2, idx3, idx4, idx5, idx6, idx7, idx8;
                                            
    idx1 =  (sqs[ 0] << 38) | (sqs[ 1] << 36) | (sqs[ 2] << 34) | (sqs[ 3] << 32)
          | (sqs[ 4] << 30) | (sqs[ 5] << 28) | (sqs[ 6] << 26) | (sqs[ 7] << 24)
          | (sqs[ 8] << 22) | (sqs[ 9] << 20) | (sqs[10] << 18) | (sqs[11] << 16) 
          | (sqs[12] << 14) | (sqs[13] << 12) | (sqs[14] << 10) | (sqs[15] <<  8) 
          | (sqs[16] <<  6) | (sqs[17] <<  4) | (sqs[18] <<  2) | (sqs[19] <<  0); 
                                                   
    idx2 =  (sqs[12] << 38) | (sqs[ 8] << 36) | (sqs[ 3] << 34) | (sqs[17] << 32)
          | (sqs[13] << 30) | (sqs[ 9] << 28) | (sqs[ 4] << 26) | (sqs[ 0] << 24)
          | (sqs[18] << 22) | (sqs[14] << 20) | (sqs[ 5] << 18) | (sqs[ 1] << 16) 
          | (sqs[19] << 14) | (sqs[15] << 12) | (sqs[10] << 10) | (sqs[ 6] <<  8) 
          | (sqs[ 2] <<  6) | (sqs[16] <<  4) | (sqs[11] <<  2) | (sqs[ 7] <<  0); 

    idx3 =  (sqs[19] << 38) | (sqs[18] << 36) | (sqs[17] << 34) | (sqs[16] << 32)
          | (sqs[15] << 30) | (sqs[14] << 28) | (sqs[13] << 26) | (sqs[12] << 24)
          | (sqs[11] << 22) | (sqs[10] << 20) | (sqs[ 9] << 18) | (sqs[ 8] << 16) 
          | (sqs[ 7] << 14) | (sqs[ 6] << 12) | (sqs[ 5] << 10) | (sqs[ 4] <<  8) 
          | (sqs[ 3] <<  6) | (sqs[ 2] <<  4) | (sqs[ 1] <<  2) | (sqs[ 0] <<  0); 
          
    idx4 =  (sqs[ 7] << 38) | (sqs[11] << 36) | (sqs[16] << 34) | (sqs[ 2] << 32)
          | (sqs[ 6] << 30) | (sqs[10] << 28) | (sqs[15] << 26) | (sqs[19] << 24)
          | (sqs[ 1] << 22) | (sqs[ 5] << 20) | (sqs[14] << 18) | (sqs[18] << 16) 
          | (sqs[ 0] << 14) | (sqs[ 4] << 12) | (sqs[ 9] << 10) | (sqs[13] <<  8) 
          | (sqs[17] <<  6) | (sqs[ 3] <<  4) | (sqs[ 8] <<  2) | (sqs[12] <<  0);           
          
    /*  
        210     a = 10  b = 11 c = 12 d = 13 e = 14 f = 15 g = 16
       76543    h = 17  i = 18 j = 19
       ba 98
       gfedc    
        jih      
    */   
    
    idx5 =  (sqs[ 2] << 38) | (sqs[ 1] << 36) | (sqs[ 0] << 34) | (sqs[ 7] << 32)
          | (sqs[ 6] << 30) | (sqs[ 5] << 28) | (sqs[ 4] << 26) | (sqs[ 3] << 24)
          | (sqs[11] << 22) | (sqs[10] << 20) | (sqs[ 9] << 18) | (sqs[ 8] << 16) 
          | (sqs[16] << 14) | (sqs[15] << 12) | (sqs[14] << 10) | (sqs[13] <<  8) 
          | (sqs[12] <<  6) | (sqs[19] <<  4) | (sqs[18] <<  2) | (sqs[17] <<  0); 
                                                   
    idx6 =  (sqs[16] << 38) | (sqs[11] << 36) | (sqs[ 7] << 34) | (sqs[19] << 32)
          | (sqs[15] << 30) | (sqs[10] << 28) | (sqs[ 6] << 26) | (sqs[ 2] << 24)
          | (sqs[18] << 22) | (sqs[14] << 20) | (sqs[ 5] << 18) | (sqs[ 1] << 16) 
          | (sqs[17] << 14) | (sqs[13] << 12) | (sqs[ 9] << 10) | (sqs[ 4] <<  8) 
          | (sqs[ 0] <<  6) | (sqs[12] <<  4) | (sqs[ 8] <<  2) | (sqs[ 3] <<  0); 

    idx7 =  (sqs[17] << 38) | (sqs[18] << 36) | (sqs[19] << 34) | (sqs[12] << 32)
          | (sqs[13] << 30) | (sqs[14] << 28) | (sqs[15] << 26) | (sqs[16] << 24)
          | (sqs[ 8] << 22) | (sqs[ 9] << 20) | (sqs[10] << 18) | (sqs[11] << 16) 
          | (sqs[ 3] << 14) | (sqs[ 4] << 12) | (sqs[ 5] << 10) | (sqs[ 6] <<  8) 
          | (sqs[ 7] <<  6) | (sqs[ 0] <<  4) | (sqs[ 1] <<  2) | (sqs[ 2] <<  0); 
          
    idx8 =  (sqs[ 3] << 38) | (sqs[ 8] << 36) | (sqs[12] << 34) | (sqs[ 0] << 32)
          | (sqs[ 4] << 30) | (sqs[ 9] << 28) | (sqs[13] << 26) | (sqs[17] << 24)
          | (sqs[ 1] << 22) | (sqs[ 5] << 20) | (sqs[14] << 18) | (sqs[18] << 16) 
          | (sqs[ 2] << 14) | (sqs[ 6] << 12) | (sqs[10] << 10) | (sqs[15] <<  8) 
          | (sqs[19] <<  6) | (sqs[ 7] <<  4) | (sqs[11] <<  2) | (sqs[16] <<  0);             
          
    idx1 = std::min(idx1, idx2);
    idx3 = std::min(idx3, idx4);
    idx5 = std::min(idx5, idx6);
    idx7 = std::min(idx7, idx8);
    
    idx1 = std::min(idx1, idx3);
    idx5 = std::min(idx5, idx7);
    
    idx1 = std::min(idx1, idx5);                  
          
    return idx1;          
}

void FastBoard::add_pattern_moves(int color, int vertex, movelist_t & moves, size_t & movecnt) {
    for (int i = 0; i < 8; i++) {        
        int sq = vertex + m_extradirs[i];
        
        if (m_square[sq] == EMPTY) {                  
            if (!self_atari(color, sq)) {                    
                moves[movecnt++] = sq;        
            }            
        }                                        
    }                               
    
    return;
}        

// add capture moves for color
void FastBoard::add_global_captures(int color, movelist_t & moves, size_t & movecnt) {
    // walk critical squares
    for (size_t i = 0; i < m_critical.size(); i++) {
        try_capture(color, m_critical[i], moves, movecnt);
    }
    m_critical.clear();
}

void FastBoard::check_nakade(int color, int vertex,
                             movelist_t & moves, size_t & movecnt) {
    std::tr1::array<int, 6> nakade;
    std::tr1::array<int, 6> empty_counts;
    std::tr1::array<int, 5> nbr_to_coord;

    std::fill(empty_counts.begin(), empty_counts.end(), 0);
#ifndef NDEBUG
    std::fill(nakade.begin(), nakade.end(), 0);
    std::fill(nbr_to_coord.begin(), nbr_to_coord.end(), 0);
#endif

    // we're on an empty square
    size_t sq_count = 0;
    int nbrs = count_neighbours(EMPTY, vertex);
    nbr_to_coord[nbrs] = vertex;
    empty_counts[nbrs]++;
    nakade[sq_count++] = vertex;

    size_t idx = 0;

    do {
        int new_vertex = nakade[idx];
        for (int k = 0; k < 4; k++) {
            int ai = new_vertex + m_dirs[k];
            if (m_square[ai] == !color) {
                // Not surrounded, not nakade
                return;
            } else if (m_square[ai] == color) {
                // Fill stops here, but keep looking
                continue;
            } else if (m_square[ai] == EMPTY) {
                // Add eyespace
                bool found = false;
                for (size_t j = 0; j < sq_count; j++) {
                    int sq = nakade[j];
                    if (sq == ai) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (sq_count < nakade.size()) {
                        int nbrs = count_neighbours(EMPTY, ai);
                        nbr_to_coord[nbrs] = ai;
                        empty_counts[nbrs]++;
                        nakade[sq_count++] = ai;
                    } else {
                        // always alive
                        return;
                    }
                }
            }
        }
    } while (++idx < sq_count);

    // http://senseis.xmp.net/?KillableEyeShapes
    if (sq_count <= 2) {
        return;
    } else if (sq_count == 3) {
        assert(nbr_to_coord[2] != 0);
        moves[movecnt++] = nbr_to_coord[2];
    } else if (sq_count == 4) {
        // Square 4 is dead but doesn't need immediate nakade
        // Pyramid 4 is dead
        if (empty_counts[3] == 1) {
            assert(nbr_to_coord[3] != 0);
            moves[movecnt++] = nbr_to_coord[3];
        } else if (empty_counts[2] == 2 && empty_counts[1] == 2) {
            // Straight 4 is alive
            // Bent 4 in the corner is ko
            int crit_2_lib_pnt = 0;
            bool bent4 = false;
            for (size_t j = 0; j < sq_count; j++) {
                int sq = nakade[j];
                std::pair<int, int> coords = get_xy(sq);
                if ((coords.first == 0 || coords.first == get_boardsize() - 1)
                    && (coords.second == 0 || coords.second == get_boardsize() - 1)) {
                    // corner square
                    if (count_neighbours(EMPTY, sq) == 2) {
                        // must be a bent 4, but we have to play on the other
                        // 2-lib point
                        bent4 = true;
                    }
                } else {
                    // Non corner, 2 libs, may be critical point
                    if (count_neighbours(EMPTY, sq) == 2) {
                        crit_2_lib_pnt = sq;
                    }
                }
            }
            if (bent4) {
                assert(crit_2_lib_pnt);
                moves[movecnt++] = crit_2_lib_pnt;
            }
        }
        // Everything else lives
    } else if (sq_count == 5) {
        if (empty_counts[1] == 1 && empty_counts[3] == 1) {
            // Bulky 5 is dead
            assert(nbr_to_coord[3] != 0);
            moves[movecnt++] = nbr_to_coord[3];
        } else if (empty_counts[4] == 1) {
            // Crossed 5 is dead
            assert(nbr_to_coord[4] != 0);
            moves[movecnt++] = nbr_to_coord[4];
        }
        // Everything else lives
    } else if (sq_count == 6) {
        // Rabbitty 6
        if (empty_counts[1] == 2 && empty_counts[4] == 1) {
            assert(empty_counts[2] == 3);
            assert(nbr_to_coord[4] != 0);
            moves[movecnt++] = nbr_to_coord[4];
        } else if (empty_counts[2] == 4 && empty_counts[3] == 2) {
            // Rectangular 6 in the corner is dead
            int crit_3_lib_pnt = 0;
            bool rect6 = false;
            for (size_t j = 0; j < sq_count; j++) {
                int sq = nakade[j];
                std::pair<int, int> coords = get_xy(sq);
                if ((coords.first == 0 || coords.first == get_boardsize() - 1)
                    && (coords.second == 0 || coords.second == get_boardsize() - 1)) {
                    // corner square
                    rect6 = true;
                } else {
                    // Non corner, 3 libs, may be critical point
                    if (count_neighbours(EMPTY, sq) == 3
                        && count_neighbours(color, sq) == 1
                        && count_neighbours(!color, sq) == 1) {
                        crit_3_lib_pnt = sq;
                    }
                }
            }
            if (rect6) {
                assert(crit_3_lib_pnt);
                moves[movecnt++] = crit_3_lib_pnt;
            }
        }
        // Everything else lives
    }
}

// add nakade moves for color
void FastBoard::add_nakade_moves(int color, int vertex,
                                 movelist_t & moves, size_t & movecnt) {
    // empty square directly next to last stone?
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        if (m_square[ai] == EMPTY) {
            // nakade shape is made by color not to move
            check_nakade(!color, ai, moves, movecnt);
        }
    }
}

int FastBoard::capture_size(int color, int vertex) {
    assert(m_square[vertex] == EMPTY);
    
    int limitlibs = count_neighbours(!color, vertex);
            
    if (!limitlibs) {
        return 0;
    }
    
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        
        if (m_square[ai] == !color) {
            int par = m_parent[ai];
            int lib = m_libs[par];
                           
            if (lib <= 1) {                                    
                return string_size(ai);                                                                   
            }                        
        }                                                
    }  
    
    return 0;
}

void FastBoard::try_capture(int color, int vertex, movelist_t & moves, size_t & movecnt) {
    if (m_square[vertex] == EMPTY) {                
        int limitlibs = count_neighbours(!color, vertex);
        
        // no enemy neighbors, nothing to capture
        if (!limitlibs) {
            return;
        }
    
        for (int k = 0; k < 4; k++) {
            int ai = vertex + m_dirs[k];
            
            if (m_square[ai] == !color) {
                int par = m_parent[ai];
                int lib = m_libs[par];
                                                   
                if (lib <= 1) {      
                    moves[movecnt++] = vertex;                    
                    return;                                                                                                              
                }                        
            }                                                
        }  
    }      
}

std::string FastBoard::get_stone_list() {
    std::string res;
    
    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize; j++) {
            int vertex = get_vertex(i, j);
            
            if (get_square(vertex) != EMPTY) {
                res += move_to_text(vertex) + " ";
            }
        }
    }
    
    // eat final space
    res.resize(res.size() - 1);
    
    return res;
}

int FastBoard::string_size(int vertex) {
    assert(vertex > 0 && vertex < m_maxsq);
    assert(m_square[vertex] == WHITE || m_square[vertex] == BLACK);
      
    return m_stones[m_parent[vertex]];
}

int FastBoard::nbr_libs(int color, int vertex, int count, bool plus) {
    assert(m_square[vertex] == EMPTY);
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        if (m_square[ai] == color) {
            int lc = m_libs[m_parent[ai]];
            if (!plus) {
                if (lc == count) {
                    return true;
                }
            } else {
                assert(plus);
                if (lc >= count) {
                    return true;
                }
            }
        }
    }

    return false;
}

int FastBoard::minimum_elib_count(int color, int vertex) {
    int minlib = 100; // XXX hardcoded in some places
    
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        if (m_square[ai] == !color) {
            int lc = m_libs[m_parent[ai]];            
            if (lc < minlib) {
                minlib = lc;
            }
        }    
    } 
    
    return minlib;
}

int FastBoard::enemy_atari_size(const int color, const int vertex) {
    int atari_size = 0;

    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        if (m_square[ai] == !color) {
            int lc = m_libs[m_parent[ai]];
            if (lc <= 2) {
                atari_size = std::max(string_size(ai), atari_size);
            }
        }
    }

    return atari_size;
}

// returns our lowest liberties, enemies lowest liberties
// 8 is the maximum
std::pair<int, int> FastBoard::nbr_criticality(int color, int vertex) {    
    std::tr1::array<int, 4> color_libs;

    color_libs[0] = 8;
    color_libs[1] = 8;
    color_libs[2] = 8;
    color_libs[3] = 8;
    
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];        
        int lc = m_libs[m_parent[ai]];     
        if (lc < color_libs[m_square[ai]]) {
            color_libs[m_square[ai]] = lc;
        }
    } 
        
    return std::make_pair(color_libs[color], color_libs[!color]);    
}

int FastBoard::count_rliberties(int vertex) {
    /*std::vector<bool> marker(m_maxsq, false);
    
    int pos = vertex;
    int liberties = 0;
    int color = m_square[vertex];

    assert(color == WHITE || color == BLACK);    
  
    do {       
        assert(m_square[pos] == color);
        
        for (int k = 0; k < 4; k++) {
            int ai = pos + m_dirs[k];
            if (m_square[ai] == EMPTY) {
                if (!marker[ai]) {
                    liberties++;
                    marker[ai] = true;
                }
            }
        }                
        pos = m_next[pos];
    } while (pos != vertex);            
    
    return liberties;*/
    return m_libs[m_parent[vertex]];
}

std::pair<int, int> FastBoard::after_liberties(const int color, const int vtx) {
    FastBoard tmp = *this;
    int mylibs;
    int opplibs;

    if (is_suicide(vtx, color)) {
        mylibs = 0;
    } else {
        tmp.update_board_fast(color, vtx);
        mylibs = tmp.count_rliberties(vtx);
    }

    tmp = *this;

    if (is_suicide(vtx, !color)) {
        opplibs = 0;
    } else {
        tmp.update_board_fast(!color, vtx);
        opplibs = tmp.count_rliberties(vtx);
    }

    return std::make_pair(mylibs, opplibs);
}

bool FastBoard::check_losing_ladder(const int color, const int vtx, int branching) {
    FastBoard tmp = *this;
    
    if (branching > 5) {
        return false;
    }
    
    // killing opponents?
    int elib = tmp.minimum_elib_count(color, vtx);
    if (elib == 0 || elib == 1) {
        return false;
    }
        
    int atari = vtx;        
                
    tmp.update_board_fast(tmp.m_tomove, vtx);  
    
    while (1) {   
        // suicide
        if (tmp.get_square(atari) == EMPTY) {
            return true;
        }
                                                                  
        int newlibs = tmp.count_rliberties(atari);
        
        // self-atari
        if (newlibs == 1) {
            return true;
        }
        
        // escaped ladder
        if (newlibs >= 3) {
            return false;
        }
        
        // atari on opponent
        int newelib = tmp.minimum_elib_count(color, atari);
        if (newelib == 1) {
            return false;
        }
        
        int lc = 0;
		std::tr1::array<int, 2> libarr;
        tmp.add_string_liberties<2>(atari, libarr, lc);
        
        assert(lc == 2);                
        
        // 2 good options => always lives
        if (tmp.count_pliberties(libarr[0]) == 3 && tmp.count_pliberties(libarr[1]) == 3) {
            return false;
        }
        
        // 2 equal but questionable moves => branch
        if (tmp.count_pliberties(libarr[0]) == tmp.count_pliberties(libarr[1])) {
            FastBoard tmp2 = tmp;
            
            // play atari in liberty 1, escape in liberty 2
            tmp2.update_board_fast(!tmp.m_tomove, libarr[0]);
            bool ladder1 = tmp2.check_losing_ladder(color, libarr[1], branching + 1);
            
            tmp2 = tmp;
            
            // play atari in liberty 2, escape in liberty 1
            tmp2.update_board_fast(!tmp.m_tomove, libarr[1]);
            bool ladder2 = tmp2.check_losing_ladder(color, libarr[0], branching + 1);
            
            // if both escapes lose, we're dead
            if (ladder1 && ladder2) {
                return true;
            } else {
                return false;
            }            
        } else {    
            // non branching, play atari that causes most unpleasant escape move
            if (tmp.count_pliberties(libarr[0]) > tmp.count_pliberties(libarr[1])) {
                tmp.update_board_fast(!tmp.m_tomove, libarr[0]);
            } else if (tmp.count_pliberties(libarr[0]) < tmp.count_pliberties(libarr[1])) {
                tmp.update_board_fast(!tmp.m_tomove, libarr[1]);
            }     
        }
        
        // find and play new saving move
        atari = tmp.in_atari(atari);
        tmp.update_board_fast(tmp.m_tomove, atari);   
    };
    
    return false;
    
}

int FastBoard::merged_string_size(int color, int vertex) {
    int totalsize = 0;
    std::tr1::array<int, 4> nbrpar;
    int nbrcnt = 0;        

    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        
        if (get_square(ai) == color) {
            int par = m_parent[ai];
            
            bool found = false;
            for (int i = 0; i < nbrcnt; i++) {
                if (nbrpar[i] == par) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                totalsize += string_size(ai);
                nbrpar[nbrcnt++] = par;
            }
        }
        
    }
                 
    return totalsize;
}

std::vector<int> FastBoard::get_neighbour_ids(int vertex) {
    std::vector<int> result;
    
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        
        if (get_square(ai) < EMPTY) {
            int par = m_parent[ai];
            
            bool found = false;
            for (size_t i = 0; i < result.size(); i++) {
                if (result[i] == par) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {                
                result.push_back(par);
            }
        }        
    }
    
    return result;
}

// Not alive does not imply dead
// XXX implement prediction really
int FastBoard::predict_is_alive(const int move, const int vertex) {     
    int par = m_parent[vertex];
    int color = m_square[vertex];
    int pos = par;

    assert(color == WHITE || color == BLACK);    

    std::vector<bool> marker(m_maxsq, false);
    int eyes = 0;
  
    do {       
        assert(m_square[pos] == color);
        
        for (int k = 0; k < 4; k++) {
            int ai = pos + m_dirs[k];
            if (m_square[ai] == EMPTY) {
                if (!marker[ai]) {                    
                    marker[ai] = true;
                    // not seen liberty, check if it's a real eye
                    if (predict_solid_eye(move, color, ai)) {
                        eyes++;
                        if (eyes >= 2) {
                            return eyes;
                        }
                    }
                    // might check liberties here?
                }
            }
        }                
        pos = m_next[pos];
    } while (pos != vertex);            

    return eyes;
}

int FastBoard::get_empty() {
    return m_empty_cnt;
}

int FastBoard::get_empty_vertex(int idx) {
    return m_empty[idx];
}

void FastBoard::augment_chain(std::vector<int> & chains, int vertex) {
    // get our id
    int par = m_parent[vertex];

    // check if we were found already
    std::vector<int>::iterator it = std::find(chains.begin(), chains.end(), par);

    // we are already on the list, return
    if (it != chains.end()) {
        return;
    } else {
        // add ourselves to the list
        chains.push_back(par);
    }

    int color = m_square[vertex];
    int pos = par;

    assert(color == WHITE || color == BLACK);    
    
    // discovered nearby chains (identified by parent)
    // potential chains need 2 liberties
    // sure chains are sure to be connected
    std::vector<bool> potential_chain(m_maxsq, false);

    // marks visited places
    // XXX: this is redundant with the previous
    std::vector<bool> marker(m_maxsq, false);    
  
    // go over string, note our stones and get nearby chains
    // that are surely connected
    do {       
        assert(m_square[pos] == color);                        

        for (int k = 0; k < 4; k++) {
            int ai = pos + m_dirs[k];
            
            // liberty, check if we link up through it
            if (m_square[ai] == EMPTY && !marker[ai]) {                    
                // mark it as visited
                marker[ai] = true;
                // is there another string nearby?
                for (int j = 0; j < 4; j++) {
                    int aai = ai + m_dirs[j];
                    // friendly string, not ourselves
                    if (m_square[aai] == color && m_parent[aai] != par) {
                        // playing on the liberty is illegal or 
                        // gets captured instantly, or we already
                        // found another shared liberty
                        if (count_neighbours(color, ai) >= 3
                            || potential_chain[m_parent[aai]]) {
                            augment_chain(chains, aai);
                        } else {                                    
                            potential_chain[m_parent[aai]] = true;
                        }                            
                    }
                }                                                                                
            }
        }                
        pos = m_next[pos];
    } while (pos != vertex);       
}

// returns a list of all vertices on the augmented
// chain of the string pointed to by vertex
std::vector<int> FastBoard::get_augmented_string(int vertex) {
    std::vector<int> res;
    std::vector<int> chains;

    augment_chain(chains, vertex);

    for (size_t i = 0; i < chains.size(); i++) {
        std::vector<int> stones = get_string_stones(chains[i]);
        std::copy(stones.begin(), stones.end(), back_inserter(res));
    }

    return res;
}

std::vector<int> FastBoard::dilate_liberties(std::vector<int> & vtxlist) {  
    std::vector<int> res;

    std::copy(vtxlist.begin(), vtxlist.end(), back_inserter(res));    

    // add all direct liberties
    for (size_t i = 0; i < vtxlist.size(); i++) {
        for (int k = 0; k < 4; k++) {
            int ai = m_dirs[k] + vtxlist[i];
            if (m_square[ai] == EMPTY) {
                res.push_back(ai);
            }
        }
    }

    // now uniq the list
    //std::sort(res.begin(), res.end());    
    //res.erase(std::unique(res.begin(), res.end()), res.end());    

    return res;
}

std::vector<int> FastBoard::get_nearby_enemies(std::vector<int> & vtxlist) {    
    std::vector<int> strings;
    std::vector<int> res;

    if (vtxlist.empty()) return strings;

    int color = m_square[vtxlist[0]];

    for (size_t i = 0; i < vtxlist.size(); i++) {
        assert(m_square[vtxlist[i]] == color);
        for (int k = 0; k < 8; k++) {
            int ai = get_extra_dir(k) + vtxlist[i];
            if (m_square[ai] == !color) {
                if (m_libs[m_parent[ai]] <= 3)  {
                    strings.push_back(m_parent[ai]);
                }
            }
        }
    }

    // uniq the list of string ids
    std::sort(strings.begin(), strings.end());    
    strings.erase(std::unique(strings.begin(), strings.end()), strings.end());    

    // now add full strings
    for (size_t i = 0; i < strings.size(); i++) {
        std::vector<int> stones = get_string_stones(strings[i]);
        std::copy(stones.begin(), stones.end(), back_inserter(res));
    }

    return res;
}

bool FastBoard::predict_kill(const int move, const int groupid) {
    assert(groupid == m_parent[groupid]);

    if (m_libs[m_parent[groupid]] > 1) return false;

    int color = m_square[groupid];       
    
    assert(color == WHITE || color == BLACK); 
    
    if (get_to_move() == color) {
        return false;
    }        
    
    for (int k = 0; k < 4; k++) {
        int ai = move + m_dirs[k];
        if (m_square[ai] == color && m_parent[ai] == groupid) {
            return true;            
        }
    }
    
    return false;
}
