#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <fstream>
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

OpenCL::OpenCL() {
}

void OpenCL::convolve(int filter_size, int channels, int outputs,
                      float * input,
                      float * output,
                      float * weights,
                      float * biases) {
    // fixed for 19x19
    constexpr int width = 19;
    constexpr int height = 19;
    unsigned int filter_len = filter_size * filter_size;

    size_t inSize = width * height * channels * sizeof(float);
    size_t outSize = width * height * outputs * sizeof(float);
    size_t weightSize = filter_len * channels * outputs * sizeof(float);
    size_t biasSize = outputs * sizeof(float);

    // Produce channel * output planes and merge them at the end
    size_t mergeSize = channels * outSize;

    cl::Buffer bufferInput   = cl::Buffer(CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                                          inSize, input);
    cl::Buffer bufferOutput  = cl::Buffer(CL_MEM_WRITE_ONLY, outSize);
    cl::Buffer bufferMerge   = cl::Buffer(CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS,
                                          mergeSize);

    cl::Buffer bufferWeights = cl::Buffer(CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                                          weightSize, weights);
    cl::Buffer bufferBiases  = cl::Buffer(CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                                          biasSize, biases);

    cl::CommandQueue queue = cl::CommandQueue::getDefault();

    m_convolve_kernel.setArg(0, bufferInput);
    m_convolve_kernel.setArg(1, bufferMerge);
    m_convolve_kernel.setArg(2, bufferWeights);
    m_convolve_kernel.setArg(3, filter_size);

    size_t workgroup_size = std::min(outputs, 32);

    try {
        queue.enqueueNDRangeKernel(m_convolve_kernel, cl::NullRange,
                                   cl::NDRange(channels, outputs),
                                   cl::NullRange
                                   /*cl::NDRange(2, workgroup_size)*/);
    } catch (cl::Error &e) {
        std::cerr << "Error in convolve: " << e.what() << ": "
                  << e.err() << std::endl;
        return;
    }

    m_merge_kernel.setArg(0, bufferMerge);
    m_merge_kernel.setArg(1, bufferOutput);
    m_merge_kernel.setArg(2, bufferBiases);
    m_merge_kernel.setArg(3, channels);

    try {
        queue.enqueueNDRangeKernel(m_merge_kernel, cl::NullRange,
                                   cl::NDRange(outputs),
                                   cl::NullRange
                                   /*cl::NDRange(1)*/);
    } catch (cl::Error &e) {
        std::cerr << "Error in merge: " << e.what() << ": "
                  << e.err() << std::endl;
        return;
    }

    queue.enqueueReadBuffer(bufferOutput, CL_FALSE, 0, outSize, output);
    queue.finish();
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
    try {
        cl::Platform::get(&platforms);
    } catch (cl::Error &e) {
        std::cerr << e.what() << std::endl;
        return;
    }

    float best_version = 0.0f;
    cl::Platform best_platform;
    cl::Device best_device;
    bool found_device = false;

    std::cerr << "Detected " << platforms.size()
              << " OpenCL platforms" << std::endl;

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
        try {
            p.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        } catch (cl::Error &e) {
            std::cerr << "Error getting device: " << e.what() << ": "
                      << e.err() << std::endl;
            devices.clear();
        }
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

            if (d.getInfo<CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_CPU) {
                if (opencl_version > best_version) {
                    best_version = opencl_version;
                    best_platform = p;
                    best_device = d;
                    found_device = true;
                }
            }
        }
    }

    if (!found_device) {
        return;
    }

    cl::Platform::setDefault(best_platform);
    std::cerr << "Selected " << best_platform.getInfo<CL_PLATFORM_NAME>()
              << std::endl;
    std::cerr << "Selected " << best_device.getInfo<CL_DEVICE_NAME>()
              << std::endl;
    std::cerr << "with OpenCL " << best_version << " capability" << std::endl;

    cl::Context context(best_device);
    cl::Context::setDefault(context);
    cl::Device::setDefault(best_device);

    cl::CommandQueue::setDefault(cl::CommandQueue(context, best_device));

    // Read source file
    std::ifstream sourceFile("convolve_kernel.cl", std::ifstream::in);
    std::string sourceCode(std::istreambuf_iterator<char>(sourceFile),
                           (std::istreambuf_iterator<char>()));

    // Make program of the source code in the context
    cl::Program program(sourceCode);

    // Build program for these specific devices
    try {
        program.build();
    } catch (cl::Error &e) {
        std::cerr << "Error building: " << std::endl
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl::Device::getDefault())
                  << std::endl;
        return;
    }

    // Make kernel
    m_convolve_kernel = cl::Kernel(program, "convolve");
    m_merge_kernel = cl::Kernel(program, "merge");

    m_init_ok = true;
}
