#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include <vector>
#include <functional>
#include <boost/thread.hpp>

#include "config.h"
#include "FastState.h"
#include "Playout.h"
#include "UCTNode.h"
#include "UCTSearch.h"
#include "Utils.h"
#include "Matcher.h"

using namespace Utils;

UCTNode::UCTNode(int vertex, float score) 
 : m_firstchild(NULL), m_move(vertex), m_score(score),
   m_blackwins(0.0f), m_visits(0),
   m_ravevisits(50), m_ravestmwins(25.0f) {
}

UCTNode::~UCTNode() {    
    boost::mutex::scoped_lock lock(get_mutex());    
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

boost::mutex & UCTNode::get_mutex() {
    return m_nodemutex;
}

int UCTNode::create_children(FastState & state) {   
    // acquire the lock
    boost::mutex::scoped_lock lock(get_mutex());
    // check whether somebody beat us to it
    if (has_children()) {
        return 0;
    }                      
    
    FastBoard & board = state.board;      
    
    typedef std::pair<float, UCTNode*> scored_node; 
    std::vector<scored_node> nodelist;        
    
    std::vector<int> territory = state.board.influence();
    std::vector<int> moyo = state.board.moyo();

    if (state.get_passes() < 2) {             
        for (int i = 0; i < board.m_empty_cnt; i++) {  
            int vertex = board.m_empty[i];  
            
            assert(board.get_square(vertex) == FastBoard::EMPTY);             
            
            // add and score a node        
            if (vertex != state.komove && board.no_eye_fill(vertex)) {
                if (!board.is_suicide(vertex, board.m_tomove)) {                                          
                    float score = state.score_move(territory, moyo, vertex);
                    UCTNode * vtx = new UCTNode(vertex, score);
                    nodelist.push_back(std::make_pair(score, vtx));                    
                } 
            }                                           
        }      
        
        UCTNode * vtx = new UCTNode(FastBoard::PASS, 0.0f);        
        nodelist.push_back(std::make_pair(0.0f, vtx));        
    }    
    
    // sort (this will reverse scores, but linking is backwards too)
    std::sort(nodelist.begin(), nodelist.end());        
    
    // link the nodes together, we only really link the last few
    std::vector<scored_node>::const_iterator it; 
    const int maxchilds = 35;   // about 4M visits
    int childrenseen = 0;
    int childrenadded = 0;
    int totalchildren = nodelist.size();
    
    for (it = nodelist.begin(); it != nodelist.end(); ++it) {        
        if (totalchildren - childrenseen <= maxchilds) {
            link_child((*it).second);
            childrenadded++;
        }
        childrenseen++;
    }    

    return childrenadded;
}

int UCTNode::get_move() const {
    return m_move;
}

void UCTNode::set_move(int move) {
    m_move = move;
}

void UCTNode::update(Playout & gameresult, int color) {   
    boost::mutex::scoped_lock lock(get_mutex());
    m_visits++;    
    m_ravevisits++;
    
    // prefer winning with more territory    
    float score = gameresult.get_score();
           
    m_blackwins += 0.05f * score; 
    
    if (score > 0.0f) {
        m_blackwins += 1.0f;
    }        
    
    if (color == FastBoard::WHITE) {
        if (score < 0.0f) {
            m_ravestmwins += 1.0f + 0.05f * -score;
        }
    } else if (color == FastBoard::BLACK) {
        if (score > 0.0f) {
            m_ravestmwins += 1.0f + 0.05f * score;
        }
    }
}

// terminal node
void UCTNode::finalize(float gameresult) {
    boost::mutex::scoped_lock lock(get_mutex());
    m_visits += 100;
    
    m_blackwins += 5.0f * gameresult;
    
    if (gameresult > 0.0f) {
        m_blackwins += 100.0f;
    }
}

bool UCTNode::has_children() const {    
    return m_firstchild != NULL;
}

float UCTNode::get_blackwins() const {
    return m_blackwins;
}

void UCTNode::set_visits(int visits) {    
    boost::mutex::scoped_lock lock(get_mutex());
    m_visits = visits;
}

void UCTNode::set_blackwins(float wins) {
    boost::mutex::scoped_lock lock(get_mutex());
    m_blackwins = wins;
}

float UCTNode::get_score() {
    return m_score;
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
    float rate = m_ravestmwins / m_ravevisits;        
    
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
        
    //int childbound = std::max(1, (int)(((logf((float)get_visits()) - 3.6888f) / 0.1823216f) - 0.5f));
    int childbound = std::max(1, (int)(((logf((float)get_visits()) - 3.6888f) / 0.336472f) - 0.5f));
        
    int rave_parentvisits = 0;
    int parentvisits      = 1;   // avoid logparent being illegal
    
    int childcount = 0;
    UCTNode * c = m_firstchild;        
    while (c != NULL && childcount < childbound) {        
        parentvisits      += c->get_visits();
        rave_parentvisits += c->get_ravevisits();                     
        c = c->m_nextsibling;           
        childcount++;
    }
    
    float logparent     = logf((float)parentvisits);    
    float lograveparent = logf((float)rave_parentvisits); 
    
    // Mixing
    const float k = 5000.0f;
    float beta = sqrtf(k / ((3.0f * parentvisits) + k));     
        
    childcount = 0;
    UCTNode * child = m_firstchild;            
    
    while (child != NULL && childcount < childbound) {
        float value;
        float uctvalue;                

        if (child->get_ravevisits() > 0) {        
            if (!child->first_visit()) {
                // UCT part
                float winrate   = child->get_winrate(color);     
                float childrate = logparent / child->get_visits();            
                            
                float var = winrate - (winrate * winrate) + sqrtf(2.0f * childrate);            

                float uncertain = std::max(0.0f, std::min(0.25f, var));
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
                            
            float raveuncertain = std::max(0.0f, std::min(0.25f, ravevar));
            float rave = 0.8f * sqrtf(ravechildrate * raveuncertain);
            
            float ravevalue = ravewinrate + rave;                                              
                                   
            value = beta * ravevalue + (1.0f - beta) * uctvalue;
            
            assert(value > -1000.0f);
        } else {
            value = 1.1f;
        }                
        
        if (value > best_value) {
            best_value = value;
            best = child;
        }
        
        child = child->m_nextsibling;     
        childcount++;
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
        //if (a->get_winrate(m_color) == b->get_winrate(m_color)) {
            if (a->get_visits() > b->get_visits()) {
                return true;
            } else {
                return false;
            }
        //} else if (a->get_winrate(m_color) > b->get_winrate(m_color)) {
        //    return true;
        //} else {
        //    return false;
        //} 
    }
};

/*
    sort children by converting linked list to vector,
    sorting the vector, and reconstructing to linked list again
*/        
void UCTNode::sort_children(int color) {
    boost::mutex::scoped_lock lock(get_mutex());
    std::vector<UCTNode*> tmp;
    
    UCTNode * child = m_firstchild;    
    
    while (child != NULL) {        
        tmp.push_back(child);
                        
        child = child->m_nextsibling;       
    }        
    
    // reverse sort, because list reconstruction is backwards
    std::sort(tmp.begin(), tmp.end(), NodeComp(color));        
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
    boost::mutex::scoped_lock lock(get_mutex());  
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
void UCTNode::updateRAVE(Playout & playout, int color) {      
    float score = playout.get_score();            
    
    // siblings
    UCTNode * child = m_firstchild;    
    
    while (child != NULL) {                
        int move = child->get_move();                
        
        if (color == FastBoard::BLACK) {
            bool bpass = playout.passthrough(FastBoard::BLACK, move);        
            
            if (bpass) { 
                boost::mutex::scoped_lock lock(child->get_mutex());
                child->m_ravevisits++;
                        
                if (score > 0.0f) {
                    child->m_ravestmwins += 1.0f + 0.05f * score;
                } 
            }        
        } else {
            bool wpass = playout.passthrough(FastBoard::WHITE, move);        
            
            if (wpass) { 
                boost::mutex::scoped_lock lock(child->get_mutex());
                child->m_ravevisits++;
                        
                if (score < 0.0f) {
                    child->m_ravestmwins += 1.0f + 0.05f * -score;
                } 
            }
        }
                        
        child = child->m_nextsibling;       
    }      
}