/* 
 * Authors: Eric Flamand, GreenWaves Technologies (eric.flamand@greenwaves-technologies.com)
 */

#include <stdio.h>
#include "conv2d.h"

int main()
{
  printf("Entering main controller\n");

  int height = 64;
  int width = 64;

  rt_cluster_mount(1, 0, 0, NULL);

  conv_args_t *args = rt_alloc(RT_ALLOC_CL_DATA, sizeof(conv_args_t));
  if (args == NULL) return -1;

  args->in = rt_alloc(RT_ALLOC_CL_DATA, height*width);
  args->out = rt_alloc(RT_ALLOC_CL_DATA, height*width);
  args->coeffs = rt_alloc(RT_ALLOC_CL_DATA, 5*5);
  if (args->in == NULL || args->out == NULL || args->coeffs == NULL) return -1;
  
  memset(args->in, 0, width*height);
  memset(args->out, 0, width*height);
  memset(args->coeffs, 0, 5*5);

  args->W = width;
  args->H = height;
  args->NormFactor = 1;
  args->NormShift = 0;

  rt_cluster_call(NULL, 0, conv2d_cluster_entry, (void *)args, NULL, 0, 0, 0, NULL);

  int time = args->time * 1000 / args->W / args->H;
  printf("Cycles per pixel: %d.%03d\n", time/1000, time%1000);

  rt_cluster_mount(0, 0, 0, NULL);

  return 0;
}
