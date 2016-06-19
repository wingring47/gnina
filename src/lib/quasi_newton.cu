/*

   Copyright (c) 2006-2010, The Scripps Research Institute

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   Author: Dr. Oleg Trott <ot14@columbia.edu>, 
           The Olson Lab, 
           The Scripps Research Institute

*/

#include "quasi_newton.h"
#include "bfgs.h"
#include "gpu_util.h"
#include <cuda_runtime.h>

struct quasi_newton_aux {
	model* m;
	const precalculate* p;
	const igrid* ig;
	const vec v;
	const grid* user_grid;
	quasi_newton_aux(model* m_, const precalculate* p_, const igrid* ig_,
                     const vec& v_, const grid* user_grid_)
        : m(m_),
          p(p_),
          ig(ig_),
          v(v_),
          user_grid(user_grid_) {}
    
	fl operator()(const conf& c, change& g) {
		return m->eval_deriv(*p, *ig, v, c, g, *user_grid);
	}
};

void quasi_newton::operator()(model& m, const precalculate& p,
                              const igrid& ig, output_type& out,
                              change& g, const vec& v, const grid& user_grid) const
{ // g must have correct size
    m.coords[0].pad[0] = 0x666;
	quasi_newton_aux aux(&m, &p, &ig, v, &user_grid);
	// check whether we're using the gpu for the minimization algorithm. if so, malloc and
	// copy change, out, aux, and model here. then launch the bfgs kernel
	if (gpu_on) {
		change* c = NULL; 
		cudaMalloc(&c, sizeof(*c));
		cudaMemcpy(c, &g, sizeof(*c), cudaMemcpyHostToDevice);

		output_type* outgpu = NULL;
		cudaMalloc(&outgpu, sizeof(*outgpu));
		cudaMemcpy(outgpu, &out, sizeof(*outgpu), cudaMemcpyHostToDevice);

		quasi_newton_aux* aux_gpu = NULL;
		cudaMalloc(&aux_gpu, sizeof(*aux_gpu));
		cudaMemcpy(aux_gpu, &aux, sizeof(*aux_gpu), cudaMemcpyHostToDevice);

		model* m_gpu = NULL;
		cudaMalloc(&m_gpu, sizeof(*m_gpu));
		cudaMemcpy(m_gpu, &m, sizeof(*m_gpu), cudaMemcpyHostToDevice);	

		bfgs_kernel<<<dim3(1), 1>>>(aux_gpu, &outgpu->c, &outgpu->e, c, average_required_improvement, params);
		cudaThreadSynchronize();
		abort_on_gpu_err();
		cudaMemcpy(&out, outgpu, sizeof(outgpu), cudaMemcpyDeviceToHost);
		// set out.e on the device since the kernel can't have a return value
	}
	else {
		fl res = bfgs(aux, out.c, g, average_required_improvement, params);
		out.e = res;
	}
}
