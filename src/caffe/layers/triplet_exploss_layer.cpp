/*
* =====================================================================================
*
*       Filename:  triplet_loss_layer.cpp
*
*    Description:
*
*        Version:  1.0
*        Created:  2015?��08??07��? 16����31��?56??
*       Revision:  none
*       Compiler:  gcc
*
*         Author:  YuanYang (), bengouawu@gmail.com
*        Company:  SUNTEKPCI
*
* =====================================================================================
*/

/*
* triplet_loss_layer.cpp
*/

#include <algorithm>
#include <vector>

#include "caffe/layers/triplet_exploss_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

  template <typename Dtype>
  void TripletExpLossLayer<Dtype>::LayerSetUp(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
    Layer<Dtype>::LayerSetUp(bottom, top);
    CHECK_EQ(bottom[0]->num(), bottom[1]->num());
    CHECK_EQ(bottom[1]->num(), bottom[2]->num());
    CHECK_EQ(bottom[0]->channels(), bottom[1]->channels());
    CHECK_EQ(bottom[1]->channels(), bottom[2]->channels());
    CHECK_EQ(bottom[0]->height(), 1);
    CHECK_EQ(bottom[0]->width(), 1);
    CHECK_EQ(bottom[1]->height(), 1);
    CHECK_EQ(bottom[1]->width(), 1);
    CHECK_EQ(bottom[2]->height(), 1);
    CHECK_EQ(bottom[2]->width(), 1);

    /* bottom[3] is for sample's weight, decarded here */
    if (bottom.size() == 4)
    {
        CHECK_EQ(bottom[3]->num(), bottom[0]->num());
        CHECK_EQ(bottom[3]->channels(),1);
        CHECK_EQ(bottom[3]->height(), 1);
        CHECK_EQ(bottom[3]->width(), 1);
    }

    diff_ap_.Reshape(bottom[0]->num(), bottom[0]->channels(), 1, 1);
    diff_an_.Reshape(bottom[0]->num(), bottom[0]->channels(), 1, 1);
    diff_pn_.Reshape(bottom[0]->num(), bottom[0]->channels(), 1, 1);

    diff_sq_ap_.Reshape(bottom[0]->num(), bottom[0]->channels(), 1, 1);
    diff_sq_an_.Reshape(bottom[0]->num(), bottom[0]->channels(), 1, 1);
    dist_sq_ap_.Reshape(bottom[0]->num(), 1, 1, 1);
    dist_sq_an_.Reshape(bottom[0]->num(), 1, 1, 1);

    // 
    exp_weight_.Reshape(bottom[0]->num(), 1, 1, 1);
    for (int i=0; i< bottom[0]->num(); i++)
        exp_weight_.mutable_cpu_data()[i] = Dtype(1);
    // vector of ones used to sum along channels
    summer_vec_.Reshape(bottom[0]->channels(), 1, 1, 1);
    for (int i = 0; i < bottom[0]->channels(); ++i)
      summer_vec_.mutable_cpu_data()[i] = Dtype(1);
    dist_binary_.Reshape(bottom[0]->num(), 1, 1, 1);
    for (int i = 0; i < bottom[0]->num(); ++i)
      dist_binary_.mutable_cpu_data()[i] = Dtype(1);
    // top reshape
    top[0]->Reshape(1, 1, 1, 1);
  }

  template <typename Dtype>
  void TripletExpLossLayer<Dtype>::Forward_cpu(
    const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
    int count = bottom[0]->count();
    
    Dtype* sampleWv = NULL;
    Dtype* exp_weight = exp_weight_.mutable_cpu_data();
    Blob<Dtype> sampleWv_Blob;
    Dtype sampleW = Dtype(0.0);
    if(bottom.size() == 4)
    {
        sampleWv = bottom[3]->mutable_cpu_data();
    }else
    {
        sampleWv_Blob.Reshape(bottom[0]->num(),1,1,1);
        sampleWv = sampleWv_Blob.mutable_cpu_data();
        for(int i= 0; i<bottom[0]->num(); i++) sampleWv[i] = Dtype(1);
    }
    caffe_sub(
      count,
      bottom[0]->cpu_data(),  // a
      bottom[1]->cpu_data(),  // p
      diff_ap_.mutable_cpu_data());  // a_i-p_i
    caffe_sub(
      count,
      bottom[0]->cpu_data(),  // a
      bottom[2]->cpu_data(),  // n
      diff_an_.mutable_cpu_data());  // a_i-n_i
    caffe_sub(
      count,
      bottom[1]->cpu_data(),  // p
      bottom[2]->cpu_data(),  // n
      diff_pn_.mutable_cpu_data());  // p_i-n_i
    const int channels = bottom[0]->channels();
    Dtype margin = this->layer_param_.triplet_exploss_param().margin();
   
    Dtype loss(0.0);
    for (int i = 0; i < bottom[0]->num(); ++i) {
      dist_sq_ap_.mutable_cpu_data()[i] = caffe_cpu_dot(channels,
                                                        diff_ap_.cpu_data() + (i*channels), diff_ap_.cpu_data() + (i*channels));
      dist_sq_an_.mutable_cpu_data()[i] = caffe_cpu_dot(channels,
                                                        diff_an_.cpu_data() + (i*channels), diff_an_.cpu_data() + (i*channels));
      sampleW = sampleWv[i];
      Dtype mdist = std::max(margin + dist_sq_ap_.cpu_data()[i] - dist_sq_an_.cpu_data()[i], Dtype(0.0));
      mdist = mdist/2;
      // compute the exp weight, which is just for loss function
      caffe_exp(1, &mdist, &(exp_weight[i]));
      // weight the mdist 
      mdist = sampleW*(exp_weight[i]-1.0);
      loss += mdist;
      if (mdist < Dtype(1e-9)) {
        //dist_binary_.mutable_cpu_data()[i] = Dtype(0);
        //prepare for backward pass
        caffe_set(channels, Dtype(0), diff_ap_.mutable_cpu_data() + (i*channels));
        caffe_set(channels, Dtype(0), diff_an_.mutable_cpu_data() + (i*channels));
        caffe_set(channels, Dtype(0), diff_pn_.mutable_cpu_data() + (i*channels));
      }
    }
    loss = loss / static_cast<Dtype>(bottom[0]->num());
    top[0]->mutable_cpu_data()[0] = loss;
    // free the weight, automally in Blob layer
  }

  template <typename Dtype>
  void TripletExpLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
                                             const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
    //Dtype margin = this->layer_param_.contrastive_loss_param().margin();
    Dtype* sampleWv = NULL;
    Dtype* exp_weight = exp_weight_.mutable_cpu_data();
    Dtype sampleW = Dtype(0.0);
    Blob<Dtype> sampleWv_Blob;
    if(bottom.size() == 4)
    {
        sampleWv = bottom[3]->mutable_cpu_data();
    }else
    {
        sampleWv_Blob.Reshape(bottom[0]->num(),1,1,1);
        sampleWv = sampleWv_Blob.mutable_cpu_data();
        for(int i= 0; i<bottom[0]->num(); i++) sampleWv[i] = Dtype(1);
    }

    for (int i = 0; i < 3; ++i) {
      if (propagate_down[i]) {
        const Dtype sign = (i < 2) ? -1 : 1;
        const Dtype alpha = sign * top[0]->cpu_diff()[0] /
          static_cast<Dtype>(bottom[i]->num());
        int num = bottom[i]->num();
        int channels = bottom[i]->channels();
        for (int j = 0; j < num; ++j) {
          // take the weight into consider
          sampleW = sampleWv[j]*exp_weight[j];
          Dtype* bout = bottom[i]->mutable_cpu_diff();
          if (i == 0) {  // a
            //if(dist_binary_.cpu_data()[j]>Dtype(0)){
            caffe_cpu_axpby(
              channels,
              alpha*sampleW,
              diff_pn_.cpu_data() + (j*channels),
              Dtype(0.0),
              bout + (j*channels));
            //}else{
            //  caffe_set(channels, Dtype(0), bout + (j*channels));
            //}
          }
          else if (i == 1) {  // p
            //if(dist_binary_.cpu_data()[j]>Dtype(0)){
            caffe_cpu_axpby(
              channels,
              alpha*sampleW,
              diff_ap_.cpu_data() + (j*channels),
              Dtype(0.0),
              bout + (j*channels));
            //}else{
            //	  caffe_set(channels, Dtype(0), bout + (j*channels));
            //}
          }
          else if (i == 2) {  // n
            //if(dist_binary_.cpu_data()[j]>Dtype(0)){
            caffe_cpu_axpby(
              channels,
              alpha*sampleW,
              diff_an_.cpu_data() + (j*channels),
              Dtype(0.0),
              bout + (j*channels));
            //}else{
            //   caffe_set(channels, Dtype(0), bout + (j*channels));
            //}
          }
        } // for num
      } //if propagate_down[i]
    } //for i
    // free the wight buffer, auto
  }

#ifdef CPU_ONLY
  STUB_GPU(TripletExpLossLayer);
#endif

  INSTANTIATE_CLASS(TripletExpLossLayer);
  REGISTER_LAYER_CLASS(TripletExpLoss);

}  // namespace caffe
