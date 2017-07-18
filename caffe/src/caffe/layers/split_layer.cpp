#include <vector>

#include "caffe/layers/split_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void SplitLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  count_ = bottom[0]->count();
  for (int i = 0; i < top.size(); ++i) {
    // Do not allow in-place computation in the SplitLayer.  Instead, share data
    // by reference in the forward pass, and keep separate diff allocations in
    // the backward pass.  (Technically, it should be possible to share the diff
    // blob of the first split output with the input, but this seems to cause
    // some strange effects in practice...)
    CHECK_NE(top[i], bottom[0]) << this->type() << " Layer does not "
        "allow in-place computation.";
    top[i]->ReshapeLike(*bottom[0]);
    CHECK_EQ(count_, top[i]->count());
  }
}

template <typename Dtype>
void SplitLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  for (int i = 0; i < top.size(); ++i) {
    top[i]->ShareData(*bottom[0]);
  }
}

template <typename Dtype>
void SplitLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  if (!propagate_down[0]) { return; }
  if (top.size() == 1) {
    caffe_copy(count_, top[0]->cpu_diff(), bottom[0]->mutable_cpu_diff());
    return;
  }
  caffe_add(count_, top[0]->cpu_diff(), top[1]->cpu_diff(),
            bottom[0]->mutable_cpu_diff());
  // Add remaining top blob diffs.
  for (int i = 2; i < top.size(); ++i) {
    const Dtype* top_diff = top[i]->cpu_diff();
    Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
    caffe_axpy(count_, Dtype(1.), top_diff, bottom_diff);
  }
}

//determines which (affinity or pose prediction) is propagated down to the shared
//part of the network
template <typename Dtype>
void SplitLayer<Dtype>::Backward_relevance_split(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom,
      const int blob_to_propagate)
{
    float sum = 0;
    for (int i = 0; i < top[0]->count(); ++i)
    {
        sum += top[0]->cpu_diff()[i];
    }
    std::cout << "SPLIT TOP[0] SUM: " << sum << '\n';

    const Dtype* top_diff;
    Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
    int top_count;
    
    if(blob_to_propagate == 1)
    {
        top_diff = top[1]->cpu_diff();
        Blob<Dtype> buffer_blob((top[1])->shape());
        top_count = buffer_blob.count();
    }
    else if(blob_to_propagate == 0)
    {
        top_diff = top[0]->cpu_diff();
        Blob<Dtype> buffer_blob((top[0])->shape());
        top_count = buffer_blob.count();
    }
    else
    {
        std::cout << "invalid blob to propagate, aborting...\n";
        abort();
    }

    //copy desired blob to bottom diff
    for(int i = 0; i < top_count; i++)
    {
        bottom_diff[i] = top_diff[i];
    }

    sum = 0;
    for (int i = 0; i < top[0]->count(); ++i)
    {
        sum += top[1]->cpu_diff()[i];
    }
    std::cout << "SPLIT TOP[1] SUM: " << sum << '\n';

    /*
    std::cout << "split top[0,0]: " << top[0]->cpu_diff()[0] << '\n';
    std::cout << "split top[0,1]: " << top[0]->cpu_diff()[1] << '\n';
    std::cout << "split top[1,0]: " << top[1]->cpu_diff()[0] << '\n';
    std::cout << "split top[1,1]: " << top[1]->cpu_diff()[1] << '\n';
    */

    sum = 0;
    for (int i = 0; i < bottom[0]->count(); ++i)
    {
        sum += bottom[0]->cpu_diff()[i];
    }
    std::cout << "SPLIT BOTTOM SUM: " << sum << '\n';
}


template <typename Dtype>
void SplitLayer<Dtype>::Backward_relevance(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom)
{
    float sum = 0;
    for (int i = 0; i < top[0]->count(); ++i)
    {
        sum += top[0]->cpu_diff()[i];
    }
    std::cout << "SPLIT TOP SUM: " << sum << '\n';
    
    //take average
    unsigned n = bottom[0]->count();
    Dtype *bottom_diff = bottom[0]->mutable_cpu_diff();
    caffe_copy(n, top[0]->cpu_diff(), bottom_diff);
    for(unsigned i = 1, ntop = top.size(); i < ntop; i++) {
    	caffe_axpy<Dtype>(n, 1.0, top[i]->cpu_diff(), bottom_diff);
    }

    //in our network this is merging softmax and softmaxwithloss, only one of which is
    //set see wee take the sum instead of the average
    //average
    //caffe_scal<Dtype>(n, 1.0/top.size(), bottom_diff);

    sum = 0;
    for (int i = 0; i < bottom[0]->count(); ++i)
    {
        sum += bottom[0]->cpu_diff()[i];
    }
    std::cout << "SPLIT BOTTOM SUM: " << sum << '\n';
}
#ifdef CPU_ONLY
STUB_GPU(SplitLayer);
#endif

INSTANTIATE_CLASS(SplitLayer);
REGISTER_LAYER_CLASS(Split);

}  // namespace caffe
