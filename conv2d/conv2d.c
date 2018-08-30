/* 
 * Authors: Eric Flamand, GreenWaves Technologies (eric.flamand@greenwaves-technologies.com)
 */

#include <stdio.h>
#include "conv2d.h"

static void plp_conv5x5_u8_norm(uint8_t * __restrict__ In, uint8_t * __restrict__ coeffs, uint8_t * __restrict__ Out, unsigned int W, unsigned int H, int NormFactor, int NormShift)
{
	unsigned int i, j;
	v4qu V0, V1, V2, V3, V4, V5, V6;
  /* Move all Kernel coeffs into registers */
	v4qu C0 = *(v4qu *)&coeffs[0], C1 = *(v4qu *)&coeffs[5], C2 = *(v4qu *)&coeffs[10], C3 = *(v4qu *)&coeffs[15], C4 = *(v4qu *)&coeffs[20], C5 = (v4qu) {coeffs[4], coeffs[9], coeffs[14], coeffs[19]}, C6 = (v4qu) {coeffs[24], 0, 0, 0};

  /* Shuffling mask for the delay line: Shuffle((1 2 3 4) (5 6 7 8), Mask)  = (2 3 4 5) */
	v4qu Mask  = {1,2,3,4};
	v4qu *VectIn;
	uint8_t *O;
	unsigned int S;
	int CoreId;
	int nbCores = rt_nb_pe();
	unsigned int First, Last, Chunk;

  /* Assumption: W%4==0 for proper vectorial behaviour, if not image should be padded */

	CoreId = rt_core_id();

  // Round up the number of chunk in case it is not a multiple of the number of cores
  // In this case, the last core will process a smaller chunk but this won't change the
  // time needed as at least one core must execute the up-rounded chunk
	Chunk = (W-4 + nbCores - 1)/nbCores;
	First =  CoreId*Chunk; Last = First+Chunk;
	if (Last >= W-4) Last = W - 4;

	for (i=First; i<Last; i++) {
		V5 = (v4qu) {0, In[i+4],In[i+W+4], In[i+2*W+4]};
		VectIn = (v4qu *) (In + i);
		V1 = VectIn[0]; 		VectIn += (W>>2);
		V2 = VectIn[0]; 		VectIn += (W>>2);
		V3 = VectIn[0]; 		VectIn += (W>>2);
		V4 = VectIn[0]; V6 = VectIn[1]; VectIn += (W>>2);
		O = Out + i;
		for (j=0; j<((unsigned)H-4); j++) {
			V0 = V1; V1 = V2; V2 = V3; V3 = V4;
			V5 = __builtin_shuffle(V5, V6, Mask);
			V4 = *VectIn; VectIn++; V6 = *VectIn;
			VectIn += ((W>>2)-1);
			S = __DOTPU4   (V0, C0);
			S = __SUMDOTPU4(V1, C1, S); S = __SUMDOTPU4(V2, C2, S);
			S = __SUMDOTPU4(V3, C3, S); S = __SUMDOTPU4(V4, C4, S);
			S = __SUMDOTPU4(V5, C5, S); S = __SUMDOTPU4(V6, C6, S);
			*O = (S*NormFactor)>>NormShift;
			O+= (W-4);
		}
	}
}

static void plp_conv5x5_u8_norm_stub(void *_args)
{
  conv_args_t *args = (conv_args_t *)_args;
  plp_conv5x5_u8_norm(args->in, args->coeffs, args->out, args->W, args->H, args->NormFactor, args->NormShift);
}

// This function is entered on cluster side when the fabric controller
// is calling it remotly using rt_cluster_call.
// Only core 0 is entering it and can then use rt_team_fork to fork
// the execution on multiple cores.
void conv2d_cluster_entry(void *arg)
{
  conv_args_t *args = (conv_args_t *)arg;

  // This will make all available cores entering pe_entry, including
  // core 0 which is calling this function.
  // Core 0 will return from rt_team_fork only when all cores have returned
  // from pe_entry (there is an implicit barrier).
  rt_perf_t perf;
  rt_perf_init(&perf);
  rt_perf_conf(&perf, (1<<RT_PERF_CYCLES) | (1<<RT_PERF_INSTR)); 
  rt_perf_reset(&perf);
  rt_perf_start(&perf);
  
  rt_team_fork(0, plp_conv5x5_u8_norm_stub, arg);

  rt_perf_stop(&perf);

  args->time = rt_perf_read(RT_PERF_CYCLES);
}
