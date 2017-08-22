#ifndef MATCHER_H_INCLUDED
#define MATCHER_H_INCLUDED

#include <array>
#include <vector>

class Matcher {
public:
    int matches(int color, int pattern) const;

    /*
        return the "global" matcher
    */
    static Matcher* get_Matcher(void);

private:
    Matcher();
    int PatHashG(uint32 pattern) const;
    int PatHashV(uint32 d, uint32 pattern) const;
    int PatIndex(uint32 pattern) const;

    static constexpr size_t G_SIZE = 8192;
    static constexpr size_t V_SIZE = 1 << 18;
    static const std::array<int, G_SIZE> G;
    std::array<std::vector<unsigned short>, 2> m_patterns;
};

#endif
