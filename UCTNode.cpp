#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "config.h"
#include "FastState.h"
#include "Playout.h"
#include "UCTNode.h"
#include "UCTSearch.h"

#include <vector>
#include <functional>

UCTNode::UCTNode(int vertex) 
: m_visits(0), m_blackwins(0.0f),
  m_firstchild(NULL), m_move(vertex) {
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

float UCTNode::do_one_playout(FastState &startstate) {
    Playout p;

    p.run(startstate);

    return p.get_score();
}

void UCTNode::link_child(UCTNode * newchild) {
    newchild->m_nextsibling = m_firstchild;
    m_firstchild = newchild;    
}

void UCTNode::create_children(FastState &state) {             
    FastBoard & board = state.board;        
        
    for (int i = 0; i < board.m_empty_cnt; i++) {  
        int vertex = board.m_empty[i];  
        
        assert(board.get_square(vertex) == FastBoard::EMPTY);             
                
        if (vertex != state.komove && board.no_eye_fill(vertex)) {
            if (!board.is_suicide(vertex, board.m_tomove)) {                                             
                link_child(new UCTNode(vertex));
            } 
        }                   
    }      

    if (state.get_passes() < 2) {
        link_child(new UCTNode(FastBoard::PASS));
    }    
}

int UCTNode::get_move() const {
    return m_move;
}

void UCTNode::set_move(int move) {
    m_move = move;
}

void UCTNode::update(float gameresult) {
    m_visits++;
    
    float result = (gameresult > 0.0f);
    m_blackwins +=  result;       
}

bool UCTNode::has_children() const {
    return m_firstchild != NULL;
}

float UCTNode::get_winrate(int tomove) const {
    assert(m_visits > 0);

    float rate = m_blackwins / m_visits;
    
    if (tomove == FastBoard::WHITE) {
        rate = 1.0f - rate;
    }
    
    return rate;
}

int UCTNode::get_visits() const {
    return m_visits;
}

UCTNode* UCTNode::uct_select_child(int color) {            
    UCTNode * best = NULL;
    UCTNode * child = m_firstchild;
    float best_uct = -1000.0f;        
        
    float logparent = logf(get_visits() - UCTSearch::MATURE_TRESHOLD);
        
    while (child != NULL) {
        float uctvalue;
                
        if (!child->first_visit()) {
            float winrate   = child->get_winrate(color);     
            float childrate = logparent / child->get_visits();            
            
            //faster formula for pure binomial
            float var = winrate - (winrate * winrate) + sqrtf(2.0f * childrate);            

            float uncertain = min(0.25f, var);
            float uct = 1.2f * sqrtf(childrate * uncertain);
            
            uctvalue = winrate + uct;            
        } else {
            uctvalue = 1.1f;
        }                        
        
        if (uctvalue > best_uct) {
            best_uct = uctvalue;
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
    
    std::vector<UCTNode*>::iterator it = tmp.begin();
    
    for (; it != tmp.end(); ++it) {
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
