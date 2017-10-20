#!/usr/bin/env python3

f = open('net.txt', 'w')

for i in range(2, 40):
    f.write("""
# residual block {3}
layer {{
  name: "conv{0}"
  type: "Convolution"
  bottom: "elt{1}"
  top: "conv{0}"
  convolution_param {{
    num_output: 256
    kernel_size: 3
    pad: 1
    weight_filler {{
      type: "xavier"
    }}
    bias_filler {{
      type: "constant"
    }}
  }}
}}

layer {{
  name: "bn{0}"
  type: "BatchNorm"
  bottom: "conv{0}"
  top: "conv{0}"
}}

layer {{
  name: "relu{0}"
  type: "ReLU"
  bottom: "conv{0}"
  top: "conv{0}"
}}

layer {{
  name: "conv{2}"
  type: "Convolution"
  bottom: "conv{0}"
  top: "conv{2}"
  convolution_param {{
    num_output: 256
    kernel_size: 3
    pad: 1
    weight_filler {{
      type: "xavier"
    }}
    bias_filler {{
      type: "constant"
    }}
  }}
}}

layer {{
  name: "bn{2}"
  type: "BatchNorm"
  bottom: "conv{2}"
  top: "conv{2}"
}}

layer {{
  name: "elt{2}"
  type: "Eltwise"
  bottom: "elt{1}"
  bottom: "conv{2}"
  top: "elt{2}"
}}

layer {{
  name: "relu{2}"
  type: "ReLU"
  bottom: "elt{2}"
  top: "elt{2}"
}}
"""\
    .format(i*2, i*2 - 1, i*2 + 1, i))

f.close()
