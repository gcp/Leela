#include "config.h"
#ifdef USE_OPENCL

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/tr1/array.hpp>

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

void OpenCL::add_weights(int layer,
                         size_t size,
                         float * weights) {
    if (layer >= m_layers.size()) {
        m_layers.push_back(Layer());
    }

    size_t weightSize = size * sizeof(float);

    cl::Buffer bufferWeights = cl::Buffer(CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                                          weightSize, weights);

    m_layers.back().weights.push_back(bufferWeights);
}

void OpenCL::forward(std::vector<float>& input,
                     std::vector<float>& output) {
    size_t inSize = sizeof(float) * input.size();
    size_t outSize = sizeof(float) * output.size();
    size_t finalSize = m_layers.back().outputs * 19 * 19 * sizeof(float);

    cl::Buffer inBuffer = cl::Buffer(CL_MEM_COPY_HOST_PTR | CL_MEM_READ_WRITE
                                     | CL_MEM_HOST_NO_ACCESS,
                                     inSize, &input[0]);
    cl::Buffer tmpBuffer = cl::Buffer(CL_MEM_READ_WRITE
                                      | CL_MEM_HOST_NO_ACCESS,
                                      outSize);
    cl::Buffer outBuffer = cl::Buffer(CL_MEM_WRITE_ONLY, finalSize);

    for (auto & layer : m_layers) {
        if (layer.is_batchnorm) {
           batchnorm(layer.outputs,
                     tmpBuffer,
                     inBuffer,
                     layer.weights);
        } else {
            // convolution
            convolve(layer.filter_size,
                     layer.channels,
                     layer.outputs,
                     inBuffer,
                     tmpBuffer,
                     layer.weights);
        }
    }
    cl::CommandQueue queue = cl::CommandQueue::getDefault();
    // last layer is always a convolution, so output is in tmp
    queue.enqueueCopyBuffer(tmpBuffer, outBuffer, 0, 0, finalSize);
    queue.enqueueReadBuffer(outBuffer, CL_FALSE, 0, finalSize, &output[0]);
    queue.finish();
}

void OpenCL::batchnorm(int outputs,
                       cl::Buffer & bufferInput,
                       cl::Buffer & bufferOutput,
                       std::vector<cl::Buffer>& weights) {
    // fixed for 19x19
    constexpr int width = 19;
    constexpr int height = 19;
    constexpr int boardsize = width * height;

    cl::CommandQueue queue = cl::CommandQueue::getDefault();

    m_batchnorm_kernel.setArg(0, bufferInput);
    m_batchnorm_kernel.setArg(1, bufferOutput);
    m_batchnorm_kernel.setArg(2, weights[0]);
    m_batchnorm_kernel.setArg(3, weights[1]);
    m_batchnorm_kernel.setArg(4, weights[2]);

    try {
        queue.enqueueNDRangeKernel(m_batchnorm_kernel, cl::NullRange,
                                   cl::NDRange(outputs, boardsize),
                                   cl::NullRange);
    } catch (cl::Error &e) {
        std::cerr << "Error in convolve: " << e.what() << ": "
            << e.err() << std::endl;
        return;
    }

}

