#ifndef OPENCL_H_INCLUDED
#define OPENCL_H_INCLUDED

#include "config.h"

#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION  200
#define CL_HPP_ENABLE_EXCEPTIONS
#include <CL/cl2.hpp>

class OpenCL {
public:
    static OpenCL* get_OpenCL(void);

private:
    void initialize();

    static OpenCL* s_OpenCL;
};

#endif
