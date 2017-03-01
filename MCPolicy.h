#ifndef MCPOLICY_H_INCLUDED
#define MCPOLICY_H_INCLUDED

#include "config.h"

#include <array>
#include <vector>

constexpr int MWF_FLAG_SAVING  = 1;
constexpr int MWF_FLAG_CAPTURE = 1 << 1;
constexpr int MWF_FLAG_NAKADE  = 1 << 2;
constexpr int MWF_FLAG_PATTERN = 1 << 3;
constexpr int MWF_FLAG_SA      = 1 << 4;
constexpr int MWF_FLAG_RANDOM  = 1 << 5;
constexpr int MWF_FLAG_PASS    = 1 << 6;

class MovewFeatures {
public:
    MovewFeatures(int vertex, int flags = 0)
        : m_vertex(vertex),
          m_flags(flags),
          m_target_size(0) {
    }
    void set_target_size(int x) {
        m_target_size = x;
    }
    int get_sq() {
        return m_vertex;
    }
    int get_flags() {
        return m_flags;
    }
    int get_target_size() {
        return m_target_size;
    }
private:
    // This is the actual move.
    int m_vertex;
    // Attributes/Features
    int m_flags;
    int m_target_size;
};

class MCPolicy {
public:
    static void mse_from_file(std::string filename);
};

#endif