#include "shadow/kernel.hpp"
#include "shadow/util/util.hpp"

namespace Kernel {

#if defined(USE_CUDA)
cublasHandle_t cublas_handle_ = nullptr;

void Setup(int device_id) {
  CheckError(cudaSetDevice(device_id));
  cublasCreate(&cublas_handle_);
}

void Release() {
  if (cublas_handle_ != nullptr) cublasDestroy(cublas_handle_);
}

template <typename T, typename Dtype>
T *MakeBuffer(int size, Dtype *host_ptr) {
  T *buffer;
  CheckError(cudaMalloc(&buffer, size * sizeof(Dtype)));
  if (host_ptr != nullptr) {
    WriteBuffer(size, host_ptr, buffer);
  }
  return buffer;
}

template <typename T, typename Dtype>
void ReadBuffer(int size, const T *src, Dtype *des) {
  CheckError(
      cudaMemcpy(des, src, size * sizeof(Dtype), cudaMemcpyDeviceToHost));
}

template <typename T, typename Dtype>
void WriteBuffer(int size, const Dtype *src, T *des) {
  CheckError(
      cudaMemcpy(des, src, size * sizeof(Dtype), cudaMemcpyHostToDevice));
}

template <typename T, typename Dtype>
void CopyBuffer(int size, const T *src, T *des) {
  CheckError(
      cudaMemcpy(des, src, size * sizeof(Dtype), cudaMemcpyDeviceToDevice));
}

template <typename T>
void ReleaseBuffer(T *buffer) {
  CheckError(cudaFree(buffer));
}

dim3 GridDim(int size) {
  unsigned int k = (unsigned int)(size - 1) / BLOCK + 1;
  unsigned int x = k;
  unsigned int y = 1;
  if (x > 65535) {
    x = (unsigned int)std::ceil(std::sqrt(k));
    y = (size - 1) / (x * BLOCK) + 1;
  }
  return dim3(x, y, 1);
}

void CheckError(cudaError_t status) {
  cudaError_t status2 = cudaGetLastError();
  if (status != cudaSuccess) {
    const char *s = cudaGetErrorString(status);
    std::string message(s);
    Fatal("CUDA Error: " + message);
  }
  if (status2 != cudaSuccess) {
    std::string message(cudaGetErrorString(status));
    Fatal("CUDA Error Prev: " + message);
  }
}

// Explicit instantiation
template int *MakeBuffer<int, int>(int size, int *host_ptr);
template float *MakeBuffer<float, float>(int size, float *host_ptr);

template void ReadBuffer<int, int>(int size, const int *src, int *des);
template void ReadBuffer<float, float>(int size, const float *src, float *des);

template void WriteBuffer<int, int>(int size, const int *src, int *des);
template void WriteBuffer<float, float>(int size, const float *src, float *des);

template void CopyBuffer<int, int>(int size, const int *src, int *des);
template void CopyBuffer<float, float>(int size, const float *src, float *des);

template void ReleaseBuffer<int>(int *buffer);
template void ReleaseBuffer<float>(float *buffer);
#endif

#if defined(USE_CL)
EasyCL *easyCL = nullptr;

void Setup(int device_id) {
  easyCL = EasyCL::createForFirstGpuOtherwiseCpu(true);
  std::string cl_file = "./src/shadow/kernel.cl";
  cl_datatransform_kernel_ = easyCL->buildKernel(cl_file, "DataTransform");
  cl_im2col_kernel_ = easyCL->buildKernel(cl_file, "Im2Col");
  cl_pooling_kernel_ = easyCL->buildKernel(cl_file, "Pooling");
  cl_activations_kernel_ = easyCL->buildKernel(cl_file, "ActivateArray");
  cl_setarray_kernel_ = easyCL->buildKernel(cl_file, "SetArray");
  cl_setarrayrepeat_kernel_ = easyCL->buildKernel(cl_file, "SetArrayRepeat");
  clblasSetup();
}

void Release() {
  cl_datatransform_kernel_->~CLKernel();
  cl_im2col_kernel_->~CLKernel();
  cl_pooling_kernel_->~CLKernel();
  cl_activations_kernel_->~CLKernel();
  cl_setarray_kernel_->~CLKernel();
  cl_setarrayrepeat_kernel_->~CLKernel();
  easyCL->~EasyCL();
  clblasTeardown();
}

template <typename T, typename Dtype>
T *MakeBuffer(int size, Dtype *host_ptr) {
  T *buffer = new cl_mem();
  *buffer = clCreateBuffer(*easyCL->context, CL_MEM_READ_WRITE,
                           size * sizeof(Dtype), host_ptr, nullptr);
  return buffer;
}

template <typename T, typename Dtype>
void ReadBuffer(int size, const T *src, Dtype *des) {
  clEnqueueReadBuffer(*easyCL->queue, *src, CL_TRUE, 0, size * sizeof(Dtype),
                      des, 0, nullptr, nullptr);
  clFinish(*easyCL->queue);
}

template <typename T, typename Dtype>
void WriteBuffer(int size, const Dtype *src, T *des) {
  clEnqueueWriteBuffer(*easyCL->queue, *des, CL_TRUE, 0, size * sizeof(Dtype),
                       src, 0, nullptr, nullptr);
  clFinish(*easyCL->queue);
}

template <typename T, typename Dtype>
void CopyBuffer(int size, const T *src, T *des) {
  clEnqueueCopyBuffer(*easyCL->queue, *src, *des, 0, 0, size * sizeof(Dtype), 0,
                      nullptr, nullptr);
  clFinish(*easyCL->queue);
}

template <typename T>
void ReleaseBuffer(T *buffer) {
  clReleaseMemObject(*buffer);
}

template <typename T>
void DataTransform(int N, const T *in_data, float scale, float mean_value,
                   T *out_data) {
  cl_kernel kernel = cl_datatransform_kernel_->GetKernel();
  clSetKernelArg(kernel, 0, sizeof(int), &N);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), in_data);
  clSetKernelArg(kernel, 2, sizeof(float), &scale);
  clSetKernelArg(kernel, 3, sizeof(float), &mean_value);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), out_data);
  size_t global = N;
  clEnqueueNDRangeKernel(*easyCL->queue, kernel, 1, nullptr, &global, nullptr,
                         0, nullptr, nullptr);
  clFinish(*easyCL->queue);
}

