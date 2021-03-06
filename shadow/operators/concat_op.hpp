#ifndef SHADOW_OPERATORS_CONCAT_OP_HPP
#define SHADOW_OPERATORS_CONCAT_OP_HPP

#include "core/operator.hpp"

namespace Shadow {

class ConcatOp : public Operator {
 public:
  explicit ConcatOp(const shadow::OpParam &op_param, Workspace *ws)
      : Operator(op_param, ws) {
    concat_axis_ = get_single_argument<int>("axis", 1);
    CHECK_GE(concat_axis_, 0);
    CHECK_LT(concat_axis_, bottoms<float>(0)->num_axes());
    num_concats_ = bottoms<float>(0)->count(0, concat_axis_);
    concat_input_size_ = bottoms<float>(0)->count(concat_axis_ + 1);
  }

  void Reshape() override;
  void Forward() override;

 private:
  int concat_axis_, num_concats_, concat_input_size_;
};

namespace Vision {

template <typename T>
void Concat(const T *in_data, int count, int num_concats, int concat_size,
            int top_concat_axis, int bottom_concat_axis, int offset_concat_axis,
            T *out_data);

}  // namespace Vision

}  // namespace Shadow

#endif  // SHADOW_OPERATORS_CONCAT_OP_HPP
