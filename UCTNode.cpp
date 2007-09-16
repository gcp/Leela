#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include <vector>
#include <functional>

#include "config.h"
#include "FastState.h"
#include "Playout.h"
#include "UCTNode.h"
#include "UCTSearch.h"
#include "Utils.h"

using namespace Utils;

UCTNode::UCTNode(int vertex) 
: m_firstchild(NULL), m_move(vertex), 
  m_blackwins(0.0f), m_visits(0),
  m_raveblackwins(25.0f), m_ravevisits(50) {
}

UCTNode::~UCTNode() {
    UCTNode * next = m_firstchild;
    
    while (next != NULL) {
        UCTNode * tmp = next->m_nextsibling;           
        delete next;                    
        next = tmp;
    }
}

bool UCTNode::first_visit() const {        
    return m_visits == 0;
}

void UCTNode::link_child(UCTNode * newchild) {
    newchild->m_nextsibling = m_firstchild;
    m_firstchild = newchild;            
}

int UCTNode::create_children(FastState &state) {             
    FastBoard & board = state.board;  
    int children = 0;

    if (state.get_passes() < 2) {         
        for (int i = 0; i < board.m_empty_cnt; i++) {  
            int vertex = board.m_empty[i];  
            
            assert(board.get_square(vertex) == FastBoard::EMPTY);             
                    
            if (vertex != state.komove && board.no_eye_fill(vertex)) {
                if (!board.is_suicide(vertex, board.m_tomove)) {  
                    UCTNode * vtx = new UCTNode(vertex);
                    link_child(vtx);                    
                    children++;                                        
                } 
            }                   
        }      
        
        UCTNode * vtx = new UCTNode(FastBoard::PASS);        
        link_child(vtx);
        children++;
    }    

    return children;
}

int UCTNode::get_move() const {
    return m_move;
}

void UCTNode::set_move(int move) {
    m_move = move;
}

void UCTNode::update(Playout & gameresult) {        
    m_visits++;
    m_ravevisits++;
    
    float result = (gameresult.get_score() > 0.0f);
    m_blackwins     += result;       
    m_raveblackwins += result;
    
}

// terminal node
void UCTNode::finalize(float gameresult) {
    m_visits += 100;
    
    float result = (gameresult > 0.0f);
    m_blackwins += 100.0f * result;
}

bool UCTNode::has_children() const {    
    return m_firstchild != NULL;
}

float UCTNode::get_blackwins() const {
    return m_blackwins;
}

void UCTNode::set_visits(int visits) {
    m_visits = visits;
}

void UCTNode::set_blackwins(float wins) {
    m_blackwins = wins;
}

float UCTNode::get_winrate(int tomove) const {    
    assert(!first_visit());

    float rate = get_blackwins() / get_visits();
    
    if (tomove == FastBoard::WHITE) {
        rate = 1.0f - rate;
    }
    
    return rate;
}

float UCTNode::get_raverate(int tomove) const {
    float rate = m_raveblackwins / m_ravevisits;
    
    if (tomove == FastBoard::WHITE) {
        rate = 1.0f - rate;
    }
    
    return rate;
}

int UCTNode::get_visits() const {        
    return m_visits;
}

int UCTNode::get_ravevisits() const {
    return m_ravevisits;
}

UCTNode* UCTNode::uct_select_child(int color) {                                   
    UCTNode * best = NULL;    
    float best_value = -1000.0f;                                
    
    float rave_parentvisits = (float)get_ravevisits();
    
    float logparent     = logf((float)(get_visits() - UCTSearch::MATURE_TRESHOLD));    
    float lograveparent = logf(rave_parentvisits); 
    
    // Mixing
    float beta = sqrtf(1000.0f / ((3.0f * rave_parentvisits) + 1000.0f));     
        
    UCTNode * child = m_firstchild;        
    while (child != NULL) {
        float value;
        float uctvalue;

        if (child->get_ravevisits() > 0) {        
            if (!child->first_visit()) {
                // UCT part
                float winrate   = child->get_winrate(color);     
                float childrate = logparent / child->get_visits();            
                            
                float var = winrate - (winrate * winrate) + sqrtf(2.0f * childrate);            

                float uncertain = min(0.25f, var);
                float uct = 0.8f * sqrtf(childrate * uncertain);
                
                uctvalue = winrate + uct;
            } else {
                uctvalue = 1.1f;
            }                                                    
            
            // RAVE part
            float ravewinrate = child->get_raverate(color);
            float ravechildrate = lograveparent / child->get_ravevisits();
            
            float ravevar = ravewinrate - (ravewinrate * ravewinrate) 
                            + sqrtf(2.0f * ravechildrate);
                            
            float raveuncertain = min(0.25f, ravevar);                            
            float rave = 0.8f * sqrtf(ravechildrate * raveuncertain);
            
            float ravevalue = ravewinrate + rave;                                              
                                   
            value = beta * ravevalue + (1.0f - beta) * uctvalue;
        } else {
            value = 1.1f;
        }
        
        if (value > best_value) {
            best_value = value;
            best = child;
        }
        
        child = child->m_nextsibling;
    }   
    
    assert(best != NULL);         
    
    return best;
}