template <typename T>
void Im2Col(const T *in_data, int offset, int in_c, int in_h, int in_w,
            int kernel_size, int stride, int pad, int out_h, int out_w,
            T *out_data) {
  cl_kernel kernel = cl_im2col_kernel_->GetKernel();
  clSetKernelArg(kernel, 0, sizeof(cl_mem), in_data);
  clSetKernelArg(kernel, 1, sizeof(int), &offset);
  clSetKernelArg(kernel, 2, sizeof(int), &in_c);
  clSetKernelArg(kernel, 3, sizeof(int), &in_h);
  clSetKernelArg(kernel, 4, sizeof(int), &in_w);
  clSetKernelArg(kernel, 5, sizeof(int), &kernel_size);
  clSetKernelArg(kernel, 6, sizeof(int), &stride);
  clSetKernelArg(kernel, 7, sizeof(int), &pad);
  clSetKernelArg(kernel, 8, sizeof(int), &out_h);
  clSetKernelArg(kernel, 9, sizeof(int), &out_w);
  clSetKernelArg(kernel, 10, sizeof(cl_mem), out_data);
  size_t global = in_c * out_h * out_w;
  clEnqueueNDRangeKernel(*easyCL->queue, kernel, 1, nullptr, &global, nullptr,
                         0, nullptr, nullptr);
  clFinish(*easyCL->queue);
}

template <typename T>
void Pooling(const T *in_data, int batch, int in_c, int in_h, int in_w,
             int kernel_size, int stride, int out_h, int out_w, int mode,
             T *out_data) {
  cl_kernel kernel = cl_pooling_kernel_->GetKernel();
  clSetKernelArg(kernel, 0, sizeof(cl_mem), in_data);
  clSetKernelArg(kernel, 1, sizeof(int), &batch);
  clSetKernelArg(kernel, 2, sizeof(int), &in_c);
  clSetKernelArg(kernel, 3, sizeof(int), &in_h);
  clSetKernelArg(kernel, 4, sizeof(int), &in_w);
  clSetKernelArg(kernel, 5, sizeof(int), &kernel_size);
  clSetKernelArg(kernel, 6, sizeof(int), &stride);
  clSetKernelArg(kernel, 7, sizeof(int), &out_h);
  clSetKernelArg(kernel, 8, sizeof(int), &out_w);
  clSetKernelArg(kernel, 9, sizeof(int), &mode);
  clSetKernelArg(kernel, 10, sizeof(cl_mem), out_data);
  size_t global = batch * in_c * out_h * out_w;
  clEnqueueNDRangeKernel(*easyCL->queue, kernel, 1, nullptr, &global, nullptr,
                         0, nullptr, nullptr);
  clFinish(*easyCL->queue);
}

