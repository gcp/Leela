__kernel
void convolve(
                       __global const float * in,
                       __global float * merge,
                       __global const float * weights,
                       __private int filter_size,
                       __local float * channel_buff,
                       __local float * filter_buff,
                       __local float * merge_buff,
                       __private int chan_shift) {

    // cl::NDRange global(channels, outputs, row);
    const unsigned int c   = get_global_id(0);  // channel
    const unsigned int o   = get_global_id(1);  // output
    const unsigned int row = get_global_id(2);  // row

    const unsigned int channels = get_global_size(0);
    const unsigned int outputs = get_global_size(1);
    const unsigned int rows = get_global_size(2);

    // cl::NDRange local(2, (1->32), 1);
    const unsigned int lx = get_local_id(0);
    const unsigned int ly = get_local_id(1);

    const unsigned int chan_buff_size = get_local_size(0);

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
    if (ly == 0) {
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
    }

    // Copy the filter we are applying locally
    // output * channel * filter_len
    for (unsigned int f = 0; f < filter_len; f++) {
        filter_buff[(ly * chan_buff_size + lx) * filter_len + f]
            = weights[(o * channels + c) * filter_len + f];
    }
    __local float * filter_offset =
        &filter_buff[(ly * chan_buff_size + lx) * filter_len];

    barrier(CLK_LOCAL_MEM_FENCE);

    unsigned int ch = row; // row by row
    for (unsigned int cw = 0; cw < width; cw++) {
        int fwstart = cw - extent;
        int fwend   = cw + extent;
        __local float * filter_idx = filter_offset;
        float out = 0.0f;
        // Start filter
        if (fwstart >= 0 && fwend < width) {
            unsigned int fid = lx * strip_size + fwstart;
            if (filter_size == 3) {
                out += channel_buff[fid              ] * *filter_idx++;
                out += channel_buff[fid           + 1] * *filter_idx++;
                out += channel_buff[fid           + 2] * *filter_idx++;

                out += channel_buff[fid + width]       * *filter_idx++;
                out += channel_buff[fid + width   + 1] * *filter_idx++;
                out += channel_buff[fid + width   + 2] * *filter_idx++;

                out += channel_buff[fid + width*2    ] * *filter_idx++;
                out += channel_buff[fid + width*2 + 1] * *filter_idx++;
                out += channel_buff[fid + width*2 + 2] * *filter_idx++;
            } else if (filter_size == 5) {
                out += channel_buff[fid              ] * *filter_idx++;
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
            }
        } else {
            for (unsigned int fh = 0; fh < filter_size; fh++) {
                for (int fw = fwstart; fw <= fwend; fw++) {
                    // "zero padding"
                    if ((unsigned)fw >= width) {
                        filter_idx++;
                        continue;
                    }

                    float input = channel_buff[(lx * filter_size + fh) * width + fw];
                    out += input * (*filter_idx);

                    filter_idx++;
                }
            }
        }
        // End filter
        merge_buff[ly * chan_buff_size + lx] = out;
        barrier(CLK_LOCAL_MEM_FENCE);
        if (lx == 0) {
            float val;
            if (chan_buff_size == 2) {
                val  = merge_buff[ly * 2 + 0];
                val += merge_buff[ly * 2 + 1];
            } else {
                val  = merge_buff[ly * 8 + 0];
                val += merge_buff[ly * 8 + 1];
                val += merge_buff[ly * 8 + 2];
                val += merge_buff[ly * 8 + 3];
                val += merge_buff[ly * 8 + 4];
                val += merge_buff[ly * 8 + 5];
                val += merge_buff[ly * 8 + 6];
                val += merge_buff[ly * 8 + 7];
            }
            merge[(((c>>chan_shift) * height + ch) * width + cw) * outputs + o] = val;
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

    const float mean = means[c] / scale[0];
    const float variance = variances[c] / scale[0];
    const float scale_stddiv = 1.0f / sqrt(variance);

    out[c * board_size + b] = scale_stddiv * (in[c * board_size + b] - mean);
}
