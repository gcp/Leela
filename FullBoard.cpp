#include "config.h"

#include "FullBoard.h"
#include "Zobrist.h"
#include "Utils.h"

using namespace Utils;

int FullBoard::remove_string(int i) {
    int pos = i;
    int removed = 0;
    int color = m_square[i];
  
    do {                    
        hash    ^= Zobrist::zobrist[m_square[pos]][pos];
        ko_hash ^= Zobrist::zobrist[m_square[pos]][pos];                                                          
                        
        m_square[pos] = EMPTY;      
        m_stones[color]--;        
        m_parent[pos] = MAXSQ;    
        
        remove_neighbour(pos, color);  
        
        m_empty_idx[pos]      = m_empty_cnt;
        m_empty[m_empty_cnt]  = pos;            
        m_empty_cnt++;
        
        hash    ^= Zobrist::zobrist[m_square[pos]][pos];
        ko_hash ^= Zobrist::zobrist[m_square[pos]][pos];    
                
        removed++;
        pos = m_next[pos];
    } while (pos != i);    

    return removed;
}


uint64 FullBoard::calc_ko_hash(void) {
    uint64 res;
    
#ifdef _WIN32
    res = 0x1234567887654321UI64;
#else
    res = 0x1234567887654321ULL;
#endif
    
    for (int i = 0; i < m_maxsq; i++) {
        if (m_square[i] != INVAL) {
            res ^= Zobrist::zobrist[m_square[i]][i];
        }
    }
    
    ko_hash = res;
    
    /* Tromp-Taylor has positional superko */        
    return res;
}

uint64 FullBoard::calc_hash(void) {
    uint64 res;    
    
#ifdef _WIN32
    res = 0x1234567887654321UI64;
#else
    res = 0x1234567887654321ULL;
#endif
    
    for (int i = 0; i < m_maxsq; i++) {
        if (m_square[i] != INVAL) {
            res ^= Zobrist::zobrist[m_square[i]][i];
        }            
    }
        
    /* prisoner hashing is rule set dependent */
    res ^= Zobrist::zobrist_pris[0][m_prisoners[0]];
    res ^= Zobrist::zobrist_pris[1][m_prisoners[1]];    
    
    if (m_tomove == BLACK)
#ifdef _WIN32
        res ^= 0xABCDABCDABCDABCDUI64;
#else
        res ^= 0xABCDABCDABCDABCDULL;
#endif    

    hash = res;

    return res;
}

uint64 FullBoard::get_hash(void) {
    return hash;
}


int FullBoard::update_board(const int color, const int i) {         
    assert(m_square[i] == EMPTY);   
    
    hash    ^= Zobrist::zobrist[m_square[i]][i];
    ko_hash ^= Zobrist::zobrist[m_square[i]][i];      
        
    m_square[i]    = (square_t)color;    
    m_next[i]      = i;     
    m_parent[i]    = i;
    m_plibs[i]     = count_liberties(i);
    m_stones[color]++;
    
    hash    ^= Zobrist::zobrist[m_square[i]][i];
    ko_hash ^= Zobrist::zobrist[m_square[i]][i];               
    
    /* update neighbor liberties (they all lose 1) */        
    add_neighbour(i, color);    
        
     /* did we play into an opponent eye? */
    int eyeplay = (m_neighbours[i] & s_eyemask[!color]);           
       
    int captured_sq;
    int captured_stones = 0;
    
    for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];
                
        if (m_square[ai] == !color) {
            if (m_plibs[m_parent[ai]] <= 0) {
                int this_captured    = remove_string(ai);
                captured_sq          = ai;
                captured_stones     += this_captured;                                
            }
        } else if (m_square[ai] == color) {                
            int ip  = m_parent[i];
            int aip = m_parent[ai];                    
                                   
            if (ip != aip) {                                                                         
                if (m_plibs[ip] >= m_plibs[aip]) {                
                    merge_strings(ip, aip);                                                        
                } else {
                    merge_strings(aip, ip);                                                        
                }                       
            }                                         
        }        
    }                                             
    
    hash ^= Zobrist::zobrist_pris[color][m_prisoners[color]];                
    m_prisoners[color] += captured_stones;    
    hash ^= Zobrist::zobrist_pris[color][m_prisoners[color]];                  
    
    /* move last vertex in list to our position */    
    int lastvertex           = m_empty[--m_empty_cnt];
    m_empty_idx[lastvertex]  = m_empty_idx[i];
    m_empty[m_empty_idx[i]]  = lastvertex;    
    
    /* check whether we still live (i.e. detect suicide) */    
    if (m_plibs[m_parent[i]] == 0) {                                
        assert(captured_stones == 0);        
        remove_string_fast(i);                
    }                                                  
            
    /* check for possible simple ko */
    if (captured_stones == 1 && eyeplay) {
        return captured_sq;
    } else {
        return -1;
    }        
}

void FullBoard::display_board(int lastmove) {
    FastBoard::display_board(lastmove);
    
    myprintf("Hash: %I64X Ko-Hash: %I64X\n\n", hash, ko_hash);
}

void FullBoard::reset_board(int size) {
    FastBoard::reset_board(size);
    
    calc_hash();
    calc_ko_hash();    
}