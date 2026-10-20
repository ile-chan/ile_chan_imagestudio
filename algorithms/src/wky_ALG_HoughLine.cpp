#include "wky_algorithm_support.h"
#include"wky_ALG_HoughLine.h"
#include<math.h>
#include"string.h"
int wkyHoughLine(int width, int height, int* x, int* y, int N, double* A, double* B, double* C) {
    *A=*B=*C=0;if(N<=0)return 0;
    int radius=int(std::ceil(std::hypot(double(width),double(height))))+1,bins=radius*2+1;
    std::vector<int> counts(size_t(bins)*180,0);int best=0,bestTheta=0,bestRho=0;
    constexpr double pi=3.14159265358979323846;
    for(int theta=0;theta<180;++theta){double c=std::cos(theta*pi/180),s=std::sin(theta*pi/180);
        for(int i=0;i<N;++i){int rho=int(std::lround(x[i]*c+y[i]*s));if(rho<-radius||rho>radius)continue;
            int value=++counts[size_t(theta)*bins+rho+radius];if(value>best){best=value;bestTheta=theta;bestRho=rho;}}
    }
    *A=std::cos(bestTheta*pi/180);*B=std::sin(bestTheta*pi/180);*C=-bestRho;return best;
}

