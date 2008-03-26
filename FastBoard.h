#ifndef FASTBOARD_H_INCLUDED
#define FASTBOARD_H_INCLUDED

#include "config.h"

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
#ifdef LITEVERSION    
    static const int MAXBOARDSIZE = 13;
#else
    static const int MAXBOARDSIZE = 19;
#endif    

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
    static const int PASS   = -1;
    /*
        vertex of a "resign move"
    */        
    static const int RESIGN = -2;

    /*
        possible contents of a square
    */        
    enum square_t : unsigned char { 
        BLACK = 0, WHITE = 1, EMPTY = 2, INVAL = 3
    };

    /*
        move generation types
    */
    typedef std::tr1::array<int, 24> movelist_t;
    typedef std::pair<int, float> movescore_t;
    typedef std::tr1::array<movescore_t, 24> scoredlist_t;
    
    int get_boardsize(void);
    square_t get_square(int x, int y);
    square_t get_square(int vertex); 
    int get_vertex(int i, int j);
    void set_square(int x, int y, square_t content);
    void set_square(int vertex, square_t content); 
    std::pair<int, int> get_xy(int vertex);
    int get_groupid(int vertex);
        
    bool is_suicide(int i, int color);
    int fast_ss_suicide(const int color, const int i);
    int update_board_fast(const int color, const int i);            
    void save_critical_neighbours(int color, int vertex, movelist_t & moves, int & movecnt);
    void add_pattern_moves(int color, int vertex, movelist_t & moves, int & movecnt);    
    void add_global_captures(int color, movelist_t & moves, int & movecnt);             
    int capture_size(int color, int vertex);
    int saving_size(int color, int vertex);
    int minimum_elib_count(int color, int vertex);
    std::pair<int, int> nbr_criticality(int color, int vertex);
    int count_pliberties(const int i);
    int count_rliberties(const int i);
    bool check_losing_ladder(const int color, const int vtx, int branching = 0);
    bool is_connecting(const int color, const int vertex);
    int nbr_weight(const int color, const int vertex);
    int merged_string_size(int color, int vertex);
    std::vector<int> get_neighbour_ids(int vertex);
    void augment_chain(std::vector<int> & chains, int vertex);
    std::vector<int> get_augmented_string(int vertex);
    std::vector<int> dilate_liberties(std::vector<int> & vtxlist);
    std::vector<int> get_nearby_enemies(std::vector<int> & vtxlist);
        
    bool self_atari(int color, int vertex);    
    int get_dir(int i);
    int get_extra_dir(int i);

    bool is_eye(const int color, const int vtx);    
    bool no_eye_fill(const int i);
    int get_pattern_fast(const int sq);    
    int get_pattern_fast_augment(const int sq); 
    int get_pattern3(const int sq, bool invert);   
    int get_pattern3_augment(const int sq, bool invert);
    int get_pattern3_augment_spec(const int sq, int libspec, bool invert);
    int get_pattern4(const int sq, bool invert);
    uint64 get_pattern5(const int sq, bool invert, bool extend);
    
    int estimate_mc_score(float komi = 7.5f);    
    float final_mc_score(float komi = 7.5f);        
    float area_score(float komi = 7.5f);
    float percentual_area_score(float komi = 7.5f);
    std::vector<bool> calc_reach_color(int col);        
    std::vector<int> influence(void);
    std::vector<int> moyo(void);
    std::vector<int> area(void);
    int predict_is_alive(const int move, const int vertex);    
    bool predict_kill(const int move, const int groupid);

    int eval(float komi); 
    int get_prisoners(int side);
    int get_empty();
    bool black_to_move();
    int get_to_move();
        
    std::string move_to_text(int move);
    std::string move_to_text_sgf(int move);    
    std::string get_stone_list(); 
    int string_size(int vertex);
    std::vector<int> get_string_stones(int vertex);    
    std::string get_string(int vertex);
    
    void reset_board(int size);                    
    void display_map(std::vector<int> influence);            
    void display_liberties(int lastmove = -1);
    void display_board(int lastmove = -1);    

    static bool starpoint(int size, int point);
    static bool starpoint(int size, int x, int y);                           
            
    std::tr1::array<unsigned short, MAXSQ> m_empty;       /* empty squares */
    std::tr1::array<unsigned short, MAXSQ> m_empty_idx;   /* indexes of square */
    int m_empty_cnt;                                      /* count of empties */
     
    int m_tomove;       
    int m_maxsq;       
    
protected:
    /*
        bit masks to detect eyes on neighbors
    */        
    static const std::tr1::array<int,      2> s_eyemask; 
    static const std::tr1::array<square_t, 4> s_cinvert; /* color inversion */
    
    std::tr1::array<square_t,  MAXSQ>           m_square;      /* board contents */            
    std::tr1::array<unsigned short, MAXSQ+1>    m_next;        /* next stone in string */ 
    std::tr1::array<unsigned short, MAXSQ+1>    m_parent;      /* parent node of string */            
    std::tr1::array<unsigned short, MAXSQ+1>    m_libs;        /* liberties per string parent */        
    std::tr1::array<unsigned short, MAXSQ+1>    m_stones;      /* stones per string parent */        
    std::tr1::array<unsigned short, MAXSQ>      m_neighbours;  /* counts of neighboring stones */       
    std::tr1::array<int, 4>          m_dirs;        /* movement directions 4 way */    
    std::tr1::array<int, 8>          m_extradirs;   /* movement directions 8 way */
    std::tr1::array<int, 2>          m_prisoners;   /* prisoners per color */
    std::tr1::array<int, 2>          m_totalstones; /* stones per color */                 
    std::vector<int>                 m_critical;    /* queue of critical points */    

    int m_boardsize;    
        
    int count_neighbours(const int color, const int i);   
    void merge_strings(const int ip, const int aip);    
    int remove_string_fast(int i);        
    void add_neighbour(const int i, const int color);
    void remove_neighbour(const int i, const int color);        
    int update_board_eye(const int color, const int i);    
    std::vector<int> run_bouzy(int dilat, int eros);  
    bool kill_or_connect(int color, int vertex);  
    int in_atari(int vertex);
    bool fast_in_atari(int vertex);
    template <int N> void add_string_liberties(int vertex, 
                                               std::tr1::array<int, N> & nbr_libs, 
                                               int & nbr_libs_cnt);
    void kill_neighbours(int vertex, movelist_t & moves, int & movecnt);                                  
    void try_capture(int color, int vertex, movelist_t & moves, int & movecnt);    
    FastBoard remove_dead();
    bool predict_solid_eye(const int move, const int color, const int vtx);
};

#endif
