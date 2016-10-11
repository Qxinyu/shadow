#ifndef SHADOW_UTIL_IMAGE_HPP
#define SHADOW_UTIL_IMAGE_HPP

#include "shadow/kernel.hpp"
#include "shadow/util/util.hpp"

namespace Image {

template <typename T>
void DataTransform(int N, const T *in_data, float scale, float mean_value,
                   T *out_data);

template <typename T>
void Im2Col(const VecInt &in_shape, const T *in_data, int offset,
            int kernel_size, int stride, int pad, const VecInt &out_shape,
            T *out_data);

template <typename T>
void Pooling(const VecInt &in_shape, const T *in_data, int kernel_size,
             int stride, int mode, const VecInt &out_shape, T *out_data);

template <typename T>
void Concat(const T *in_data, int count, int num_concats, int concat_size,
            int top_concat_axis, int bottom_concat_axis, int offset_concat_axis,
            T *out_data);

template <typename T, typename Dtype>
void Permute(const T *in_data, int count, int num_axes,
             const Dtype *permute_order, const Dtype *old_steps,
             const Dtype *new_steps, T *out_data);

}  // namespace Image

#endif  // SHADOW_UTIL_IMAGE_HPP
