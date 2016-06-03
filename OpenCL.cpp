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

static std::string sourceCode = R"(
    __kernel
    void convolve5(
            __global const float * in,
            __global float * merge,
            __global const float * weights,
            __local float * channel_buff,
            __private const int chan_shift,
            __local float * row_buff,
            __private const int row_buff_size) {

        // cl::NDRange global(channels, outputs, row);
        const unsigned int c   = get_global_id(0);  // channel
        const unsigned int o   = get_global_id(1);  // output
        const unsigned int row = get_global_id(2);  // row

        const unsigned int channels = get_global_size(0);
        const unsigned int outputs  = get_global_size(1);

        // cl::NDRange local(2, (1->32), 1);
        const unsigned int lx = get_local_id(0);
        const unsigned int ly = get_local_id(1);

        const unsigned int chan_buff_size = get_local_size(0);
        const unsigned int out_buff_size  = get_local_size(1);

        const unsigned int filter_size = 5;
        const unsigned int filter_len = filter_size * filter_size;
        const unsigned int mid = (filter_size / 2) + 1;
        const unsigned int extent = mid - 1;

        // input = channels * height * width
        // output = outputs * height * width
        // weights = output * channels * filter
        // merge = channels * outputs * height * width

        const unsigned int width = 19;
        const unsigned int height = 19;
        const unsigned int strip_size = filter_size * width;

        // Copy the input channels (strips) locally
        if (out_buff_size < 19 && ly == 0) {
            // strip-row
            for (unsigned int srow = 0; srow < filter_size; srow++) {
                int in_row = row - extent + srow;
                if ((unsigned)in_row >= height) {
                    for (unsigned int w = 0; w < width; w++) {
                        channel_buff[(lx * filter_size + srow) * width + w] = 0.0f;
                    }
                } else {
                    for (unsigned int w = 0; w < width; w++) {
                        channel_buff[(lx * filter_size + srow) * width + w] =
                            in[(c * height + in_row) * width + w];
                    }
                }
            }
        } else if (out_buff_size >= 19 && ly < 19) {
            // Every thread copies a column
            for (unsigned int srow = 0; srow < filter_size; srow++) {
                int in_row = row - extent + srow;
                float val = 0.0f;
                if ((unsigned)in_row < height) {
                    val = in[(c * height + in_row) * width + ly];
                }
                channel_buff[(lx * filter_size + srow) * width + ly] = val;
            }
        }

        __private float filter_buff[25];

        // Copy the filter we are applying locally
        // output * channel * filter_len
        for (unsigned int f = 0; f < filter_len; f++) {
            filter_buff[f] = weights[(o * channels + c) * filter_len + f];
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        unsigned int out_lane = 0;
        unsigned int out_cw   = 0;
        for (unsigned int cw = 0; cw < width; cw++) {
            int fwstart = cw - extent;
            int fwend   = cw + extent;
            const float * filter_idx = filter_buff;
            float out;
            // Start filter
            if (fwstart >= 0 && fwend < width) {
                unsigned int fid = lx * strip_size + fwstart;
                out  = channel_buff[fid              ] * *filter_idx++;
                out += channel_buff[fid           + 1] * *filter_idx++;
                out += channel_buff[fid           + 2] * *filter_idx++;
                out += channel_buff[fid           + 3] * *filter_idx++;
                out += channel_buff[fid           + 4] * *filter_idx++;

                out += channel_buff[fid + width      ] * *filter_idx++;
                out += channel_buff[fid + width   + 1] * *filter_idx++;
                out += channel_buff[fid + width   + 2] * *filter_idx++;
                out += channel_buff[fid + width   + 3] * *filter_idx++;
                out += channel_buff[fid + width   + 4] * *filter_idx++;

                out += channel_buff[fid + width*2    ] * *filter_idx++;
                out += channel_buff[fid + width*2 + 1] * *filter_idx++;
                out += channel_buff[fid + width*2 + 2] * *filter_idx++;
                out += channel_buff[fid + width*2 + 3] * *filter_idx++;
                out += channel_buff[fid + width*2 + 4] * *filter_idx++;

                out += channel_buff[fid + width*3    ] * *filter_idx++;
                out += channel_buff[fid + width*3 + 1] * *filter_idx++;
                out += channel_buff[fid + width*3 + 2] * *filter_idx++;
                out += channel_buff[fid + width*3 + 3] * *filter_idx++;
                out += channel_buff[fid + width*3 + 4] * *filter_idx++;

                out += channel_buff[fid + width*4    ] * *filter_idx++;
                out += channel_buff[fid + width*4 + 1] * *filter_idx++;
                out += channel_buff[fid + width*4 + 2] * *filter_idx++;
                out += channel_buff[fid + width*4 + 3] * *filter_idx++;
                out += channel_buff[fid + width*4 + 4] * *filter_idx++;
            } else {
                out = 0.0f;
                for (unsigned int fh = 0; fh < filter_size; fh++) {
                    for (int fw = fwstart; fw <= fwend; fw++) {
                        // "zero padding"
                        if ((unsigned)fw >= width) {
                            filter_idx++;
                            continue;
                        }

                        float input = channel_buff[(lx * filter_size + fh) * width + fw];
                        out += input * *filter_idx++;
                    }
                }
            }
            // End filter
            row_buff[(ly * chan_buff_size + lx) * row_buff_size + out_lane] = out;
            out_lane++;

            // Row buffer full or last lane?
            if (out_lane == row_buff_size || (cw == width - 1)) {
                barrier(CLK_LOCAL_MEM_FENCE);
                if (lx < out_lane) {
                    float val;
                    if (chan_buff_size == 2) {
                        val  = row_buff[(ly * chan_buff_size + 0) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 1) * row_buff_size + lx];
                    } else {
                        val  = row_buff[(ly * chan_buff_size + 0) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 1) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 2) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 3) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 4) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 5) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 6) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 7) * row_buff_size + lx];
                    }
                    merge[(((c >> chan_shift) * height + row) * width + out_cw + lx) * outputs + o] = val;
                }
                out_cw  += row_buff_size;
                out_lane = 0;
            }
        }
    }

    __kernel
    void convolve3(
        __global const float * in,
        __global float * merge,
        __global const float * weights,
        __local float * channel_buff,
        __private const int chan_shift,
        __local float * row_buff,
        __private const int row_buff_size) {

        // cl::NDRange global(channels, outputs, row);
        const unsigned int c   = get_global_id(0);  // channel
        const unsigned int o   = get_global_id(1);  // output
        const unsigned int row = get_global_id(2);  // row

        const unsigned int channels = get_global_size(0);
        const unsigned int outputs  = get_global_size(1);

        // cl::NDRange local(2, (1->32), 1);
        const unsigned int lx = get_local_id(0);
        const unsigned int ly = get_local_id(1);

        const unsigned int chan_buff_size = get_local_size(0);
        const unsigned int out_buff_size  = get_local_size(1);

        const unsigned int width = 19;
        const unsigned int height = 19;

        const unsigned int filter_size = 3;
        const unsigned int filter_len = filter_size * filter_size;
        const unsigned int mid = (filter_size / 2) + 1;
        const unsigned int extent = mid - 1;
        const unsigned int pad_width = width + filter_size - 1;

        // input = channels * height * width
        // output = outputs * height * width
        // weights = output * channels * filter
        // merge = channels * outputs * height * width

        const unsigned int strip_size = filter_size * pad_width;

        // Copy the input channels (strips) locally
        if (out_buff_size < 21 && ly == 0) {
            // strip-row
            for (unsigned int srow = 0; srow < filter_size; srow++) {
                int in_row = row - extent + srow;
                channel_buff[(lx * filter_size + srow) * pad_width + 0]             = 0.0f;
                channel_buff[(lx * filter_size + srow) * pad_width + pad_width - 1] = 0.0f;
                for (unsigned int w = 0; w < width; w++) {
                    float val = 0.0f;
                    if ((unsigned)in_row < height) {
                        val = in[(c * height + in_row) * width + w];
                    }
                    channel_buff[(lx * filter_size + srow) * pad_width + w + extent] = val;
                }
            }
        } else if (out_buff_size >= 21 && ly < 21) {
            // Every thread copies a column
            for (unsigned int srow = 0; srow < filter_size; srow++) {
                int in_row = row - extent + srow;
                float val = 0.0f;
                if ((unsigned)in_row < height && ly >= 1 && ly <= 19) {
                    val = in[(c * height + in_row) * width + ly - 1];
                }
                channel_buff[(lx * filter_size + srow) * pad_width + ly] = val;
            }
        }

        __private float filter_buff[9];

        // Copy the filter we are applying locally
        // output * channel * filter_len
        for (unsigned int f = 0; f < filter_len; f++) {
            filter_buff[f] = weights[(o * channels + c) * filter_len + f];
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        unsigned int out_lane = 0;
        unsigned int out_cw   = 0;
        for (unsigned int cw = 0; cw < width; cw++) {
            unsigned int fid = lx * strip_size + cw;
            float out;
            // Start filter
            out  = channel_buff[fid                  ] * filter_buff[0];
            out += channel_buff[fid               + 1] * filter_buff[1];
            out += channel_buff[fid               + 2] * filter_buff[2];

            out += channel_buff[fid + pad_width      ] * filter_buff[3];
            out += channel_buff[fid + pad_width   + 1] * filter_buff[4];
            out += channel_buff[fid + pad_width   + 2] * filter_buff[5];

            out += channel_buff[fid + pad_width*2    ] * filter_buff[6];
            out += channel_buff[fid + pad_width*2 + 1] * filter_buff[7];
            out += channel_buff[fid + pad_width*2 + 2] * filter_buff[8];
            // End filter
            row_buff[(ly * chan_buff_size + lx) * row_buff_size + out_lane] = out;
            out_lane++;

            // Row buffer full or last lane?
            if (out_lane == row_buff_size || (cw == width - 1)) {
                barrier(CLK_LOCAL_MEM_FENCE);
                if (lx < out_lane) {
                    // lx = channels 2 or 8, ly = outputs 32
                    // repurpose the lx threads over columns now
                    float val;
                    if (chan_buff_size == 2) {
                        val  = row_buff[(ly * chan_buff_size + 0) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 1) * row_buff_size + lx];
                    } else {
                        val  = row_buff[(ly * chan_buff_size + 0) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 1) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 2) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 3) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 4) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 5) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 6) * row_buff_size + lx];
                        val += row_buff[(ly * chan_buff_size + 7) * row_buff_size + lx];
                    }
                    merge[(((c >> chan_shift) * height + row) * width + out_cw + lx) * outputs + o] = val;
                }
                out_cw  += row_buff_size;
                out_lane = 0;
            }
        }
    }

    __kernel void merge(
        __global const float * in,
        __global float * out,
        __constant const float * biases,
        __private const int channels) {

        // cl::NDRange global(outputs, 19*19);
        const int gx = get_global_id(0);
        const int gy = get_global_id(1);

        const int output = gx;
        const int b = gy;
        const int outputs = get_global_size(0);

        const int width = 19;
        const int height = 19;
        const int boardsize = width * height;

        const int o = output;
        const float bias = biases[o];

        float sum = bias;
        for (unsigned int c = 0; c < channels; c++) {
            sum += in[(c * boardsize + b) * outputs + o];
        }
        // ReLU if outputs > 1 (not last layer)
        if (outputs > 1) {
            sum = max(sum, 0.0f);
        }
        out[o * boardsize + b] = sum;
    }

    __kernel void batchnorm(
        __global const float * in,
        __global float * out,
        __constant const float * means,
        __constant const float * variances,
        __constant const float * scale) {

        // cl::NDRange global(outputs, 19*19);
        const int gx = get_global_id(0);
        const int gy = get_global_id(1);

        const int output = gx;
        const int outputs = get_global_size(0);

        const unsigned int width = 19;
        const unsigned int height = 19;
        const unsigned int board_size = width * height;

        const unsigned int c = output;
        const unsigned int b = gy;

        const float epsilon = 1e-5;

        const float mean = means[c] / scale[0];
        const float variance = epsilon + variances[c] / scale[0];
        const float scale_stddiv = 1.0f / sqrt(variance);

        out[c * board_size + b] = scale_stddiv * (in[c * board_size + b] - mean);
    }
)";