void OpenCL::convolve(int filter_size, int channels, int outputs,
                      cl::Buffer& bufferInput,
                      cl::Buffer& bufferOutput,
                      std::vector<cl::Buffer>& weights) {
    // fixed for 19x19
    constexpr int width = 19;
    constexpr int height = 19;
    unsigned int filter_len = filter_size * filter_size;

    size_t inSize = width * height * channels * sizeof(float);

    // Every input channel is this big
    size_t chanSize = width * height * sizeof(float);

    size_t channelGroup;
    size_t outputGroup;
    size_t rowGroup;
    size_t waveFronts;

    cl::Kernel m_convolve_kernel;
    if (filter_size == 3) {
        m_convolve_kernel = m_convolve3_kernel;
    } else {
        m_convolve_kernel = m_convolve5_kernel;
    }

    // Workgroup things
    outputGroup = std::min(outputs, 32);
    /*int maxShift = (int)std::floor(std::log2(256 / outputGroup));
    int channelShift = 0;
    do {
        channelShift++;
        if (channelShift >= maxShift) break;
    } while (channels % (1 << (channelShift + 1)) == 0);
    channelGroup = 1 << channelShift;*/
    int channelShift;
    if (channels % 8 == 0) {
        channelGroup = 8;
        channelShift = 3;
    } else {
        channelGroup = 2;
        channelShift = 1;
    }
    rowGroup = 1;

    // Total output size after reducing
    size_t outSize = width * height * outputs * sizeof(float);

    // Produce channel * output planes and merge them at the end
    size_t mergeSize = (channels >> channelShift) * outSize;

    // Store the filters locally
    size_t filtSize = outputGroup * channelGroup * filter_len * sizeof(float);

    // Copy the rows locally
    size_t stripSize;
    if (filter_size == 3) {
        stripSize = filter_size * (width + (filter_size - 1)) * sizeof(float);
    } else {
        stripSize = filter_size * width * sizeof(float);
    }

    size_t rowSize;
    // Row buffer
    if (filter_size == 5) {
        rowSize = channelGroup * outputGroup * width * sizeof(float);
    } else {
        rowSize = channelGroup * outputGroup * (channelGroup+1) * sizeof(float);
    }
    cl::Buffer bufferMerge = cl::Buffer(CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS,
                                        mergeSize);

    cl::CommandQueue queue = cl::CommandQueue::getDefault();

    m_convolve_kernel.setArg(0, bufferInput);
    m_convolve_kernel.setArg(1, bufferMerge);
    m_convolve_kernel.setArg(2, weights[0]);
    m_convolve_kernel.setArg(3, cl::Local(stripSize * channelGroup * rowGroup));
    m_convolve_kernel.setArg(4, cl::Local(rowSize));
    m_convolve_kernel.setArg(5, channelShift);

    try {
        queue.enqueueNDRangeKernel(m_convolve_kernel, cl::NullRange,
                                   cl::NDRange(channels, outputs, 19),
                                   cl::NDRange(channelGroup, outputGroup, rowGroup));
    } catch (cl::Error &e) {
        std::cerr << "Error in convolve: " << e.what() << ": "
                  << e.err() << std::endl;
        return;
    }

    m_merge_kernel.setArg(0, bufferMerge);
    m_merge_kernel.setArg(1, bufferOutput);
    m_merge_kernel.setArg(2, weights[1]);
    m_merge_kernel.setArg(3, channels >> channelShift);

    try {
        queue.enqueueNDRangeKernel(m_merge_kernel, cl::NullRange,
                                   cl::NDRange(outputs, 361),
                                   cl::NDRange(std::min(8, outputs), 19));
    } catch (cl::Error &e) {
        std::cerr << "Error in merge: " << e.what() << ": "
                  << e.err() << std::endl;
        return;
    }
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

            if (d.getInfo<CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_GPU) {
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
    std::cerr << "Selected " << trim(best_device.getInfo<CL_DEVICE_NAME>())
              << std::endl;
    std::cerr << "with OpenCL " << boost::format("%2.1f") % best_version
              << " capability" << std::endl;

    cl::Context context(best_device);
    cl::Context::setDefault(context);
    cl::Device::setDefault(best_device);

    cl::CommandQueue::setDefault(cl::CommandQueue(context, best_device));

    // Read source file
    std::ifstream sourceFile("convolve_kernel.cl", std::ifstream::in);
    std::string sourceCode(std::istreambuf_iterator<char>(sourceFile),
                           (std::istreambuf_iterator<char>()));

    // Make program of the source code in the context
    cl::Program program;

    try {
        program = cl::Program(sourceCode);
    } catch (cl::Error &e) {
        std::cerr << "Error getting kernels: " << e.what() << ": "
                  << e.err() << std::endl;
        return;
    }
    // Build program for these specific devices
    try {
        program.build("-cl-mad-enable -cl-fast-relaxed-math -cl-no-signed-zeros");
    } catch (cl::Error &e) {
        std::cerr << "Error building: " << std::endl
                  << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl::Device::getDefault())
                  << std::endl;
        return;
    }

    // Make kernel
    m_convolve3_kernel = cl::Kernel(program, "convolve3");
    m_convolve5_kernel = cl::Kernel(program, "convolve5");
    m_merge_kernel = cl::Kernel(program, "merge");
    m_batchnorm_kernel = cl::Kernel(program, "batchnorm");

    m_init_ok = true;
}
#endif
