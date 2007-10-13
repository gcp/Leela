#include <string>
#include <iostream>
#include <sstream>
#include <assert.h>

#include "config.h"

#include "FastBoard.h"
#include "Utils.h"
#include "Matcher.h"

using namespace Utils;

const std::tr1::array<int, 2> FastBoard::s_eyemask = {    
    4 * (1 << (NBR_SHIFT * BLACK)),
    4 * (1 << (NBR_SHIFT * WHITE))    
};

const std::tr1::array<int, 4> FastBoard::s_cinvert = {    
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

void FastBoard::reset_board(int size) {  
    m_boardsize = size;
    m_maxsq = (size + 2) * (size + 2);
    m_tomove = BLACK;
    m_prisoners[BLACK] = 0;
    m_prisoners[WHITE] = 0;    
    m_stones[BLACK] = 0;    
    m_stones[WHITE] = 0;    
    m_empty_cnt = 0;        

    m_dirs[0] = -size-2;
    m_dirs[1] = +1;
    m_dirs[2] = +size+2;
    m_dirs[3] = -1;    
    
    m_alterdirs[0] = 1;
    m_alterdirs[1] = size+2;
    m_alterdirs[2] = 1;
    m_alterdirs[3] = size+2;    
    
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
    m_plibs[MAXSQ]  = 999;
    m_next[MAXSQ]   = MAXSQ;                        
}

bool FastBoard::is_suicide(int i, int color) {    
    int k;     
    int connecting = false;
        
    for (k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
                      
        if (get_square(ai) == EMPTY) {
            return false;
        } else {
            int libs = m_plibs[m_parent[ai]];
            if (get_square(ai) == color) {
                if (libs > 4) {
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
    }
    
    add_neighbour(i, color);
    
    bool opps_live = true;
    bool ours_die = true;
    
    for (k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
                 
        int libs = m_plibs[m_parent[ai]];
        
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

int FastBoard::count_liberties(const int i) {          
    return count_neighbours(EMPTY, i);
}

int FastBoard::count_neighbours(const int c, const int v) {
    assert(c == WHITE || c == BLACK || c == EMPTY);
    return (m_neighbours[v] >> (NBR_SHIFT * c)) & 7;
}

int FastBoard::fast_ss_suicide(const int color, const int i)  {    
    int eyeplay = (m_neighbours[i] & s_eyemask[!color]);
    
    if (!eyeplay) return false;
    
    int all_live = true;    
        
    if (--m_plibs[m_parent[i - 1              ]] <= 0) all_live = false;
    if (--m_plibs[m_parent[i + 1              ]] <= 0) all_live = false;
    if (--m_plibs[m_parent[i + m_boardsize + 2]] <= 0) all_live = false;
    if (--m_plibs[m_parent[i - m_boardsize - 2]] <= 0) all_live = false;
    
    m_plibs[m_parent[i - m_boardsize - 2]]++;             
    m_plibs[m_parent[i + m_boardsize + 2]]++;
    m_plibs[m_parent[i + 1]]++;
    m_plibs[m_parent[i - 1]]++;                         
        
    return all_live;
}

void FastBoard::add_neighbour(const int i, const int color) {       
    assert(color == WHITE || color == BLACK || color == EMPTY);                        
                           
    m_neighbours[i - 1]               += (1 << (NBR_SHIFT * color)) - (1 << (NBR_SHIFT * EMPTY));                       
    m_neighbours[i + 1]               += (1 << (NBR_SHIFT * color)) - (1 << (NBR_SHIFT * EMPTY));  
    m_neighbours[i + m_boardsize + 2] += (1 << (NBR_SHIFT * color)) - (1 << (NBR_SHIFT * EMPTY));  
    m_neighbours[i - m_boardsize - 2] += (1 << (NBR_SHIFT * color)) - (1 << (NBR_SHIFT * EMPTY));         

    m_plibs[m_parent[i - 1]]--;
    m_plibs[m_parent[i + 1]]--;
    m_plibs[m_parent[i + m_boardsize + 2]]--;
    m_plibs[m_parent[i - m_boardsize - 2]]--;                                       
}

void FastBoard::remove_neighbour(const int i, const int color) {         
    assert(color == WHITE || color == BLACK || color == EMPTY);        
        
    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
                           
        m_neighbours[ai] += (1 << (NBR_SHIFT * EMPTY))  
                          - (1 << (NBR_SHIFT * color));        
                            
        m_plibs[m_parent[ai]]++;                 
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
        m_stones[color]--;    
        
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
        
    fill(bd.begin(), bd.end(), false);
    fill(last.begin(), last.end(), false);
        
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

float FastBoard::percentual_area_score(float komi) {    
    return area_score(komi) / (float)(m_boardsize * m_boardsize);
}

float FastBoard::area_score(float komi) {
    
    std::vector<bool> white = calc_reach_color(WHITE);
    std::vector<bool> black = calc_reach_color(BLACK);

    float score = -komi;

    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize; j++) {  
            int vertex = get_vertex(i, j);
            
            assert(!(white[vertex] && black[vertex]));
            assert(!(white[vertex] && m_square[vertex] == BLACK));
            assert(!(black[vertex] && m_square[vertex] == WHITE));
            
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

    bsc = m_stones[BLACK];       
    wsc = m_stones[WHITE];   

    return bsc-wsc-((int)komi)+1;
}

float FastBoard::final_mc_score(float komi) {    
    int wsc, bsc;        
    int maxempty = m_empty_cnt;
    
    bsc = m_stones[BLACK];       
    wsc = m_stones[WHITE];
        
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

std::vector<int> FastBoard::run_bouzy(int dilat, int eros) {
    int i;
    int d, e;
    int goodsec, badsec;
    std::vector<int> tmp(MAXSQ);
    std::vector<int> influence(MAXSQ);
    
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

void FastBoard::display_influence(void) {
    int i, j;    
    /* 4/13 alternate 5/10 moyo 4/0 area */
    std::vector<int> influence = run_bouzy(5, 21);
    
    for (j = m_boardsize-1; j >= 0; j--) {
        for (i = 0; i < m_boardsize; i++) {
            int infl = influence[get_vertex(i,j)];
            if (infl > 0) {
                if (get_square(i, j) ==  BLACK) {
                    myprintf("X ");
                } else if (get_square(i, j) == WHITE) {
                    myprintf("o ");
                } else {
                    myprintf("x ");
                }                               
            } else if (infl < 0) {
                if (get_square(i, j) ==  BLACK) {
                    myprintf("x ");
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
        myprintf("%c ", (('a' + i < 'i') ? 'a' + i : 'a' + i + 1));
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
        myprintf("%c ", (('a' + i < 'i') ? 'a' + i : 'a' + i + 1));
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
                int libs = m_plibs[m_parent[get_vertex(i,j)]];
                if (libs > 9) { libs = 9; };
                myprintf("%1d", libs);                
            } else if (get_square(i,j) == BLACK)  {
                int libs = m_plibs[m_parent[get_vertex(i,j)]];
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

    /* merge liberties */
    m_plibs[ip]   += m_plibs[aip];                   
    
    /* loop over stones, update parents */       
    int newpos = aip;

    do {       
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
    m_plibs[i]   = 0;            
    m_stones[color]++;           
    
    add_neighbour(i, color); 

    int captured_sq;    
    int captured_stones = 0;

    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];

        assert(ai >= 0 && ai <= m_maxsq);
       
        if (m_plibs[m_parent[ai]] <= 0) {
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
    m_plibs[i]   = count_liberties(i);            
    m_stones[color]++;                        
        
    assert(m_plibs[i] >= 0 && m_plibs[i] <= 4);        
        
    add_neighbour(i, color); 
    
    int captured_sq;    
    int captured_stones = 0;
    
    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
        
        if (m_square[ai] > WHITE) continue; 
        
        assert(ai >= 0 && ai <= m_maxsq);
                                       
        if (m_square[ai] == !color) {
            if (m_plibs[m_parent[ai]] <= 0) {
                int this_captured    = remove_string_fast(ai);
                captured_sq          = ai;
                captured_stones     += this_captured;                                                                      
            }
        } else if (m_square[ai] == color) {                                                    
            int ip  = m_parent[i];
            int aip = m_parent[ai];                    
                                   
            if (ip != aip) {
                /* determining the shorter string is slower */                
                merge_strings(aip, ip);                
            }                                         
        }        
    }           
    
    m_prisoners[color] += captured_stones;   
    
    /* move last vertex in list to our position */    
    int lastvertex               = m_empty[--m_empty_cnt];
    m_empty_idx[lastvertex]      = m_empty_idx[i];
    m_empty[m_empty_idx[i]]      = lastvertex;                      
        
    assert(m_plibs[m_parent[i]] >= 0);
    
    /* check whether we still live (i.e. detect suicide) */    
    if (m_plibs[m_parent[i]] == 0) {                                
        assert(captured_stones == 0);        
        remove_string_fast(i);                
    } 
        
    return -1;
}

bool FastBoard::no_eye_fill(const int i) {     
    int color = m_tomove;    
    
    /* check for 4 neighbors of the same color */
    int ownsurrounded = (m_neighbours[i] & s_eyemask[color]);
    
    /* if not, it can't be an eye */
    if (!ownsurrounded) {
        return true;
    }                                      

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
            return true;
        }
    } else {
        if (colorcount[!color]) {
            return true;
        }
    }                        
                               
    return false;    
}

std::string FastBoard::move_to_text(int move) {    
    std::ostringstream result;
    
    int column = move % (m_boardsize + 2);
    int row = move / (m_boardsize + 2);
    
    column--;
    row--;
    
    assert(move == FastBoard::PASS || (row >= 0 && row < m_boardsize));
    assert(move == FastBoard::PASS || (column >= 0 && column < m_boardsize));
    
    if (move >= 0 && move <= m_maxsq) {
        result << static_cast<char>(column < 8 ? 'A' + column : 'A' + column + 1);
        result << (row + 1);        
    } else if (move == FastBoard::PASS) {
        result << "pass";
    } else if (move == -2) {
	result << "?";
    } else {
	result << "error";
    }
    	
    return result.str();
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

// check if string is in atari, returns 0 if not,
// single liberty if it is
int FastBoard::in_atari(int vertex) {        
    int pos = vertex;        
    int liberty = 0;
    
    assert(m_square[vertex] < EMPTY);
  
    do {                               
        if (count_liberties(pos)) {                    
            for (int k = 0; k < 4; k++) {
                int ai = pos + m_dirs[k];                                
                
                if (m_square[ai] == EMPTY) { 
                    if (liberty == 0) {                               
                        liberty = ai;
                    } else if (ai != liberty) {
                        return 0;
                    }
                }
            }                    
        }
        
        pos = m_next[pos];
    } while (pos != vertex);    
    
    return liberty; 
}

// loop over a string and try to kill neighbors
void FastBoard::kill_neighbours(int vertex, std::vector<int> & work) {       
    int scolor = m_square[vertex];            
    int kcolor = !scolor;
    int pos = vertex;
          
    do {                               
        assert(m_square[pos] == scolor);
            
        for (int k = 0; k < 4; k++) {
            int ai = pos + m_dirs[k];                                
            
            if (m_square[ai] == kcolor) { 
                int par = m_parent[ai];
                int lib = m_plibs[par];
                
                if (lib <= 4) {
                    int atari = in_atari(ai);
                    
                    if (atari) {
                        assert(m_square[atari] == EMPTY);
                        work.push_back(atari);
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
            int lib = m_plibs[par];
            
            if (lib <= 4) {                
                int atari = in_atari(ai);
                
                if (atari) {                                            
                    if (!self_atari(color, atari)) {
                        return string_size(ai);  
                    }                                         
                }
            }
        } 
    }
    
    return 0;        
}

// look for a neighbors of vertex with "color" that are critical,
// and add moves that save them to work
// vertex is sure to be filled with !color
void FastBoard::save_critical_neighbours(int color, int vertex,
                                         std::vector<int> & work) {        
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        
        if (m_square[ai] == color) {        
            int par = m_parent[ai];
            int lib = m_plibs[par];
            
            if (lib <= 4) {                
                int atari = in_atari(ai);
                
                if (atari) {                        
                    size_t startsize = work.size();
                                    
                    // find saving moves for atari square "atari"                
                    // we can save by either
                    // 1) playing in the atari if it increases liberties,
                    //    i.e. it is not self-atari 
                    // 2) capturing an opponent, which means that he should
                    //    also be in atari                                                        
                    if (!self_atari(color, atari)) {
                        work.push_back(atari);        
                    } 
                    
                    kill_neighbours(ai, work);     
                    
                    // saving moves failed, add this to critical points
                    if (work.size() == startsize) {
                        // keep the queue small
                        while (m_critical.size() > 8) {
                            m_critical.pop();
                        }
                        m_critical.push(atari);
                    }
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
    int connecting = false;
    int i = vertex;          
    
    add_neighbour(i, color);
    
    bool opps_live = true;    
    bool live_connect = false;
    
    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
                 
        int libs = m_plibs[m_parent[ai]];
        
        if (libs == 0 && get_square(ai) == !color) {  
            opps_live = false;                                   
        } else if (libs >= 8 && get_square(ai) == color) {
            live_connect = true;
        }      
    }  
    
    remove_neighbour(i, color);      
    
    return live_connect || !opps_live;    
}

template <int N> 
void FastBoard::add_string_liberties(int vertex, 
                                     std::tr1::array<int, N> & nbr_libs, 
                                     int & nbr_libs_cnt) {
    int pos = vertex;    
    int color = m_square[pos];
  
    do {               
        assert(m_square[pos] == color);
        
        if (count_liberties(pos)) {        
            // look for empties near this stone
            for (int k = 0; k < 4; k++) {
                int ai = pos + m_dirs[k];                                
                
                if (m_square[ai] == EMPTY) {                                
                    bool found = false;
                    
                    for (int i = 0; i < nbr_libs_cnt; i++) {
                        if (nbr_libs[i] == ai) {
                            found = true;
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
    
    int newlibs = count_liberties(vertex);
    
    if (newlibs >= 2) {        
        return false;                
    }
    
    // 2) if we kill an enemy, or connect to safety, we're good 
    // as well
    
    if (kill_or_connect(color, vertex)) {        
        return false;
    }
    
    // 3) we only add at most 1 liberty, and we removed 1, so check if 
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
                }
            }
                                        
            // not in list yet, so add
            if (!found) {
                nbr_libs[nbr_libs_cnt++] = ai;
            }                
        } else if (get_square(ai) == color) {        
            int par = m_parent[ai];
            int lib = m_plibs[par];
            
            // we already know this neighbor does not have a large 
            // number of liberties, and to contribute, he must have
            // more liberties than just the one that is "vertex"
            if (lib > 1) {
                add_string_liberties(ai, nbr_libs, nbr_libs_cnt);
            }            
        }
        
        if (nbr_libs_cnt > 2) {            
            return false;
        }
    }
    
    // if we get here, there are no more than 2 liberties,
    // and we just removed 1 of those (since we added the play square
    // to the list), so it must be an auto-atari    
    return true;
}

int FastBoard::get_pattern(const int sq, bool invert) {          
    const int size = m_boardsize;
    
    if (!invert) {
        return (m_square[sq - size - 2 - 1] << 14)
             | (m_square[sq - size - 2]     << 12)
             | (m_square[sq - size - 2 + 1] << 10)
             | (m_square[sq - 1]            <<  8)
             | (m_square[sq + 1]            <<  6)
             | (m_square[sq + size + 2 - 1] <<  4)
             | (m_square[sq + size + 2]     <<  2)
             | (m_square[sq + size + 2 + 1] <<  0);  
    } else {
        return (s_cinvert[m_square[sq - size - 2 - 1]] << 14)
             | (s_cinvert[m_square[sq - size - 2]]     << 12)
             | (s_cinvert[m_square[sq - size - 2 + 1]] << 10)
             | (s_cinvert[m_square[sq - 1]]            <<  8)
             | (s_cinvert[m_square[sq + 1]]            <<  6)
             | (s_cinvert[m_square[sq + size + 2 - 1]] <<  4)
             | (s_cinvert[m_square[sq + size + 2]]     <<  2)
             | (s_cinvert[m_square[sq + size + 2 + 1]] <<  0);  
    }         
}

int FastBoard::get_pattern4(const int sq, bool invert) {          
    const int size = m_boardsize;

    if (!invert) {
        return (m_square[sq - 2*(size + 2)]      << 22)
             | (m_square[sq + 2*(size + 2)]      << 20)
             | (m_square[sq + 2]                 << 18)
             | (m_square[sq - 2]                 << 16)
             | (m_square[sq - (size + 2) - 1]    << 14)
             | (m_square[sq - (size + 2)]        << 12)
             | (m_square[sq - (size + 2) + 1]    << 10)
             | (m_square[sq - 1]                 <<  8)
             | (m_square[sq + 1]                 <<  6)
             | (m_square[sq + (size + 2) - 1]    <<  4)
             | (m_square[sq + (size + 2)]        <<  2)
             | (m_square[sq + (size + 2) + 1]    <<  0);  
    } else {
        return (s_cinvert[m_square[sq - 2*(size + 2)]]      << 22)
             | (s_cinvert[m_square[sq + 2*(size + 2)]]      << 20)
             | (s_cinvert[m_square[sq + 2]]                 << 18)
             | (s_cinvert[m_square[sq - 2]]                 << 16)
             | (s_cinvert[m_square[sq - (size + 2) - 1]]    << 14)
             | (s_cinvert[m_square[sq - (size + 2)]]        << 12)
             | (s_cinvert[m_square[sq - (size + 2) + 1]]    << 10)
             | (s_cinvert[m_square[sq - 1]]                 <<  8)
             | (s_cinvert[m_square[sq + 1]]                 <<  6)
             | (s_cinvert[m_square[sq + (size + 2) - 1]]    <<  4)
             | (s_cinvert[m_square[sq + (size + 2)]]        <<  2)
             | (s_cinvert[m_square[sq + (size + 2) + 1]]    <<  0);  
    }         
}

void FastBoard::add_pattern_moves(int color, int vertex,
                                  std::vector<int> & work) {                                      
    Matcher * matcher = Matcher::get_Matcher();

    for (int i = 0; i < 8; i++) {        
        int sq = vertex + m_extradirs[i];
        
        if (m_square[sq] == EMPTY) {      
            int pattern = get_pattern(sq, false);            
            
            if (matcher->matches(color, pattern)) {
                if (!self_atari(color, sq)) {
                    work.push_back(sq);
                }
            }
        }                                        
    }                                            
}        

// check for fixed patterns around vertex for color to move
// include rotations
bool FastBoard::match_pattern(int color, int vertex) {
    for (int k = 0; k < 4; k++) {
        int ss = vertex + m_dirs[k];
        
        if (m_square[ss] < EMPTY) {
            int c1 = m_square[ss];
            // pattern 1
            if (m_square[vertex + m_dirs[(k + 1) % 4]] == EMPTY
                && m_square[vertex + m_dirs[(k + 3) % 4]] == EMPTY) {
                // pattern 2 empties
                if (m_square[vertex + m_dirs[(k + 2) % 4]] == EMPTY) {
                    // pattern 2 white stone nbrs
                    if ((m_square[ss + m_alterdirs[k]] == !c1
                        && m_square[ss - m_alterdirs[k]] == EMPTY) 
                        ||
                        (m_square[ss - m_alterdirs[k]] == !c1
                         && m_square[ss + m_alterdirs[k]] == EMPTY)) {
                         return true;
                    }
                }
                // pattern 1 white stone nbrs
                if (m_square[ss + m_alterdirs[k]] == !c1
                    && m_square[ss - m_alterdirs[k]] == !c1) {
                    return true;
                }
            }
            // pattern 3
            if (m_square[vertex + m_dirs[(k + 1) % 4]] == EMPTY
                && m_square[vertex + m_dirs[(k + 2) % 4]] == EMPTY
                 && m_square[vertex + m_dirs[(k + 3) % 4]] == !c1) {
                if (m_square[vertex + m_dirs[k] + m_dirs[(k + 3) % 4]] == !c1) {
                    return true;
                }                 
            }
            // pattern 3 (other rotation)
            if (m_square[vertex + m_dirs[(k + 2) % 4]] == EMPTY
               && m_square[vertex + m_dirs[(k + 3) % 4]] == EMPTY
               && m_square[vertex + m_dirs[(k + 1) % 4]] == !c1) {
               if (m_square[vertex + m_dirs[k] + m_dirs[(k + 1) % 4]] == !c1) {
                   return true;
                }                 
            }      
            // pattern 4       
            if (c1 != color) {
                if (m_square[vertex + m_dirs[(k + 1) % 4]] == EMPTY
                    && m_square[vertex + m_dirs[(k + 2) % 4]] == EMPTY
                    && m_square[vertex + m_dirs[(k + 3) % 4]] == EMPTY) {                    
                    if (m_square[ss + m_alterdirs[k]] == c1
                        && m_square[ss - m_alterdirs[k]] == !c1) {
                        return true;
                    }
                    if (m_square[ss - m_alterdirs[k]] == c1
                        && m_square[ss + m_alterdirs[k]] == !c1) {
                        return true;
                    }
                }
            }
            // pattern 5             
            if (m_square[vertex + m_dirs[(k + 1) % 4]] == c1) {
                if (m_square[vertex + m_dirs[k] + m_dirs[(k + 1) % 4]] == !c1) { 
                    // pattern 6 & 7                                       
                    if ((m_square[vertex + m_dirs[(k + 3) % 4]] == c1
                        && m_square[vertex + m_dirs[(k + 2) % 4]] == EMPTY)
                        ||
                        (m_square[vertex + m_dirs[(k + 2) % 4]] == c1
                        && m_square[vertex + m_dirs[(k + 3) % 4]] == EMPTY)) {
                    } else {
                        return true;
                    }                        
                }
            }
            if (m_square[vertex + m_dirs[(k + 3) % 4]] == c1) {
                if (m_square[vertex + m_dirs[k] + m_dirs[(k + 3) % 4]] == !c1) {
                    // pattern 6 & 7
                    if ((m_square[vertex + m_dirs[(k + 1) % 4]] == c1
                         && m_square[vertex + m_dirs[(k + 2) % 4]] == EMPTY)
                         ||
                         (m_square[vertex + m_dirs[(k + 2) % 4]] == c1
                         && m_square[vertex + m_dirs[(k + 1) % 4]] == EMPTY)) {
                     } else {
                         return true;
                     }      
                 } 
            }
            // pattern 8
            if (m_square[vertex + m_dirs[(k + 1) % 4]] == !c1
                && m_square[vertex + m_dirs[(k + 3) % 4]] == !c1) {
                if (m_square[vertex + m_dirs[(k + 2) % 4]] != c1
                   || m_square[vertex + m_dirs[(k + 2) % 4] + m_alterdirs[k]] != c1
                   || m_square[vertex + m_dirs[(k + 2) % 4] - m_alterdirs[k]] != c1) {
                   return true;
                }
            }
        } else if (m_square[ss] == INVAL) {
            // edge patterns    
            // pattern 9
            if (m_square[vertex + m_dirs[(k + 2) % 4]] == EMPTY) {
                if (m_square[vertex + m_dirs[(k + 1) % 4]] != EMPTY) {
                    int c1 = m_square[vertex + m_dirs[(k + 1) % 4]];
                    if (m_square[vertex + m_dirs[(k + 1) % 4] + m_dirs[k]] == !c1
                        || m_square[vertex + m_dirs[(k + 1) % 4] - m_dirs[k]] == !c1) {
                        return true;
                    }
                }
                if (m_square[vertex + m_dirs[(k + 3) % 4]] != EMPTY) {
                    int c1 = m_square[vertex + m_dirs[(k + 3) % 4]];
                    if (m_square[vertex + m_dirs[(k + 3) % 4] + m_dirs[k]] == !c1
                        || m_square[vertex + m_dirs[(k + 3) % 4] - m_dirs[k]] == !c1) {
                        return true;
                    }
                }
            }
            // pattern 10
            if (m_square[vertex + m_dirs[(k + 2) % 4]] != EMPTY) {
                int c1 = m_square[vertex + m_dirs[(k + 2) % 4]];
                if (m_square[vertex + m_dirs[(k + 3) % 4]] == !c1
                  && m_square[vertex + m_dirs[(k + 1) % 4]] != c1) {
                    return true;
                }
                if (m_square[vertex + m_dirs[(k + 3) % 4]] == !c1
                  && m_square[vertex + m_dirs[(k + 1) % 4]] != c1) {
                    return true;
                }
            }
            // pattern 11
            if (m_square[vertex + m_dirs[(k + 2) % 4]] == color
                && m_square[vertex + m_dirs[(k + 2) % 4] + m_alterdirs[k]] == !color) {
                return true;
            }
            if (m_square[vertex + m_dirs[(k + 2) % 4]] == color
                && m_square[vertex + m_dirs[(k + 2) % 4] - m_alterdirs[k]] == !color) {
                return true;
            }
            // pattern 12
            if (m_square[vertex + m_dirs[(k + 2) % 4]] == !color) {
                if (m_square[vertex + m_dirs[(k + 1) % 4]] != !color) {
                    if (m_square[vertex + m_dirs[(k + 2) % 4] + m_dirs[(k + 1) % 4]] == color) {
                        return true;
                    }
                }
                if (m_square[vertex + m_dirs[(k + 3) % 4]] != !color) {
                    if (m_square[vertex + m_dirs[(k + 2) % 4] + m_dirs[(k + 3) % 4]] == color) {
                        return true;
                    }
                }
            }
            // pattern 13
            if (m_square[vertex + m_dirs[(k + 2) % 4]] == !color) {
                if (m_square[vertex + m_dirs[(k + 1) % 4]] == !color
                    && m_square[vertex + m_dirs[(k + 3) % 4]] == color) {
                    if (m_square[vertex + m_dirs[(k + 1) % 4] + m_dirs[(k + 2) % 4]] == color) {
                        return true;
                    }
                }
                if (m_square[vertex + m_dirs[(k + 3) % 4]] == !color
                   && m_square[vertex + m_dirs[(k + 1) % 4]] == color) {
                    if (m_square[vertex + m_dirs[(k + 3) % 4] + m_dirs[(k + 2) % 4]] == color) {
                        return true;
                    }
                }
            }
        }    
    }
    
    return false;
}

// add capture moves for color
void FastBoard::add_global_captures(int color, std::vector<int> & work) {
    // walk critical squares    
    while (!m_critical.empty()) {
        int sq = m_critical.front();
        m_critical.pop();                
        
        try_capture(color, sq, work);
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
            int lib = m_plibs[par];
                           
            if (lib <= 4 && limitlibs >= lib) {                    
                int samenbrs = 0;
                
                // check nbrs of original empty square
                for (int kk = 0; kk < 4; kk++) {
                    int aai = vertex + m_dirs[kk];
                    
                    if (m_square[aai] == !color) {
                        if (m_parent[aai] == par) {
                            samenbrs++;
                        }
                    }                            
                }
                
                assert(samenbrs <= lib);    
                
                if (samenbrs >= lib) {                                                
                    return string_size(ai);           
                }                                        
            }                        
        }                                                
    }  
    
    return false;
}

void FastBoard::try_capture(int color, int vertex, std::vector<int> & work) {
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
                int lib = m_plibs[par];
                    
                // less than 4 liberties, we are sitting on one                                    
                // quick check if there are more psuedoliberties 
                // than there are liberties around the starting square,
                // which means they certainly are not just 1 true liberty
                if (lib <= 4 && limitlibs >= lib) {                    
                    int samenbrs = 0;
                    
                    // check nbrs of original empty square
                    for (int kk = 0; kk < 4; kk++) {
                        int aai = vertex + m_dirs[kk];
                        
                        if (m_square[aai] == !color) {
                            if (m_parent[aai] == par) {
                                samenbrs++;
                            }
                        }                            
                    }
                    
                    assert(samenbrs <= lib);    
                    
                    if (samenbrs >= lib) {                            
                        work.push_back(vertex);  
                        return;                                                  
                    }                                        
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
    int pos = vertex;
    int res = 0;
    int color = m_square[vertex];

    assert(color == WHITE || color == BLACK || color == EMPTY);
  
    do {       
        assert(m_square[pos] == color);
        
        res++;
        pos = m_next[pos];
    } while (pos != vertex);    

    return res;
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

    return xy;
}

// returns 1 to 6 real liberties
int FastBoard::minimum_elib_count(int color, int vertex) {
    int minlib = 100;
    
    for (int k = 0; k < 4; k++) {
        int ai = vertex + m_dirs[k];
        if (m_square[ai] == !color) {
            int lc = 0;
            boost::array<int, 6> tmp;
            add_string_liberties(ai, tmp, lc);    
            if (lc < minlib) {
                minlib = lc;
            }
        }    
    } 
    
    return minlib;
}