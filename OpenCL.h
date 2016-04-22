#ifndef OPENCL_H_INCLUDED
#define OPENCL_H_INCLUDED

#include "config.h"

#define CL_HPP_MINIMUM_OPENCL_VERSION   110
#define CL_HPP_TARGET_OPENCL_VERSION    120
#define CL_HPP_ENABLE_EXCEPTIONS
#include <CL/cl2.hpp>

class OpenCL {
public:
    static OpenCL* get_OpenCL(void);
    void convolve(int filter_size, int channels, int outputs,
                  float * input, float * output,
                  float * weights, float * biases);

private:
    OpenCL();
    void initialize();

    static OpenCL* s_OpenCL;

    cl::Kernel m_convolve_kernel;
    cl::Kernel m_clear_and_bias_kernel;
    cl::Kernel m_merge_kernel;
    bool m_init_ok{false};
};

#endif
