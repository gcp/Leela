#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "config.h"

#include "FastBoard.h"
#include "Utils.h"

using namespace Utils;

const std::tr1::array<int, 2> FastBoard::s_eyemask = {    
    4 * (1 << (NBR_SHIFT * BLACK)),
    4 * (1 << (NBR_SHIFT * WHITE))    
};  

int FastBoard::get_boardsize(void) {
    return m_boardsize;
}
    
int FastBoard::get_vertex(int x, int y) {
    assert(x >= 0 && x <= MAXBOARDSIZE);
    assert(y >= 0 && y <= MAXBOARDSIZE);
    assert(x >= 0 && x <= m_boardsize);
    assert(y >= 0 && y <= m_boardsize);
    
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
    m_dirs[1] = -1;
    m_dirs[2] = +1;
    m_dirs[3] = +size+2;
    
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
    return m_neighbours[i] >> (NBR_SHIFT * EMPTY);
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
                    for (int k = 0; k < 4; k++) {                    
                        if (m_square[vertex + m_dirs[k]] != INVAL) {
                            bd[vertex + m_dirs[k]] = true;
                        }                        
                    }
                } else if (m_square[vertex] == EMPTY && bd[vertex]) {               
                    for (int k = 0; k < 4; k++) {                    
                        if (m_square[vertex + m_dirs[k]] != INVAL) {
                            bd[vertex + m_dirs[k]] = true;
                        }                        
                    }
                }
            }                    
        }        
    } while (last != bd);  
    
    return bd;
}

float FastBoard::final_score(float komi) {
    
    std::vector<bool> white = calc_reach_color(WHITE);
    std::vector<bool> black = calc_reach_color(BLACK);

    float score = -komi;

    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize; j++) {  
            int vertex = get_vertex(i, j);
            
            if (white[vertex] && !black[vertex]) {
                score -= 1.0f;
            } else if (black[vertex] && !white[vertex]) {
                score += 1.0f;
            }
        }
    }        
    
    return score;
}   

int FastBoard::estimate_score(float komi) {    
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

void FastBoard::run_bouzy(int *influence, int dilat, int eros) {
    int i;
    int d, e;
    int goodsec, badsec;
    int tmp[MAXSQ];
    
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
    
    memcpy(tmp, influence, sizeof(int) * m_maxsq);
        
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
        memcpy(influence, tmp, sizeof(int) * m_maxsq);
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
        memcpy(influence, tmp, sizeof(int) * m_maxsq);
    }        
}

void FastBoard::display_influence(void) {
    int i, j;
    int influence[MAXSQ];
    /* 4/13 alternate 5/10 moyo 4/0 area */
    run_bouzy(influence, 5, 21);
    
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
    int influence[MAXSQ];
    
    /* 2/3 3/7 4/13 alternate: 5/10 moyo 4/0 area */    
    run_bouzy(influence, 5, 21);

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
   
    if (m_tomove == BLACK) {
	tmp += 1;
    } else {
        tmp -= 1;			
    }	    
    
    tmp -= komi;
    
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

 void FastBoard::move_to_text(int move, char *vertex) {        
    int column = move % (m_boardsize + 2);
    int row = move / (m_boardsize + 2);
    
    column--;
    row--;
    
    assert(move == FastBoard::PASS || (row >= 0 && row < m_boardsize));
    assert(move == FastBoard::PASS || (column >= 0 && column < m_boardsize));
    
    if (move >= 0 && move <= m_maxsq) {
        sprintf(vertex, "%c%d", (column < 8 ? 'A' + column : 'A' + column + 1), row+1); 
    } else if (move == FastBoard::PASS) {
        sprintf(vertex, "pass");
    } else if (move == -2) {
	sprintf(vertex, "?");
    } else {
	sprintf(vertex, "error");
    }
    	
    return;
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
        char vtx[16];
        move_to_text(newpos, &vtx[0]);
        std::string vertstring(vtx);
        result += vertstring + " "; 
        newpos = m_next[newpos];
    } while (newpos != start);   
    
    // eat last space
    result.resize(result.size() - 1);
    
    return result;
}