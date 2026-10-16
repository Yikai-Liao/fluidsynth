#include <cstdio>
#include "hiir/PolyphaseIir2Designer.h"
int main(){double t=(22050.0-16500.0)/88200.0; printf("transition %.9f requested-count %d\n",t,hiir::PolyphaseIir2Designer::compute_nbr_coefs_from_proto(80.0,t)); for(int n=1;n<=16;n++) printf("n=%d attenuation=%.6f\n",n,hiir::PolyphaseIir2Designer::compute_atten_from_order_tbw(n,t));}
