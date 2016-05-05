#ifndef CAFFE_TRIPLET_LOSS_LAYER_HPP_
#define CAFFE_TRIPLET_LOSS_LAYER_HPP_

#include <vector>

#include "caffe/blob.hpp"
#include "caffe/layer.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/layers/loss_layer.hpp"

namespace caffe {
template <typename Dtype>
class TripletDExpLossLayer: public Layer<Dtype> {
    public:
        explicit TripletDExpLossLayer(const LayerParameter& param)
            : Layer<Dtype>(param) {}
        virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom, 
            const vector<Blob<Dtype>*>& top);
        virtual void Reshape(
            const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {};
        // virtual inline int ExactNumBottomBlobs() const { return 3; }
        virtual inline int MinBottomBlobs() const {return 3; }
        virtual inline int MaxBottomBlobs() const {return 4; }
        virtual inline const char* type() const { return "TripletDExpLoss"; }
        virtual inline bool AutoTopBlobs() const { return true; }
        /*
            just backward one layer
        */ 
        virtual inline bool AllowForceBackward(const int bottom_index) const {
            return bottom_index != 3;
        }      
    protected:
        virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom, 
                const vector<Blob<Dtype>*>& top);
        virtual void Forward_gpu(const vector<Blob<Dtype>*>& bottom,
                const vector<Blob<Dtype>*>& top);

        virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
                const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom);

        virtual void Backward_gpu(const vector<Blob<Dtype>*>& top,
                const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom);

        Blob<Dtype> diff_ap_; // cache for backward pass
        Blob<Dtype> diff_an_; // cache for backward pass
        Blob<Dtype> diff_pn_; // cache for backward pass
        
        Blob<Dtype> diff_sq_ap_; // cache for backward pass
        Blob<Dtype> diff_sq_an_; // cache for backward pass
        
        Blob<Dtype> dist_sq_ap_; // cache for backward pass
        Blob<Dtype> dist_sq_an_; // cache for backward pass

        Blob<Dtype> exp_weight_; // this is for cache the exp(delta)
        
        Blob<Dtype> summer_vec_; // tmp storage for gpu forward pass
        Blob<Dtype> dist_binary_; // tmp storage for gpu forward pass
        
        Blob<Dtype> d_diff_ap_; //cache for backward pass 
        Blob<Dtype> d_diff_an_;
        Blob<Dtype> d_diff_pn_;
        Blob<Dtype> d_diff_sq_ap_;
        Blob<Dtype> d_diff_sq_an_;
        Blob<Dtype> d_dist_sq_ap_; //cache for backward pass
        Blob<Dtype> d_dist_sq_an_;
        // Blob<Dtype> dexp_weight_; // this is for cache the exp(delta)
};


}  // namespace caffe

#endif  // CAFFE_TRIPLET_LOSS_LAYER_HPP_