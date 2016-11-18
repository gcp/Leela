#ifndef OPENCL_H_INCLUDED
#define OPENCL_H_INCLUDED

#include "config.h"

#define CL_HPP_MINIMUM_OPENCL_VERSION   110
#define CL_HPP_TARGET_OPENCL_VERSION    120
#define CL_HPP_ENABLE_EXCEPTIONS
#include <CL/cl2.hpp>

#include <boost/thread/tss.hpp>
#include <atomic>
#include <string>
#include <vector>

#include "SMP.h"

class Layer {
    friend class OpenCL;
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
private:
    cl::CommandQueue m_commandqueue;
    cl::Kernel m_convolve3_kernel;
    cl::Kernel m_convolve5_kernel;
    cl::Kernel m_merge_kernel;
    cl::Kernel m_batchnorm_kernel;
    cl::Kernel m_innerproduct_kernel;
    cl::Event m_complete_event;
    cl::Buffer m_inBuffer;
    cl::Buffer m_tmpBuffer;
    cl::Buffer m_mergeBuffer;
    cl::Buffer m_outBuffer;
    bool m_buffers_allocated{false};
    std::atomic<int> m_results_outstanding{0};
};

class OpenCL {
public:
    using event_callback =  void (CL_CALLBACK *) (cl_event, cl_int, void *);

    static OpenCL* get_OpenCL(void);
    void thread_init(void);

    template <unsigned long M, unsigned long V>
    void push_batchnorm(unsigned int channel_size,
                        std::tr1::array<float, M> & means,
                        std::tr1::array<float, V> & variances,
                        std::tr1::array<float, 1> & scale) {
        int layer = get_layer_count();
        push_weights(layer, means);
        push_weights(layer, variances);
        push_weights(layer, scale);
        m_layers[layer].is_batchnorm = true;
        m_layers[layer].channels = M;
        m_layers[layer].outputs = M;
        m_layers[layer].filter_size = channel_size;
    }

    template <unsigned long W, unsigned long B>
    void push_convolve(unsigned int filter_size,
                       std::array<float, W> & weights,
                       std::array<float, B> & biases) {
        int layer = get_layer_count();
        push_weights(layer, weights);
        push_weights(layer, biases);
        m_layers[layer].outputs = B;
        m_layers[layer].filter_size = filter_size;
        m_layers[layer].channels = W / (B * filter_size * filter_size);
    }

    template <unsigned long W, unsigned long B>
    void push_innerproduct(std::tr1::array<float, W> & weights,
                           std::tr1::array<float, B> & biases) {
        int layer = get_layer_count();
        push_weights(layer, weights);
        push_weights(layer, biases);
        m_layers[layer].is_innerproduct = true;
        m_layers[layer].channels = W / B;
        m_layers[layer].outputs = B;
    }

    size_t get_layer_count() {
        return m_layers.size();
    }

    std::string get_device_name();
    void forward_async(std::vector<float>& input, std::vector<float>& output,
                       event_callback cb, void * data);
    bool thread_can_issue();
    void callback_finished();
    void join_outstanding_cb();
    std::atomic<int> * get_thread_results_outstanding();

private:
    OpenCL();
    void initialize();
    template <unsigned long W>
    void push_weights(int layer, std::array<float, W> & weights) {
        add_weights(layer, W, &weights[0]);
    }
    void add_weights(int layer, size_t size, float * weights);
    void convolve(int filter_size, int channels, int outputs,
                  cl::Buffer& input, cl::Buffer& output, cl::Buffer& merge,
                  std::vector<cl::Buffer>& weights);
    void batchnorm(int outputs, int channel_size, cl::Buffer & input,
                   cl::Buffer & output, std::vector<cl::Buffer>& weights);
    void innerproduct(int inputs, int outputs,
                      cl::Buffer& input, cl::Buffer& output,
                      std::vector<cl::Buffer>& weights);
    static OpenCL* s_OpenCL;
    boost::thread_specific_ptr<ThreadData> thread_data;
    std::vector<Layer> m_layers;
    cl::Program m_program;

    size_t m_wavefront_size{0};
    size_t m_max_workgroup_size{0};
    std::vector<size_t> m_max_workgroup_dims;
    bool m_init_ok{false};

    // Keep track of all async/cb threads we dispatch
    std::atomic<int> m_cb_outstanding{0};
};

#endif