OpenCL* OpenCL::s_OpenCL = nullptr;

OpenCL * OpenCL::get_OpenCL(void) {
    if (!s_OpenCL) {
        s_OpenCL = new OpenCL();
        s_OpenCL->initialize();
    }
    return s_OpenCL;
}

OpenCL::OpenCL(void) {
}

void OpenCL::thread_init() {
    if (!thread_data.get()) {
        auto data = new ThreadData();

        // Make kernels
        data->m_convolve3_kernel = cl::Kernel(m_program, "convolve3");
        data->m_convolve5_kernel = cl::Kernel(m_program, "convolve5");
        data->m_merge_kernel = cl::Kernel(m_program, "merge");
        data->m_batchnorm_kernel = cl::Kernel(m_program, "batchnorm");

        data->m_commandqueue = cl::CommandQueue(cl::Context::getDefault(),
            cl::Device::getDefault());

        thread_data.reset(data);
    }
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
    cl::CommandQueue queue = thread_data.get()->m_commandqueue;
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

    cl::CommandQueue queue = thread_data.get()->m_commandqueue;

    cl::Kernel batchnorm_kernel = thread_data.get()->m_batchnorm_kernel;

    try {
        batchnorm_kernel.setArg(0, bufferInput);
        batchnorm_kernel.setArg(1, bufferOutput);
        batchnorm_kernel.setArg(2, weights[0]);
        batchnorm_kernel.setArg(3, weights[1]);
        batchnorm_kernel.setArg(4, weights[2]);

        queue.enqueueNDRangeKernel(batchnorm_kernel, cl::NullRange,
                                   cl::NDRange(outputs, boardsize),
                                   cl::NDRange(std::min(8, outputs), 19));
    } catch (cl::Error &e) {
        std::cerr << "Error in convolve: " << e.what() << ": "
            << e.err() << std::endl;
        return;
    }

}

