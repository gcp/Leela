#ifndef FASTBOARD_H_INCLUDED
#define FASTBOARD_H_INCLUDED

#include <string>
#include <vector>
#include <queue>
#include <boost/tr1/array.hpp>

class FastBoard {
public:
    /* 
        neighbor counts are up to 4, so 3 bits is ok,
        but a power of 2 makes things a bit faster 
    */
    static const int NBR_SHIFT = 4;   
        
    /*
        largest board supported 
    */
    static const int MAXBOARDSIZE = 19;

    /*
        highest existing square        
    */            
    static const int MAXSQ = ((MAXBOARDSIZE + 2) * (MAXBOARDSIZE + 2));    

    /*
        infinite score
    */        
    static const int BIG = 10000000;

    /*
        vertex of a pass
    */
    static const int PASS = -1;

    /*
        possible contents of a square
    */        
    typedef enum { 
        BLACK = 0, WHITE = 1, EMPTY = 2, INVAL = 3
    } square_t;                                                          
    
    int get_boardsize(void);
    square_t get_square(int x, int y);
    square_t get_square(int vertex); 
    int get_vertex(int i, int j);
    void set_square(int x, int y, square_t content);
    void set_square(int vertex, square_t content);  
        
    bool is_suicide(int i, int color);
    int fast_ss_suicide(const int color, const int i);
    int update_board_fast(const int color, const int i);    
    void play_critical_neighbours(int color, int vertex, std::vector<int> & work);
    void save_critical_neighbours(int color, int vertex, std::vector<int> & work);
    void add_pattern_moves(int color, int vertex, std::vector<int> & work);
    void add_global_captures(int color, std::vector<int> & work);
    void add_near_captures(int color, int vertex, std::vector<int> & work);
    
    bool self_atari(int color, int vertex);
    int get_dir(int i);

    bool no_eye_fill(const int i);
    
    int estimate_mc_score(float komi = 7.5f);    
    float final_mc_score(float komi = 7.5f);        
    float area_score(float komi = 7.5f);
    
    int eval(float komi); 
    int get_prisoners(int side);
    bool black_to_move();
        
    std::string move_to_text(int move);
    std::string get_string(int vertex);   
    std::string get_stone_list(); 
    
    void reset_board(int size);                    
    void display_influence(void);            
    void display_liberties(int lastmove = -1);
    void display_board(int lastmove = -1);    

    static bool starpoint(int size, int point);
    static bool starpoint(int size, int x, int y);                           
            
    std::tr1::array<int, MAXSQ> m_empty;       /* empty squares */
    std::tr1::array<int, MAXSQ> m_empty_idx;   /* indexes of square */
    int m_empty_cnt;                           /* count of empties */
     
    int m_tomove;       
    int m_maxsq;       
    
protected:
    /*
        bit masks to detect eyes on neighbors
    */        
    static const std::tr1::array<int, 2> s_eyemask; 
    
    std::tr1::array<square_t, MAXSQ> m_square;      /* board contents */            
    std::tr1::array<int, MAXSQ+1>    m_next;        /* next stone in string */ 
    std::tr1::array<int, MAXSQ+1>    m_parent;      /* parent node of string */        
    std::tr1::array<int, MAXSQ+1>    m_plibs;       /* pseudoliberties per string parent */        
    std::tr1::array<int, MAXSQ>      m_neighbours;  /* counts of neighboring stones */  
    std::tr1::array<int, 4>          m_dirs;        /* movement directions 4 way */
    std::tr1::array<int, 4>          m_alterdirs;   /* to change movement direction */
    std::tr1::array<int, 8>          m_extradirs;   /* movement directions 8 way */
    std::tr1::array<int, 2>          m_prisoners;   /* prisoners per color */
    std::tr1::array<int, 2>          m_stones;      /* stones per color */             
    std::queue<int>                  m_critical;    /* queue of critical points */

    int m_boardsize;    
    
    int count_liberties(const int i); 
    int count_neighbours(const int color, const int i);   
    void merge_strings(const int ip, const int aip);    
    int remove_string_fast(int i);        
    void add_neighbour(const int i, const int color);
    void remove_neighbour(const int i, const int color);        
    int update_board_eye(const int color, const int i);
    std::vector<bool> calc_reach_color(int col);    
    void run_bouzy(int *influence, int dilat, int eros);  
    bool kill_or_connect(int color, int vertex);  
    int in_atari(int vertex);
    void add_string_liberties(int vertex, 
                              std::tr1::array<int, 3> & nbr_libs, 
                              int & nbr_libs_cnt);
    void kill_neighbours(int vertex, std::vector<int> & work);                              
    bool match_pattern(int color, int vertex);
    void try_capture(int color, int vertex, std::vector<int> & work);
};

#endif