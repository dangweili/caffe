#ifndef CAFFE_ONLINEPAIR_LOSS_LAYER_HPP_
#define CAFFE_ONLINEPAIR_LOSS_LAYER_HPP_

#include <vector>
#include "caffe/blob.hpp"
#include "caffe/layer.hpp"
#include "caffe/proto/caffe.pb.h"
#include <vector>
#include "caffe/layers/loss_layer.hpp"
#include "caffe/util/other_functions.hpp"
using namespace std;
namespace caffe {

template <typename Dtype>
class RandomPairLossLayer : public LossLayer<Dtype> {
 public:
  explicit RandomPairLossLayer(const LayerParameter& param)
      : LossLayer<Dtype>(param) {}
  virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top);

  virtual inline int ExactNumBottomBlobs() const { return 2; }
  virtual inline const char* type() const { return "RandomPairLoss"; }
  /**
   * Unlike most loss layers, in the ContrastiveLossLayer we can backpropagate
   * to the first two inputs.
   */
  virtual inline bool AllowForceBackward(const int bottom_index) const {
    return bottom_index != 1;
  }

 protected:
  /// @copydoc ContrastiveLossLayer
  virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top);
  virtual void Forward_gpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top);
  virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom);
  virtual void Backward_gpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom);  

  Blob<Dtype> diff_; // tmp for diff
  Blob<Dtype> summer_vec_;  // tmp storage for gpu forward pass
  vector<PairDist> pairdist_pos_;
  vector<PairDist> pairdist_neg_;
  // Blob<Dtype> weight_; // the coefficient of each examples
  shared_ptr<Caffe::RNG> prefetch_rng_;

};

}  // namespace caffe

#endif  // CAFFE_RANDOMPAIR_LOSS_LAYER_HPP_