static int rounddown_pow2(int val) {
    return (int)std::floor(std::log2(val));
}

void OpenCL::convolve(int filter_size, int channels, int outputs,
                      cl::Buffer& bufferInput,
                      cl::Buffer& bufferOutput,
                      std::vector<cl::Buffer>& weights) {
    // fixed for 19x19
    constexpr int width = 19;
    constexpr int height = 19;
    constexpr int boardsize = width * height;
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
        m_convolve_kernel = thread_data.get()->m_convolve3_kernel;
    } else {
        m_convolve_kernel = thread_data.get()->m_convolve5_kernel;
    }

    int channelShift;
    if (channels % 8 == 0) {
        channelGroup = 8;
        channelShift = 3;
    } else {
        channelGroup = 2;
        channelShift = 1;
    }
    rowGroup = 1;
    // Workgroup things
    if (m_wavefront_size >= 64) {
        outputGroup = std::min(outputs, 32);
    } else {
        outputGroup = std::min(outputs, 1 << rounddown_pow2(m_wavefront_size / 2));
    }

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

    int rowBuffer = std::min<int>(channelGroup, 7);
    size_t rowSize = channelGroup * outputGroup * rowBuffer * sizeof(float);

    cl::Buffer bufferMerge = cl::Buffer(CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS,
                                        mergeSize);

    cl::CommandQueue queue = thread_data.get()->m_commandqueue;

    try {
        m_convolve_kernel.setArg(0, bufferInput);
        m_convolve_kernel.setArg(1, bufferMerge);
        m_convolve_kernel.setArg(2, weights[0]);
        m_convolve_kernel.setArg(3, cl::Local(stripSize * channelGroup * rowGroup));
        m_convolve_kernel.setArg(4, channelShift);
        m_convolve_kernel.setArg(5, cl::Local(rowSize));
        m_convolve_kernel.setArg(6, rowBuffer);

        queue.enqueueNDRangeKernel(m_convolve_kernel, cl::NullRange,
                                   cl::NDRange(channels, outputs, 19),
                                   cl::NDRange(channelGroup, outputGroup, rowGroup));
    } catch (cl::Error &e) {
        std::cerr << "Error in convolve: " << e.what() << ": "
                  << e.err() << std::endl;
        return;
    }

    cl::Kernel merge_kernel = thread_data.get()->m_merge_kernel;

    try {
        merge_kernel.setArg(0, bufferMerge);
        merge_kernel.setArg(1, bufferOutput);
        merge_kernel.setArg(2, weights[1]);
        merge_kernel.setArg(3, channels >> channelShift);

        queue.enqueueNDRangeKernel(merge_kernel, cl::NullRange,
                                   cl::NDRange(outputs, boardsize),
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
    std::string best_vendor;
    int best_score = 0;
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

            // assign score, try to find best device
            int this_score = 0;
            std::string this_vendor = d.getInfo<CL_DEVICE_VENDOR>();
            this_score += 1000 * boost::icontains(this_vendor, "advanced micro devices");
            this_score += 1000 * boost::icontains(this_vendor, "nvidia");
            this_score +=  500 * boost::icontains(this_vendor, "intel");
            this_score +=  100 * (d.getInfo<CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_GPU);
            this_score +=  opencl_version * 10;
            std::cerr << "Device score:  " << this_score << std::endl;

            if (this_score > best_score) {
                best_version = opencl_version;
                best_platform = p;
                best_device = d;
                best_score = this_score;
                found_device = true;
            }
        }
    }

    if (!found_device) {
        return;
    }

    cl::Platform::setDefault(best_platform);
    std::cerr << "Selected platform: " << best_platform.getInfo<CL_PLATFORM_NAME>()
              << std::endl;
    std::cerr << "Selected device: " << trim(best_device.getInfo<CL_DEVICE_NAME>())
              << std::endl;
    std::cerr << "with OpenCL " << boost::format("%2.1f") % best_version
              << " capability" << std::endl;

    cl::Context context(best_device);
    cl::Context::setDefault(context);
    cl::Device::setDefault(best_device);

    // Read source file
    std::ifstream sourceFile("convolve_kernel.cl", std::ifstream::in);
    std::string sourceCode(std::istreambuf_iterator<char>(sourceFile),
                           (std::istreambuf_iterator<char>()));

    // Make program of the source code in the context
    try {
        m_program = cl::Program(sourceCode);
    } catch (cl::Error &e) {
        std::cerr << "Error getting kernels: " << e.what() << ": "
                  << e.err() << std::endl;
        return;
    }
    // Build program for these specific devices
    try {
        m_program.build("-cl-mad-enable -cl-fast-relaxed-math -cl-no-signed-zeros");
    } catch (cl::Error &e) {
        std::cerr << "Error building: " << std::endl
                  << m_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl::Device::getDefault())
                  << std::endl;
        return;
    }

    thread_init();

    m_wavefront_size =
        thread_data.get()->m_convolve3_kernel.getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(
            best_device);
    std::cerr << "Wavefront/Warp size: " << m_wavefront_size << std::endl;

    m_init_ok = true;
}

std::string OpenCL::get_device_name() {
    std::stringstream ss;

    cl::Device device = cl::Device::getDefault();
    ss << "OpenCL: ";
    ss << device.getInfo<CL_DEVICE_VENDOR>() << " ";
    ss << device.getInfo<CL_DEVICE_NAME>() << " @ ";
    ss << device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>() << "Mhz";

    return ss.str();
}
#endif