template <typename T>
void ActivateArray(int N, const shadow::ActivateType &type, T *out_data) {
  cl_kernel kernel = cl_activations_kernel_->GetKernel();
  clSetKernelArg(kernel, 0, sizeof(int), &N);
  clSetKernelArg(kernel, 1, sizeof(int), &type);
  clSetKernelArg(kernel, 2, sizeof(cl_mem), out_data);
  size_t global = N;
  clEnqueueNDRangeKernel(*easyCL->queue, kernel, 1, nullptr, &global, nullptr,
                         0, nullptr, nullptr);
  clFinish(*easyCL->queue);
}

template <typename T>
void SetArray(int N, float value, T *out_data) {
  cl_kernel kernel = cl_setarray_kernel_->GetKernel();
  clSetKernelArg(kernel, 0, sizeof(int), &N);
  clSetKernelArg(kernel, 1, sizeof(float), &value);
  clSetKernelArg(kernel, 2, sizeof(cl_mem), out_data);
  size_t global = N;
  clEnqueueNDRangeKernel(*easyCL->queue, kernel, 1, nullptr, &global, nullptr,
                         0, nullptr, nullptr);
  clFinish(*easyCL->queue);
}

template <typename T>
void SetArrayRepeat(int N, const T *value, int value_size, T *out_data,
                    int offset) {
  cl_kernel kernel = cl_setarrayrepeat_kernel_->GetKernel();
  clSetKernelArg(kernel, 0, sizeof(int), &N);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), value);
  clSetKernelArg(kernel, 2, sizeof(int), &value_size);
  clSetKernelArg(kernel, 3, sizeof(cl_mem), out_data);
  clSetKernelArg(kernel, 4, sizeof(int), &offset);
  size_t global = N * value_size;
  clEnqueueNDRangeKernel(*easyCL->queue, kernel, 1, nullptr, &global, nullptr,
                         0, nullptr, nullptr);
  clFinish(*easyCL->queue);
}

// Explicit instantiation
template cl_mem *MakeBuffer<cl_mem, int>(int size, int *host_ptr);
template cl_mem *MakeBuffer<cl_mem, float>(int size, float *host_ptr);

template void ReadBuffer<cl_mem, int>(int size, const cl_mem *src, int *des);
template void ReadBuffer<cl_mem, float>(int size, const cl_mem *src,
                                        float *des);

template void WriteBuffer<cl_mem, int>(int size, const int *src, cl_mem *des);
template void WriteBuffer<cl_mem, float>(int size, const float *src,
                                         cl_mem *des);

template void CopyBuffer<cl_mem, int>(int size, const cl_mem *src, cl_mem *des);

template void ReleaseBuffer<cl_mem>(cl_mem *buffer);

template void DataTransform<cl_mem>(int N, const cl_mem *in_data, float scale,
                                    float mean_value, cl_mem *out_data);
template void Im2Col<cl_mem>(const cl_mem *in_data, int offset, int in_c,
                             int in_h, int in_w, int kernel_size, int stride,
                             int pad, int out_h, int out_w, cl_mem *out_data);

template void Pooling<cl_mem>(const cl_mem *in_data, int batch, int in_c,
                              int in_h, int in_w, int kernel_size, int stride,
                              int out_h, int out_w, int mode, cl_mem *out_data);

template void ActivateArray<cl_mem>(int N, const shadow::ActivateType &type,
                                    cl_mem *out_data);

template void SetArray<cl_mem>(int N, float value, cl_mem *out_data);

template void SetArrayRepeat<cl_mem>(int N, const cl_mem *value, int value_size,
                                     cl_mem *out_data, int offset);
#endif

}  // namespace Kernel