#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "config.h"
#include "FastState.h"
#include "Playout.h"
#include "UCTNode.h"

#include <vector>

UCTNode::UCTNode() 
: m_visits(0), m_blackwins(0) {
}

bool UCTNode::first_visit() const {
    return m_visits == 0;
}

float UCTNode::do_one_playout(FastState &startstate) {
    Playout p;

    p.run(startstate);

    return p.get_score();
}

void UCTNode::create_children(FastState &state) {             
    FastBoard & board = state.board;
        
    for (int i = 0; i < board.m_empty_cnt; i++) {  
        int vertex = board.m_empty[i];               
                
        if (vertex != state.komove && board.no_eye_fill(vertex)) {
            if (!board.fast_ss_suicide(board.m_tomove, vertex)) {
                UCTNode node;
                node.set_move(vertex);
                m_children.push_back(node);                
            } 
        }                   
    }      

    if (state.get_passes() < 2) {
        UCTNode node;
        node.set_move(FastBoard::PASS);              
        m_children.push_back(node);  
    }  
}

int UCTNode::get_move() const {
    return m_move;
}

void UCTNode::set_move(int move) {
    m_move = move;
}

void UCTNode::add_visit() {
    m_visits++;
}

void UCTNode::update(float gameresult) {
    m_blackwins += (gameresult > 0.0f);
}

bool UCTNode::has_children() const {
    return m_children.size() > 0;
}

UCTNode::iterator UCTNode::begin() {
    return m_children.begin();
}

UCTNode::iterator UCTNode::end() {
    return m_children.end();
}

float UCTNode::get_winrate(int tomove) const {
    assert(m_visits > 0);

    float rate = (float)m_blackwins / (float)m_visits;
    
    if (tomove == FastBoard::WHITE) {
        rate = 1.0 - rate;
    }
    
    return rate;
}

int UCTNode::get_visits() const {
    return m_visits;
}