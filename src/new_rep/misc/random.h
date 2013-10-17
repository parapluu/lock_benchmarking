#ifndef RANDOM_H
#define RANDOM_H

#include "stdlib.h"


double random_double(unsigned int *seed_ptr){
    double randomDouble = (double)rand_r(seed_ptr);
    return randomDouble/RAND_MAX;
}


#endif
