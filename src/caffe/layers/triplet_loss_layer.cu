/*
* triplet_loss_layer.cu
*
*/

#include <algorithm>
#include <vector>


#include "caffe/layers/triplet_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

  template <typename Dtype>
  void TripletLossLayer<Dtype>::Forward_gpu(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
    const int count = bottom[0]->count();
    
    Dtype* sampleWv = NULL;
    Blob<Dtype> sampleWv_Blob;
    if(bottom.size() == 4)
    {
        sampleWv = bottom[3]->mutable_cpu_data();
    }else
    {
        sampleWv_Blob.Reshape(bottom[0]->num(), 1, 1, 1);
        sampleWv = sampleWv_Blob.mutable_cpu_data();
        for(int i= 0; i<bottom[0]->num(); i++) sampleWv[i] = Dtype(1);
    }
    caffe_gpu_sub(
      count,
      bottom[0]->gpu_data(),  // a
      bottom[1]->gpu_data(),  // p
      diff_ap_.mutable_gpu_data());  // a_i-p_i
    caffe_gpu_sub(
      count,
      bottom[0]->gpu_data(),  // a
      bottom[2]->gpu_data(),  // n
      diff_an_.mutable_gpu_data());  // a_i-n_i
    caffe_gpu_sub(
      count,
      bottom[1]->gpu_data(),  // p
      bottom[2]->gpu_data(),  // n
      diff_pn_.mutable_gpu_data());  // p_i-n_i

    caffe_gpu_powx(
      count,
      diff_ap_.mutable_gpu_data(),  // a_i-p_i
      Dtype(2),
      diff_sq_ap_.mutable_gpu_data());  // (a_i-p_i)^2
    caffe_gpu_gemv(
      CblasNoTrans,
      bottom[0]->num(),
      bottom[0]->channels(),
      Dtype(1.0),                                         //alpha
      diff_sq_ap_.gpu_data(),  // (a_i-p_i)^2                // A
      summer_vec_.gpu_data(),                             // x
      Dtype(0.0),                                         //belta
      dist_sq_ap_.mutable_gpu_data());  // \Sum (a_i-p_i)^2  //y

    caffe_gpu_powx(
      count,
      diff_an_.mutable_gpu_data(),  // a_i-n_i
      Dtype(2),
      diff_sq_an_.mutable_gpu_data());  // (a_i-n_i)^2
    caffe_gpu_gemv(
      CblasNoTrans,
      bottom[0]->num(),
      bottom[0]->channels(),
      Dtype(1.0),                                         //alpha
      diff_sq_an_.gpu_data(),  // (a_i-n_i)^2                // A
      summer_vec_.gpu_data(),                             // x
      Dtype(0.0),                                         //belta
      dist_sq_an_.mutable_gpu_data());  // \Sum (a_i-n_i)^2  //y

    Dtype margin = this->layer_param_.triplet_loss_param().margin();
    Dtype loss(0.0);
    for (int i = 0; i < bottom[0]->num(); ++i) {
      loss += sampleWv[i]*std::max(margin + dist_sq_ap_.cpu_data()[i] - dist_sq_an_.cpu_data()[i], Dtype(0.0));
    }
    loss = loss / static_cast<Dtype>(bottom[0]->num()) / Dtype(2);
    top[0]->mutable_cpu_data()[0] = loss;
  }

  template <typename Dtype>
  __global__ void CLLBackward(const int count, const int channels,
                              const Dtype margin, const Dtype alpha,
                              const Dtype* diff, const Dtype* dist_sq_ap_, const Dtype* dist_sq_an_,
                              Dtype *sampleWv_cuda, Dtype *bottom_diff) {
    CUDA_KERNEL_LOOP(i, count) {
      int n = i / channels;  // the num index, to access dist_sq_ap_ and dist_sq_an_
      Dtype mdist(0.0);
      mdist = margin + dist_sq_ap_[n] - dist_sq_an_[n];
      if (mdist > 0.0) {
        bottom_diff[i] = alpha*sampleWv_cuda[n]*diff[i];
        // bottom_diff[i] = alpha*diff[i];
      }
      else {
        bottom_diff[i] = 0;
      }
    }
  }

  template <typename Dtype>
  void TripletLossLayer<Dtype>::Backward_gpu(const vector<Blob<Dtype>*>& top,
                                             const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
    Dtype margin = this->layer_param_.triplet_loss_param().margin();
    const int count = bottom[0]->count();
    const int channels = bottom[0]->channels();
    // the weight triplet loss 
    Dtype* sampleWv = NULL;
    Blob<Dtype> sampleWv_Blob;
    if(bottom.size() == 4)
    {
        sampleWv = bottom[3]->mutable_gpu_data();
    }else
    {
        sampleWv_Blob.Reshape(bottom[0]->num(), 1, 1, 1);
        sampleWv = sampleWv_Blob.mutable_cpu_data();
        for(int i= 0; i<bottom[0]->num(); i++) sampleWv[i] = Dtype(1);
        sampleWv = sampleWv_Blob.mutable_gpu_data();
    }

    for (int i = 0; i < 3; ++i) {
      if (propagate_down[i]) {
        const Dtype sign = (i < 2) ? -1 : 1;
        const Dtype alpha = sign * top[0]->cpu_diff()[0] /
          static_cast<Dtype>(bottom[0]->num());
        if (i == 0) {
          // NOLINT_NEXT_LINE(whitespace/operators)
          CLLBackward<Dtype> << <CAFFE_GET_BLOCKS(count), CAFFE_CUDA_NUM_THREADS >> >(
            count, channels, margin, alpha,
            diff_pn_.gpu_data(),  // the cached eltwise difference between p and n
            dist_sq_ap_.gpu_data(),  // the cached square distance between a and p
            dist_sq_an_.gpu_data(),  // the cached square distance between a and n
            sampleWv, // the sample's weight
            bottom[i]->mutable_gpu_diff());
          CUDA_POST_KERNEL_CHECK;
        }
        else if (i == 1) {
          // NOLINT_NEXT_LINE(whitespace/operators)
          CLLBackward<Dtype> << <CAFFE_GET_BLOCKS(count), CAFFE_CUDA_NUM_THREADS >> >(
            count, channels, margin, alpha,
            diff_ap_.gpu_data(),  // the cached eltwise difference between a and p
            dist_sq_ap_.gpu_data(),  // the cached square distance between a and p
            dist_sq_an_.gpu_data(),  // the cached square distance between a and n
            sampleWv, // the sample's weight
            bottom[i]->mutable_gpu_diff());

          CUDA_POST_KERNEL_CHECK;
        }
        else if (i == 2) {
          // NOLINT_NEXT_LINE(whitespace/operators)
          CLLBackward<Dtype> << <CAFFE_GET_BLOCKS(count), CAFFE_CUDA_NUM_THREADS >> >(
            count, channels, margin, alpha,
            diff_an_.gpu_data(),  // the cached eltwise difference between a and n
            dist_sq_ap_.gpu_data(),  // the cached square distance between a and p
            dist_sq_an_.gpu_data(),  // the cached square distance between a and n
            sampleWv, // the weight's wight
            bottom[i]->mutable_gpu_diff());
          CUDA_POST_KERNEL_CHECK;

        } // end if
      } // end propagation[i]
    } // end for i=1:3
    // release the resource, automally
  }

  INSTANTIATE_LAYER_GPU_FUNCS(TripletLossLayer);

}  // namespace caffe