class NodeComp : public std::binary_function<UCTNode*, UCTNode*, bool> {   
private:
    const int m_color;
public:   
    NodeComp(const int color) : m_color(color) {}
   
    bool operator()(const UCTNode * a, const UCTNode * b) {  
        if (a->first_visit() && !b->first_visit()) {
            return false;
        }  
        if (b->first_visit() && !a->first_visit()) {
            return true;
        }
        if (a->first_visit() && b->first_visit()) {
            return false;
        }               
        if (a->get_winrate(m_color) == b->get_winrate(m_color)) {
            if (a->get_visits() > b->get_visits()) {
                return true;
            } else {
                return false;
            }
        } else if (a->get_winrate(m_color) > b->get_winrate(m_color)) {
            return true;
        } else {
            return false;
        }        
    }
};

/*
    sort children by converting linked list to vector,
    sorting the vector, and reconstructing to linked list again
*/        
void UCTNode::sort_children(int color) {
    std::vector<UCTNode*> tmp;
    
    UCTNode * child = m_firstchild;    
    
    while (child != NULL) {        
        tmp.push_back(child);
                        
        child = child->m_nextsibling;       
    }        
    
    // reverse sort, because list reconstruction is backwards
    std::stable_sort(tmp.begin(), tmp.end(), NodeComp(color));        
    std::reverse(tmp.begin(), tmp.end());
    
    m_firstchild = NULL;
    
    std::vector<UCTNode*>::iterator it;
    
    for (it = tmp.begin(); it != tmp.end(); ++it) {
        link_child(*it);   
    } 
}

UCTNode* UCTNode::get_first_child() {
    return m_firstchild;
}

UCTNode* UCTNode::get_sibling() {
    return m_nextsibling;
}

UCTNode* UCTNode::get_pass_child() {
    UCTNode * child = m_firstchild;    
    
    while (child != NULL) {        
        if (child->m_move == FastBoard::PASS) {
            return child;
        }
                        
        child = child->m_nextsibling;       
    }      
    
    assert(FALSE && "No pass child");
    
    return NULL;  
}

UCTNode* UCTNode::get_nopass_child() {
    UCTNode * child = m_firstchild;    
    
    while (child != NULL) {        
        if (child->m_move != FastBoard::PASS) {
            return child;
        }
                        
        child = child->m_nextsibling;       
    }              
    
    return NULL;  
}

void UCTNode::delete_child(UCTNode * del_child) {    
    assert(del_child != NULL);
    
    if (del_child == m_firstchild) {        
        m_firstchild = m_firstchild->m_nextsibling;
        return;
    } else {
        UCTNode * child = m_firstchild;    
        UCTNode * prev  = NULL;
    
        do {
            prev  = child;
            child = child->m_nextsibling;                       
            
            if (child == del_child) {
                UCTNode * next = child->m_nextsibling;
                delete child;
                prev->m_nextsibling = next;
                return;
            }                                    
        } while (child != NULL);     
    }
    
    assert(0 && "Child to delete not found");           
}

// update siblings with matching RAVE info
void UCTNode::updateRAVE(Playout & playout) {    
    float score = playout.get_score();            
    
    // siblings
    UCTNode * child = m_firstchild;    
    
    while (child != NULL) {        
        int move = child->get_move();                
        
        bool bpass = playout.passthrough(FastBoard::BLACK, move);        
        
        if (bpass) { 
            child->m_ravevisits++;
                    
            if (score > 0.0f) {
                child->m_raveblackwins += 1.0f;
            } 
        }        
                        
        child = child->m_nextsibling;       
    }      
}