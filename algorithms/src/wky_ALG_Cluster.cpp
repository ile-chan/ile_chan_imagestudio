#include "wky_algorithm_support.h"
#include"wky_ALG_Cluster.h"
#include<math.h>
bool wkyClusterHistogram(int* histogram, int nClass, double decision, double* center) {
    if(nClass<2||nClass>256)return false;
    int low=0,high=255;while(low<256&&!histogram[low])++low;while(high>=0&&!histogram[high])--high;
    if(low>high)return false;
    for(int k=0;k<nClass;++k) center[k]=low+(high-low)*double(k)/(nClass-1);
    for(int iter=0;iter<500;++iter) {
        std::vector<double> sums(nClass,0),counts(nClass,0);double movement=0;
        for(int g=low;g<=high;++g) if(histogram[g]) {
            int best=0;for(int k=1;k<nClass;++k) if(std::abs(g-center[k])<std::abs(g-center[best]))best=k;
            sums[best]+=double(g)*histogram[g];counts[best]+=histogram[g];
        }
        for(int k=0;k<nClass;++k) if(counts[k]) {double next=sums[k]/counts[k];movement=std::max(movement,std::abs(next-center[k]));center[k]=next;}
        if(movement<=std::max(0.0001,decision))break;
    }
    std::sort(center,center+nClass);return true;
}

