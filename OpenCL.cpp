#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "Utils.h"
#include "OpenCL.h"

using namespace Utils;

OpenCL* OpenCL::s_OpenCL = nullptr;

OpenCL * OpenCL::get_OpenCL(void) {
    if (!s_OpenCL) {
        s_OpenCL = new OpenCL();
        s_OpenCL->initialize();
    }
    return s_OpenCL;
}

template<class T>
static std::string opencl_dev_type_to_string(T type) {
    if (type == CL_DEVICE_TYPE_CPU) {
        return "CPU";
    } else if (type == CL_DEVICE_TYPE_GPU) {
        return "GPU";
    } else {
        return "R U SERIOUS?";
    }
}

static std::string trim(std::string trim_me) {
    boost::algorithm::trim(trim_me);
    return trim_me;
}

void OpenCL::initialize(void) {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    float bestversion = 0.0f;
    cl::Platform bestplatform;
    cl::Device bestdevice;
    bool found_device = false;

    for (auto &p : platforms) {
        std::string platvers = p.getInfo<CL_PLATFORM_VERSION>();
        std::string platprof = p.getInfo<CL_PLATFORM_PROFILE>();
        std::string platname = p.getInfo<CL_PLATFORM_NAME>();
        std::string platvend = p.getInfo<CL_PLATFORM_VENDOR>();
        std::cerr << "Platform version: " << platvers << std::endl;
        std::cerr << "Platform profile: " << platprof << std::endl;
        std::cerr << "Platform name:    " << platname << std::endl;
        std::cerr << "Platform vendor:  " << platvend << std::endl;

        std::istringstream versstream(platvers);
        std::string tmp;
        float opencl_version;
        versstream >> tmp >> opencl_version;

        std::vector<cl::Device> devices;
        p.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        for (auto &d : devices) {
            std::cerr << "Device name:   "
                      << trim(d.getInfo<CL_DEVICE_NAME>())
                      << std::endl;
            std::cerr << "Device type:   "
                      << opencl_dev_type_to_string(d.getInfo<CL_DEVICE_TYPE>())
                      << std::endl;
            std::cerr << "Device vendor: "
                      << d.getInfo<CL_DEVICE_VENDOR>() << std::endl;
            std::cerr << "Device driver: "
                      << d.getInfo<CL_DRIVER_VERSION>() << std::endl;
            std::cerr << "Device speed:  "
                      << d.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>()
                      << " Mhz" << std::endl;
            std::cerr << "Device cores:  "
                      << d.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>()
                      << " CU" << std::endl;

            if (d.getInfo<CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_GPU) {
                if (opencl_version > bestversion) {
                    bestversion = opencl_version;
                    bestplatform = p;
                    bestdevice = d;
                    found_device = true;
                }
            }
        }
    }

    if (!found_device) {
        return;
    }

    cl::Platform newP = cl::Platform::setDefault(bestplatform);
    if (newP != bestplatform) {
        std::cerr << "Error setting default platform.";
        return;
    } else {
        std::cerr << "Selected " << bestplatform.getInfo<CL_PLATFORM_NAME>()
                  << std::endl;
        std::cerr << "Selected " << bestdevice.getInfo<CL_DEVICE_NAME>()
                  << std::endl;
    }

    //cl::Platform newP = cl::Platform::setDefault(plat);
    //if (newP != plat) {
    //    std::cout << "Error setting default platform.";
    //   return -1;
    //}

}
