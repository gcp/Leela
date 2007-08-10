#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "config.h"
#include "FastState.h"
#include "Playout.h"
#include "UCTNode.h"


bool UCTNode::first_visit() {
    return m_visits == 0;
}

float UCTNode::do_one_playout(FastState &startstate) {
    Playout p(startstate);

    p.run();

    return p.get_score();
}

void UCTNode::create_children(FastState &state) {
}