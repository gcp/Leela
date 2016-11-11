__kernel
void convolve5(
    __global const float * in,
    __global float * merge,
    __global const float * weights,
    __local float * channel_buff,
    __local float * row_buff) {

    // cl::NDRange global(channels, outputs, row);
    const int c   = get_global_id(0);  // channel
    const int o   = get_global_id(1);  // output
    const int row = get_global_id(2);  // row

    const int channels = get_global_size(0);
    const int outputs  = get_global_size(1);

    // cl::NDRange local(2, (1->32), 1);
    const int lx = get_local_id(0);
    const int ly = get_local_id(1);

    const int chan_buff_size = 8;
    const int out_buff_size  = get_local_size(1);
    const int row_buff_size  = 7;
    const int chan_shift     = 3;

    const int filter_size = 5;
    const int filter_len = filter_size * filter_size;
    const int mid = (filter_size / 2) + 1;
    const int extent = mid - 1;

    // input = channels * height * width
    // output = outputs * height * width
    // weights = output * channels * filter
    // merge = channels * outputs * height * width

    const int width = 19;
    const int height = 19;
    const int strip_size = filter_size * width;

    // Copy the input channels (strips) locally
    if (out_buff_size < 19 && ly == 0) {
        // strip-row
        for (int srow = 0; srow < filter_size; srow++) {
            int in_row = row - extent + srow;
            if ((unsigned)in_row >= height) {
                for (int w = 0; w < width; w++) {
                    channel_buff[(lx * filter_size + srow) * width + w] = 0.0f;
                }
            } else {
                for (int w = 0; w < width; w++) {
                    channel_buff[(lx * filter_size + srow) * width + w] =
                        in[(c * height + in_row) * width + w];
                }
            }
        }
    } else if (out_buff_size >= 19 && ly < 19) {
        // Every thread copies a column
        for (int srow = 0; srow < filter_size; srow++) {
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
    for (int f = 0; f < filter_len; f++) {
        filter_buff[f] = weights[(o * channels + c) * filter_len + f];
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    int out_lane = 0;
    int out_cw   = 0;
    for (int cw = 0; cw < width; cw++) {
        int fwstart = cw - extent;
        int fwend   = cw + extent;
        float out;
        // Start filter
        if (fwstart >= 0 && fwend < width) {
            int fid = lx * strip_size + fwstart;
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
            for (int fh = 0; fh < filter_size; fh++) {
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
    __local float * row_buff,
    const int row_tile_size) {

    // cl::NDRange global(channels, outputs, row);
    const int c   = get_global_id(0);  // channel
    const int o   = get_global_id(1);  // output
    const int r   = get_global_id(2);  // row

    const int channels = get_global_size(0);
    const int outputs  = get_global_size(1);

    // cl::NDRange local(2, (1->32), 1);
    const int lx = get_local_id(0);
    const int ly = get_local_id(1);

    const int chan_buff_size = 8;
    const int out_buff_size  = get_local_size(1);
    const int row_buff_size  = 7;
    const int chan_shift     = 3;

    const int width = 19;
    const int height = 19;

    const int filter_size = 3;
    const int filter_len = filter_size * filter_size;
    const int mid = (filter_size / 2) + 1;
    const int extent = mid - 1;
    const int pad_width = width + filter_size - 1;

    // input = channels * height * width
    // output = outputs * height * width
    // weights = output * channels * filter
    // merge = channels * outputs * height * width

    __private float filter_buff[9];
    __private float chan_cache[2];
    __private float stripe_cache[9];

    // Copy the filter we are applying locally
    // output * channel * filter_len
    for (int f = 0; f < filter_len; f++) {
        filter_buff[f] = weights[(o * channels + c) * filter_len + f];
    }

    for (int tile = 0; tile < row_tile_size; tile++) {
        int row = r * row_tile_size + tile;
        if (row > 18) break;

        // Copy the input channels (strips) locally
        if (out_buff_size < 21 && ly == 0) {
            // strip-row
            for (int srow = 0; srow < filter_size; srow++) {
                int in_row = row - extent + srow;
                channel_buff[(lx * pad_width + 0) * filter_size + srow]             = 0.0f;
                if ((unsigned)in_row < height) {
                    for (int w = 0; w < width; w++) {
                        float val = in[(c * height + in_row) * width + w];
                        channel_buff[(lx * pad_width + w + extent) * filter_size + srow] = val;
                    }
                } else {
                    for (int w = 0; w < width; w++) {
                        channel_buff[(lx * pad_width + w + extent) * filter_size + srow] = 0.0f;
                    }
                }
                channel_buff[(lx * pad_width + pad_width - 1) * filter_size + srow] = 0.0f;
            }
        } else if (out_buff_size >= 21 && ly < 21) {
            // Every thread copies a column
            int copy_idx = (lx * pad_width + ly) * filter_size;
            if (tile == 0 || row == 18) {
                // Every thread copies a column
                for (int srow = 0; srow < filter_size; srow++) {
                    int in_row = row - extent + srow;
                    float val = 0.0f;
                    if ((unsigned)in_row < height && ly >= 1 && ly <= 19) {
                        val = in[(c * height + in_row) * width + ly - 1];
                    }
                    channel_buff[copy_idx + srow] = val;
                    if (srow > 0) {
                        chan_cache[srow - 1] = val;
                    }
                }
            } else {
                int in_row = row - extent + 2;
                float val = 0.0f;
                if (ly >= 1 && ly <= 19) {
                    val = in[(c * height + in_row) * width + ly - 1];
                }
                channel_buff[copy_idx + 0] = chan_cache[0];
                channel_buff[copy_idx + 1] = chan_cache[1];
                channel_buff[copy_idx + 2] = val;
                chan_cache[0] = chan_cache[1];
                chan_cache[1] = val;
            }
        }

        int out_lane = 0;
        int out_cw   = 0;
        __local float * out_row_buff = &row_buff[(ly * chan_buff_size + lx) * row_buff_size];
        int fid = (lx * pad_width) * filter_size;
        barrier(CLK_LOCAL_MEM_FENCE);

        for (int rc = 0; rc < 9; rc++) {
            stripe_cache[rc] = channel_buff[fid + rc];
        }

        for (int cw = 0; cw < width; cw++) {
            // Start filter
            float out  =   stripe_cache[      0] * filter_buff[0]
                + stripe_cache[      1] * filter_buff[3]
                + stripe_cache[      2] * filter_buff[6]
                + stripe_cache[      3] * filter_buff[1]
                + stripe_cache[      4] * filter_buff[4]
                + stripe_cache[      5] * filter_buff[7]
                + stripe_cache[      6] * filter_buff[2]
                + stripe_cache[      7] * filter_buff[5]
                + stripe_cache[      8] * filter_buff[8];
            // End filter
            out_row_buff[out_lane++] = out;
            fid += filter_size;

            for (int rc = 0; rc < 6; rc++) {
                stripe_cache[rc] = stripe_cache[rc + 3];
            }
            stripe_cache[6] = channel_buff[fid + 6];
            stripe_cache[7] = channel_buff[fid + 7];
            stripe_cache[8] = channel_buff[fid + 8];

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
    for (int c = 0; c < channels; c++) {
        sum += in[(c * boardsize + b) * outputs + o];
    }
    // ELU
    sum = sum > 0 ? sum : 1.0f * (half_exp(sum) - 1.0f);
    out[o * boardsize + b] = sum;
}