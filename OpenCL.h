#ifndef OPENCL_H_INCLUDED
#define OPENCL_H_INCLUDED

#include "config.h"

#define CL_HPP_MINIMUM_OPENCL_VERSION   110
#define CL_HPP_TARGET_OPENCL_VERSION    120
#define CL_HPP_ENABLE_EXCEPTIONS
#include <CL/cl2.hpp>

#include <atomic>
#include <string>
#include <vector>

#include "SMP.h"

class Layer {
    friend class OpenCL_Network;
private:
    unsigned int channels{0};
    unsigned int outputs{0};
    unsigned int filter_size{0};
    bool is_batchnorm{false};
    bool is_innerproduct{false};
    std::vector<cl::Buffer> weights;
};

class ThreadData {
    friend class OpenCL;
    friend class OpenCL_Network;
private:
    bool m_is_initialized{false};
    cl::CommandQueue m_commandqueue;
    cl::Kernel m_convolve3_kernel;
    cl::Kernel m_convolve5_kernel;
    cl::Kernel m_merge_kernel;
    cl::Kernel m_batchnorm_kernel;
    cl::Kernel m_innerproduct_kernel;
    cl::Buffer m_inBuffer;
    cl::Buffer m_tmpBuffer;
    cl::Buffer m_mergeBuffer;
    cl::Buffer m_outBuffer;
    bool m_buffers_allocated{false};
    std::atomic<int> m_results_outstanding{0};
};

class OpenCL_Network {
public:
    using event_callback =  void (CL_CALLBACK *) (cl_event, cl_int, void *);

    template <size_t M, size_t V>
    void push_batchnorm(unsigned int channel_size,
                        const std::array<float, M> & means,
                        const std::array<float, V> & variances,
                        const std::array<float, 1> & scale) {
        size_t layer = get_layer_count();
        push_weights(layer, means);
        push_weights(layer, variances);
        push_weights(layer, scale);
        m_layers[layer].is_batchnorm = true;
        m_layers[layer].channels = M;
        m_layers[layer].outputs = M;
        m_layers[layer].filter_size = channel_size;
    }

    template <size_t W, size_t B>
    void push_convolve(unsigned int filter_size,
                       const std::array<float, W> & weights,
                       const std::array<float, B> & biases) {
        size_t layer = get_layer_count();
        push_weights(layer, weights);
        push_weights(layer, biases);
        m_layers[layer].outputs = B;
        m_layers[layer].filter_size = filter_size;
        m_layers[layer].channels = W / (B * filter_size * filter_size);
    }

    template <size_t W, size_t B>
    void push_innerproduct(const std::array<float, W> & weights,
                           const std::array<float, B> & biases) {
        size_t layer = get_layer_count();
        push_weights(layer, weights);
        push_weights(layer, biases);
        m_layers[layer].is_innerproduct = true;
        m_layers[layer].channels = W / B;
        m_layers[layer].outputs = B;
    }

    size_t get_layer_count() {
        return m_layers.size();
    }

    void forward(std::vector<float>& input, std::vector<float>& output,
                 event_callback cb, void * data);

private:
    template <size_t W>
    void push_weights(size_t layer, const std::array<float, W> & weights) {
        add_weights(layer, W, weights.data());
    }
    void add_weights(size_t layer, size_t size, const float * weights);
    void convolve(int filter_size, int channels, int outputs,
                  cl::Buffer& input, cl::Buffer& output, cl::Buffer& merge,
                  std::vector<cl::Buffer>& weights);
    void batchnorm(int outputs, int channel_size, cl::Buffer & input,
                   cl::Buffer & output, std::vector<cl::Buffer>& weights);
    void innerproduct(int inputs, int outputs,
                      cl::Buffer& input, cl::Buffer& output,
                      std::vector<cl::Buffer>& weights);
    std::vector<Layer> m_layers;
};

class OpenCL {
    friend class OpenCL_Network;
public:
    void initialize();
    void thread_init(void);

    std::string get_device_name();
    bool thread_can_issue();
    void callback_finished();
    void join_outstanding_cb();
    std::atomic<int> * get_thread_results_outstanding();

private:
    cl::Program m_program;

    size_t m_wavefront_size{0};
    size_t m_max_workgroup_size{0};
    std::vector<size_t> m_max_workgroup_dims;
    bool m_init_ok{false};
    // Keep track of all async/cb threads we dispatch
    std::atomic<int> m_cb_outstanding{0};
};

extern OpenCL opencl;
extern OpenCL_Network opencl_policy_net;
extern OpenCL_Network opencl_value_net;
extern thread_local ThreadData opencl_thread_data;

#endif
