#ifndef IM2COL_H_INCLUDED
#define IM2COL_H_INCLUDED

#include "config.h"
#include <vector>
#include <algorithm>
#include "Utils.h"

template <unsigned long channels,
          unsigned long filter_size>
void im2col(std::vector<float>& input,
            std::vector<float>& output) {
    constexpr unsigned int height = 19;
    constexpr unsigned int width = 19;
    constexpr unsigned int channel_size = height * width;

    constexpr unsigned int pad = (filter_size / 2);
    constexpr unsigned int output_h = height + 2 * pad - filter_size  + 1;
    constexpr unsigned int output_w = width + 2 * pad - filter_size + 1;

    const float* data_im = &input[0];
    float* data_col = &output[0];

    for (int channel = channels; channel--; data_im += channel_size) {
        for (unsigned int kernel_row = 0; kernel_row < filter_size; kernel_row++) {
            for (unsigned int kernel_col = 0; kernel_col < filter_size; kernel_col++) {
                int input_row = -pad + kernel_row;
                for (int output_rows = output_h; output_rows; output_rows--) {
                    if ((unsigned)input_row < height) {
                        int input_col = -pad + kernel_col;
                        for (int output_col = output_w; output_col; output_col--) {
                            if ((unsigned)input_col < width) {
                                *(data_col++) =
                                    data_im[input_row * width + input_col];
                            } else {
                                *(data_col++) = 0;
                            }
                            input_col++;
                        }
                    } else {
                        for (int output_cols = output_w; output_cols; output_cols--) {
                            *(data_col++) = 0;
                        }
                    }
                    input_row++;
                }
            }
        }
    }
}

template <unsigned long channels,
          unsigned long filter_size>
void col2im(std::vector<float>& input,
            std::vector<float>& output) {
    constexpr unsigned int height = 19;
    constexpr unsigned int width = 19;
    constexpr unsigned int channel_size = height * width;

    constexpr unsigned int pad = (filter_size / 2);
    constexpr unsigned int output_h = height + 2 * pad - filter_size  + 1;
    constexpr unsigned int output_w = width + 2 * pad - filter_size + 1;

    std::fill(output.begin(),
              output.begin() + width * height * channels,
              0.0f);

    const float* data_col = &input[0];
    float* data_im = &output[0];

    for (int channel = channels; channel--; data_im += channel_size) {
        for (unsigned int kernel_row = 0; kernel_row < filter_size; kernel_row++) {
            for (unsigned int kernel_col = 0; kernel_col < filter_size; kernel_col++) {
                int input_row = -pad + kernel_row;
                for (int output_rows = output_h; output_rows; output_rows--) {
                    if ((unsigned)input_row < height) {
                        int input_col = -pad + kernel_col;
                        for (int output_col = output_w; output_col; output_col--) {
                            if ((unsigned)input_col < width) {
                                data_im[input_row * width + input_col] +=
                                    *data_col;
                            }
                            data_col++;
                            input_col++;
                        }
                    } else {
                        data_col += output_w;
                    }
                    input_row++;
                }
            }
        }
    }
}

#endif
