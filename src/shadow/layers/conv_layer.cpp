#include "shadow/layers/conv_layer.hpp"
#include "shadow/util/blas.hpp"
#include "shadow/util/image.hpp"

inline int convolutional_out_size(int s, int size, int pad, int stride) {
  return (s + 2 * pad - size) / stride + 1;
}

void ConvLayer::Reshape() {
  num_output_ = layer_param_.convolution_param().num_output();
  kernel_size_ = layer_param_.convolution_param().kernel_size();
  stride_ = layer_param_.convolution_param().stride();
  pad_ = layer_param_.convolution_param().pad();

  int in_c = bottom_[0]->shape(1), in_h = bottom_[0]->shape(2),
      in_w = bottom_[0]->shape(3);

  VecInt top_shape = bottom_[0]->shape();
  top_shape[1] = num_output_;
  top_shape[2] = convolutional_out_size(in_h, kernel_size_, pad_, stride_);
  top_shape[3] = convolutional_out_size(in_w, kernel_size_, pad_, stride_);
  top_[0]->reshape(top_shape);

  out_spatial_dim_ = top_[0]->count(2);
  kernel_dim_ = kernel_size_ * kernel_size_ * in_c;

  filters_.reshape(num_output_, kernel_dim_);
  biases_.reshape(num_output_);
  biases_multiplier_.reshape(out_spatial_dim_);
  Blas::Set(out_spatial_dim_, 1, biases_multiplier_.mutable_data(), 0);
  col_image_.reshape(kernel_dim_, out_spatial_dim_);

  std::stringstream out;
  out << layer_name_ << ": "
      << Util::format_vector(bottom_[0]->shape(), ",", "(", ")") << " -> "
      << num_output_ << "_" << kernel_size_ << "x" << kernel_size_ << "_s"
      << stride_ << "_p" << pad_ << " -> "
      << Util::format_vector(top_[0]->shape(), ",", "(", ")");
  DInfo(out.str());
}

void ConvLayer::Forward() {
  int batch = bottom_[0]->shape(0);
  int top_num = top_[0]->num(), bottom_num = bottom_[0]->num();
  for (int b = 0; b < batch; ++b) {
    Image::Im2Col(bottom_[0]->data(), bottom_[0]->shape(), b * bottom_num,
                  kernel_size_, stride_, pad_, top_[0]->shape(),
                  col_image_.mutable_data());
    Blas::BlasSgemm(0, 0, num_output_, out_spatial_dim_, kernel_dim_, 1,
                    filters_.data(), 0, col_image_.data(), 0, 0,
                    top_[0]->mutable_data(), b * top_num);
    Blas::BlasSgemm(0, 0, num_output_, out_spatial_dim_, 1, 1, biases_.data(),
                    0, biases_multiplier_.data(), 0, 1, top_[0]->mutable_data(),
                    b * top_num);
  }
}

void ConvLayer::Release() {
  bottom_.clear();
  top_.clear();

  filters_.clear();
  biases_.clear();
  biases_multiplier_.clear();
  col_image_.clear();

  // DInfo("Free ConvLayer!");
}
