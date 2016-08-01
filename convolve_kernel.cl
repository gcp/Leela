__kernel
void convolve5(
               __global const float * in,
               __global float * merge,
               __global const float * weights,
               __local float * channel_buff,
               __local float * row_buff) {

    // cl::NDRange global(channels, outputs, row);
    const unsigned int c   = get_global_id(0);  // channel
    const unsigned int o   = get_global_id(1);  // output
    const unsigned int row = get_global_id(2);  // row

    const unsigned int channels = get_global_size(0);
    const unsigned int outputs  = get_global_size(1);

    // cl::NDRange local(2, (1->32), 1);
    const unsigned int lx = get_local_id(0);
    const unsigned int ly = get_local_id(1);

    const unsigned int chan_buff_size = 8;
    const unsigned int out_buff_size  = get_local_size(1);
    const unsigned int row_buff_size  = 7;
    const unsigned int chan_shift     = 3;

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
        float out;
        // Start filter
        if (fwstart >= 0 && fwend < width) {
            unsigned int fid = lx * strip_size + fwstart;
            out  = channel_buff[fid              ] * filter_buff[0];
            out += channel_buff[fid           + 1] * filter_buff[1];
            out += channel_buff[fid           + 2] * filter_buff[2];
            out += channel_buff[fid           + 3] * filter_buff[3];
            out += channel_buff[fid           + 4] * filter_buff[4];

            out += channel_buff[fid + width      ] * filter_buff[5];
            out += channel_buff[fid + width   + 1] * filter_buff[6];
            out += channel_buff[fid + width   + 2] * filter_buff[7];
            out += channel_buff[fid + width   + 3] * filter_buff[8];
            out += channel_buff[fid + width   + 4] * filter_buff[9];

            out += channel_buff[fid + width*2    ] * filter_buff[10];
            out += channel_buff[fid + width*2 + 1] * filter_buff[11];
            out += channel_buff[fid + width*2 + 2] * filter_buff[12];
            out += channel_buff[fid + width*2 + 3] * filter_buff[13];
            out += channel_buff[fid + width*2 + 4] * filter_buff[14];

            out += channel_buff[fid + width*3    ] * filter_buff[15];
            out += channel_buff[fid + width*3 + 1] * filter_buff[16];
            out += channel_buff[fid + width*3 + 2] * filter_buff[17];
            out += channel_buff[fid + width*3 + 3] * filter_buff[18];
            out += channel_buff[fid + width*3 + 4] * filter_buff[19];

            out += channel_buff[fid + width*4    ] * filter_buff[20];
            out += channel_buff[fid + width*4 + 1] * filter_buff[21];
            out += channel_buff[fid + width*4 + 2] * filter_buff[22];
            out += channel_buff[fid + width*4 + 3] * filter_buff[23];
            out += channel_buff[fid + width*4 + 4] * filter_buff[24];
        } else {
            const float * filter_idx = filter_buff;
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
                val  = row_buff[(ly * chan_buff_size + 0) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 1) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 2) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 3) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 4) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 5) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 6) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 7) * row_buff_size + lx];
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
               __local float * row_buff) {

    // cl::NDRange global(channels, outputs, row);
    const unsigned int c   = get_global_id(0);  // channel
    const unsigned int o   = get_global_id(1);  // output
    const unsigned int row = get_global_id(2);  // row

    const unsigned int channels = get_global_size(0);
    const unsigned int outputs  = get_global_size(1);

    // cl::NDRange local(2, (1->32), 1);
    const unsigned int lx = get_local_id(0);
    const unsigned int ly = get_local_id(1);

    const unsigned int chan_buff_size = 8;
    const unsigned int out_buff_size  = get_local_size(1);
    const unsigned int row_buff_size  = 7;
    const unsigned int chan_shift     = 3;

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
                val  = row_buff[(ly * chan_buff_size + 0) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 1) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 2) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 3) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 4) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 5) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 6) * row_buff_size + lx];
                val += row_buff[(ly * chan_buff_size + 7) * row_buff_size + lx];
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
    // ReLU
    if (outputs > 4 || output > 0) {
        sum = max(sum, 0.0f);
    }
    // ELU
    // sum = sum > 0 ? sum : 1.0f * (exp(sum) - 1.0f);
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
    const int outputs      = get_global_size(0);
    const int channel_size = get_global_size(1);

    const unsigned int o = output;
    const unsigned int b = gy;

    const float epsilon = 1e-5;

    const float mean = means[o] / scale[0];
    const float variance = epsilon + variances[o] / scale[0];
    const float scale_stddiv = 1.0f / sqrt(variance);

    out[o * channel_size + b] = scale_stddiv
                                * (in[o * channel_size + b] - mean);
}

__kernel void innerproduct(
    __private const int inputs,
    __global const float * in,
    __global float * out,
    __global const float * weights,
    __constant const float * biases) {

    const int gx = get_global_id(0);
    const int output = gx;

    const int outputs = get_global_size(0);

    const unsigned int o = output;

    float val = biases[o];
    for (unsigned int i = 0; i < inputs; i++) {
        val += in[i] * weights[o * inputs + i];
    }
    if (outputs > 1) {
        val = max(val, 0.0f);
    }
    out[o] = val;
}