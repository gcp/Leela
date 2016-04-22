__kernel void convolve(
                       __global const float * in,
                       __global float * merge,
                       __global const float * weights,
                       __private int filter_size) {

    // cl::NDRange global(channels, outputs);
    const int gx = get_global_id(0);
    const int gy = get_global_id(1);

    const int channels = get_global_size(0);
    const int outputs = get_global_size(1);

    const int channel = gx;
    const int output = gy;

    // cl::NDRange local(2, workgroup_size);
    const int lx = get_local_id(0);
    const int ly = get_local_id(1);

    const int width = 19;
    const int height = 19;

    const int filter_len = filter_size * filter_size;
    const int mid = (filter_size / 2) + 1;
    const int extent = mid - 1;

    // input = channels * height * width
    // output = outputs * height * width
    // weights = output * channels * filter
    // merge = channels * outputs * height * width

    const unsigned int c = channel;
    const unsigned int o = output;

    __global const float * filter = &weights[(o * channels + c) * filter_len];

    for (unsigned int ch = 0; ch < height; ch++) {
        int fhstart = ch - extent;
        int fhend   = ch + extent;
        for (unsigned int cw = 0; cw < width; cw++) {
            int fwstart = cw - extent;
            int fwend   = cw + extent;
            unsigned int filter_idx = 0;
            float out = 0.0f;
            // Start filter
            for (int fh = fhstart; fh <= fhend; fh++) {
                for (int fw = fwstart; fw <= fwend; fw++) {
                    // "zero padding"
                    if ((unsigned)fh >= height) {
                        filter_idx++;
                        continue;
                    }
                    if ((unsigned)fw >= width) {
                        filter_idx++;
                        continue;
                    }

                    float input = in[(c * height + fh) * width + fw];
                    out += input * filter[filter_idx];

                    filter_idx++;
                }
            }
            // End filter
            merge[((c * outputs + o) * height + ch) * width + cw] = out;
        }
    }
}

__kernel void merge(
                    __global const float * in,
                    __global float * out,
                    __constant const float * biases,
                    __private const int channels) {

    // cl::NDRange global(outputs);
    const int gx = get_global_id(0);

    const int output = gx;
    const int outputs = get_global_size(0);

    const int width = 19;
    const int height = 19;
    const int boardsize = width * height;

    const int o = output;
    const float bias = biases[o];

    for (unsigned int b = 0; b < boardsize; b++) {
        float sum = bias;
        for (unsigned int c = 0; c < channels; c++) {
            sum += in[(c * outputs + o) * boardsize + b];
        }
        // ReLU if outputs > 1 (not last layer)
        if (outputs > 1) {
            sum = (sum > 0.0f) ? sum : 0.0f;
        }
        out[o * boardsize + b] = sum;
    }
}
