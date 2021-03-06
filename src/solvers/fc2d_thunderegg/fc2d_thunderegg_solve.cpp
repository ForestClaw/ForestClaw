/*
Copyright (c) 2019 Carsten Burstedde, Donna Calhoun, Scott Aiton, Grady Wright
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "fc2d_thunderegg_solve.h"

#include "fc2d_thunderegg.h"
#include "fc2d_thunderegg_options.h"
#include "fc2d_thunderegg_vector.hpp"

#include <fclaw2d_elliptic_solver.h>

#include <fclaw2d_clawpatch.h>
#include <fclaw2d_clawpatch_options.h>
#include <fclaw2d_clawpatch_output_ascii.h>
#include <fclaw2d_clawpatch_output_vtk.h>

#include <fclaw2d_global.h>
#include <fclaw2d_map.h>
#include <fclaw2d_map_brick.h>
#include <fclaw2d_options.h>
#include <fclaw2d_patch.h>
#include <fclaw2d_vtable.h>

#include <p4est_bits.h>
#include <p4est_wrap.h>

#include <ThunderEgg/BiCGStab.h>
#include <ThunderEgg/BiCGStabPatchSolver.h>
#include <ThunderEgg/VarPoisson/StarPatchOperator.h>
#include <ThunderEgg/Poisson/FFTWPatchSolver.h>
#include <ThunderEgg/GMG/LinearRestrictor.h>
#include <ThunderEgg/GMG/DirectInterpolator.h>
#include <ThunderEgg/P4estDomainGenerator.h>
#include <ThunderEgg/GMG/CycleBuilder.h>
#include <ThunderEgg/BiLinearGhostFiller.h>
#include <ThunderEgg/ValVectorGenerator.h>

#define USE_FIVEPOINT 1

using namespace std;
using namespace ThunderEgg;
using namespace ThunderEgg::VarPoisson;

double beta_coeff(const std::array<double,2>& coord)
{
    return 1.0;
}  


class fivePoint : public PatchOperator<2>
{
public:
    fivePoint(std::shared_ptr<const Domain<2>>      domain,
              std::shared_ptr<const GhostFiller<2>> ghost_filler);

    void applySinglePatch(std::shared_ptr<const PatchInfo<2>> pinfo, 
                          const std::vector<LocalData<2>>& us,
                          std::vector<LocalData<2>>& fs) const override;
    void addGhostToRHS(std::shared_ptr<const PatchInfo<2>> pinfo, 
                       const std::vector<LocalData<2>>& us,
                       std::vector<LocalData<2>>& fs) const override;

};


#if 0
/* Store beta as member in fivePoint */
void fivePoint::fivePoint>(beta_vec,te_domain,ghost_filler);
#endif


fivePoint::fivePoint(std::shared_ptr<const Domain<2>>      domain,
                     std::shared_ptr<const GhostFiller<2>> ghost_filler) : PatchOperator<2>(domain,ghost_filler)
{
}


void fivePoint::applySinglePatch(std::shared_ptr<const PatchInfo<2>> pinfo, 
                                 const std::vector<LocalData<2>>& us,
                                 std::vector<LocalData<2>>& fs) const 
{
    //const cast since u ghost values have to be modified
    //ThunderEgg doesn't care if ghost values are modified, just don't modify the interior values.
    LocalData<2>& u = const_cast<LocalData<2>&>(us[0]);
    LocalData<2>& f = fs[0];

    int mx = pinfo->ns[0]; 
    int my = pinfo->ns[1];

#if 0    
    int mbc = pinfo->num_ghost_cells;
    double xlower = pinfo->starts[0];
    double ylower = pinfo->starts[1];
    double dy = pinfo->spacings[1];
#endif    
    double dx = pinfo->spacings[0];

    /* Apply homogeneous physical boundary conditions */
    for(int j = 0; j < my; j++)
    {
        /* bool hasNbr(Side<D> s) */
        if (!pinfo->hasNbr(Side<2>::west()))
        {
            u[{-1,j}] = -u[{0,j}];
        }
        //u[{-1,j}] = -u[{0,j}];

        if (!pinfo->hasNbr(Side<2>::east()))
        {
            u[{mx,j}] = -u[{mx-1,j}];;
        }
        //u[{mx,j}] = -u[{mx-1,j}];;
    }

    for(int i = 0; i < mx; i++)
    {
        if (!pinfo->hasNbr(Side<2>::south()))
        {
            u[{i,-1}] = -u[{i,0}];
        }
        //u[{i,-1}] = -u[{i,0}];
        if (!pinfo->hasNbr(Side<2>::north()))
        {
            u[{i,my}] = -u[{i,my-1}];
        }
        //u[{i,my}] = -u[{i,my-1}];
    }


#if 1
    //if physical boundary
    if (!pinfo->hasNbr(Side<2>::west())){
        auto ghosts = u.getGhostSliceOnSide(Side<2>::west(),1);
        for(int j = 0; j < my; j++){
            ghosts[{j}] = -u[{0,j}];
        }
    }
    if (!pinfo->hasNbr(Side<2>::east())){
        auto ghosts = u.getGhostSliceOnSide(Side<2>::east(),1);
        for(int j = 0; j < my; j++){
            ghosts[{j}] = -u[{mx-1,j}];
        }
    }

    if (!pinfo->hasNbr(Side<2>::south())){
        auto ghosts = u.getGhostSliceOnSide(Side<2>::south(),1);
        for(int i = 0; i < mx; i++){
            ghosts[{i}] = -u[{i,0}];
        }
    }
    if (!pinfo->hasNbr(Side<2>::north())){
        auto ghosts = u.getGhostSliceOnSide(Side<2>::north(),1);
        for(int i = 0; i < mx; i++){
            ghosts[{i}] = -u[{i,my-1}];
        }
    }
#endif

    for(int i = 0; i < mx; i++)
    {
        for(int j = 0; j < my; j++)
        {
            f[{i,j}] = (u[{i+1,j}] + u[{i-1,j}] + u[{i,j+1}] + u[{i,j-1}] - 4*u[{i,j}])/(dx*dx);
        }
    }
}

#if 0
void fivePoint::apply(std::shared_ptr<const Vector<2>> u, std::shared_ptr<Vector<2>> f) const 
{
    //ghost_filler->fillGhost(u);
    for (auto pinfo : domain->getPatchInfoVector()) {
        applySinglePatch(pinfo, u->getLocalData(pinfo->local_index),
                         f->getLocalData(pinfo->local_index));
    }
}
#endif


void fivePoint::addGhostToRHS(std::shared_ptr<const PatchInfo<2>> pinfo, 
                              const std::vector<LocalData<2>>& us, 
                              std::vector<LocalData<2>>& fs) const 
{
    const LocalData<2>& u = us[0];
    LocalData<2>& f = fs[0];

    int mx = pinfo->ns[0]; 
    int my = pinfo->ns[1];
#if 0    
    int mbc = pinfo->num_ghost_cells;
    double xlower = pinfo->starts[0];
    double ylower = pinfo->starts[1];
#endif    
    double dx = pinfo->spacings[0];
    double dx2 = dx*dx;

    double dy = pinfo->spacings[1];
    double dy2 = dy*dy;

    //return;

    for(int j = 0; j < my; j++)
    {
        /* bool hasNbr(Side<D> s) */
        if (pinfo->hasNbr(Side<2>::west()))
        {
            f[{0,j}] += -(u[{-1,j}]+u[{0,j}])/dx2;
        }

        if (pinfo->hasNbr(Side<2>::east()))
        {
            f[{mx-1,j}] += -(u[{mx-1,j}]+u[{mx,j}])/dx2;
        }
    }

    for(int i = 0; i < mx; i++)
    {
        if (pinfo->hasNbr(Side<2>::south()))
        {
            f[{i,0}] += -(u[{i,-1}]+u[{i,0}])/dy2;
        }

        if (pinfo->hasNbr(Side<2>::north()))
        {
            f[{i,my-1}] += -(u[{i,my-1}]+u[{i,my}])/dy2;
        }
    }
}
 

#if USE_FIVEPOINT == 0
/**
 * @brief Restrict the beta coefficient vector for the new domain
 * 
 * @param prev_beta_vec the finer beta vector
 * @param prev_domain the previous (finer) domain
 * @param curr_domain the current (coarser) domain
 * @return shared_ptr<Vector<2>> the restricted beta vector
 */
shared_ptr<ValVector<2>> restrict_beta_vec(shared_ptr<Vector<2>> prev_beta_vec, 
                                        shared_ptr<Domain<2>> prev_domain, 
                                        shared_ptr<Domain<2>> curr_domain)
{
    GMG::LinearRestrictor<2> restrictor(prev_domain,curr_domain, prev_beta_vec->getNumComponents(), true);
    auto new_beta_vec = ValVector<2>::GetNewVector(curr_domain, prev_beta_vec->getNumComponents());
    restrictor.restrict(new_beta_vec, prev_beta_vec);
    return new_beta_vec;
}
#endif

void fc2d_thunderegg_solve(fclaw2d_global_t *glob) 
{
    // get needed options
    fclaw2d_clawpatch_options_t *clawpatch_opt =
    fclaw2d_clawpatch_get_options(glob);
    fclaw_options_t *fclaw_opt = fclaw2d_get_options(glob);
    fc2d_thunderegg_options_t *mg_opt = fc2d_thunderegg_get_options(glob);
  
#if 0  
    fc2d_thunderegg_vtable_t *mg_vt = fc2d_thunderegg_vt();
#endif  

    // create thunderegg vector for eqn 0
    shared_ptr<Vector<2>> f = make_shared<fc2d_thunderegg_vector>(glob);

    // get patch size
    array<int, 2> ns = {clawpatch_opt->mx, clawpatch_opt->my};

    // get p4est structure
    fclaw2d_domain_t *domain = glob->domain;
    p4est_wrap_t *wrap = (p4est_wrap_t *)domain->pp;

    // create map function
    P4estDomainGenerator::BlockMapFunc bmf = [&](int block_no, double unit_x,      
                                        double unit_y, double &x, double &y) 
    {
        double x1,y1,z1;
        FCLAW2D_MAP_BRICK2C(&glob->cont,&block_no,&unit_x, &unit_y, &x1, &y1, &z1);
        x = fclaw_opt->ax + (fclaw_opt->bx - fclaw_opt->ax) * x1;
        y = fclaw_opt->ay + (fclaw_opt->by - fclaw_opt->ay) * y1;
    };

    // create neumann function
    IsNeumannFunc<2> inf = [&](Side<2> s, const array<double, 2> &lower,
                               const array<double, 2> &upper) 
    {
        return mg_opt->boundary_conditions[s.getIndex()] == 2;
    };

    // generates levels of patches for GMG
    P4estDomainGenerator domain_gen(wrap->p4est, ns, 1, inf, bmf);

    // get finest level
    shared_ptr<Domain<2>> te_domain = domain_gen.getFinestDomain();

    // define operators for problems

#if 0
    // get beta function
    auto beta_func = [&](const std::array<double,2>& coord){
        double beta;
        double grad[2];
        mg_vt->fort_beta(&coord[0],&coord[1],&beta,grad);
        return beta;
    };
#endif  

    // create vector for beta
    auto beta_vec = ValVector<2>::GetNewVector(te_domain, clawpatch_opt->meqn);
    DomainTools::SetValuesWithGhost<2>(te_domain, beta_vec, beta_coeff);

    // ghost filler
    auto ghost_filler = make_shared<BiLinearGhostFiller>(te_domain);

    // patch operator
#if USE_FIVEPOINT == 1
    auto op = make_shared<fivePoint>(te_domain,ghost_filler);
#else
    auto op = make_shared<StarPatchOperator<2>>(beta_vec,te_domain,ghost_filler);
#endif    

    // set the patch solver
    shared_ptr<PatchSolver<2>>  solver;
    if(strcmp(mg_opt->patch_solver_type , "BCGS") == 0){
        solver = make_shared<BiCGStabPatchSolver<2>>(op,
                                                     mg_opt->patch_bcgs_tol,
                                                     mg_opt->patch_bcgs_max_it);
    }else if(strcmp(mg_opt->patch_solver_type , "FFT") == 0){
        solver = make_shared<Poisson::FFTWPatchSolver<2>>(op);
    }

    // create matrix
    shared_ptr<Operator<2>> A = op;

    // create gmg preconditioner
    shared_ptr<Operator<2>> M;

    if(mg_opt->mg_prec && domain_gen.hasCoarserDomain())
    {
        // options
        GMG::CycleOpts copts;
        copts.max_levels = mg_opt->max_levels;
        copts.patches_per_proc = mg_opt->patches_per_proc;
        copts.pre_sweeps = mg_opt->pre_sweeps;
        copts.post_sweeps = mg_opt->post_sweeps;
        copts.mid_sweeps = mg_opt->mid_sweeps;
        copts.coarse_sweeps = mg_opt->coarse_sweeps;
        copts.cycle_type = mg_opt->cycle_type;

        //GMG cycle builder
        GMG::CycleBuilder<2> builder(copts);
        
        //add finest level

        //next domain
        auto curr_domain = te_domain;
        auto next_domain = domain_gen.getCoarserDomain();

        //operator
        auto patch_operator = op;

        //smoother
        shared_ptr<GMG::Smoother<2>> smoother = solver;

        //restrictor
        auto restrictor = make_shared<GMG::LinearRestrictor<2>>(curr_domain, next_domain, clawpatch_opt->meqn);

        //vector generator
        auto vg = make_shared<ValVectorGenerator<2>>(curr_domain, clawpatch_opt->meqn);

        builder.addFinestLevel(patch_operator, smoother, restrictor, vg);

        //add intermediate levels
#if USE_FIVEPOINT == 0
        auto prev_beta_vec = beta_vec;
#endif    
        auto prev_domain = curr_domain;
        curr_domain = next_domain;
        while(domain_gen.hasCoarserDomain())
        {
            next_domain = domain_gen.getCoarserDomain();

            //operator
            auto ghost_filler = make_shared<BiLinearGhostFiller>(curr_domain);
#if USE_FIVEPOINT == 1
            patch_operator = make_shared<fivePoint>(curr_domain, ghost_filler);
#else
            auto restricted_beta_vec = restrict_beta_vec(prev_beta_vec, prev_domain, curr_domain);
            patch_operator = make_shared<StarPatchOperator<2>>(restricted_beta_vec, curr_domain, ghost_filler);
            prev_beta_vec = restricted_beta_vec;
#endif    
            //smoother
            shared_ptr<GMG::Smoother<2>> smoother;
            if(strcmp(mg_opt->patch_solver_type , "BCGS") == 0){
                smoother = make_shared<BiCGStabPatchSolver<2>>(patch_operator,
                                                               mg_opt->patch_bcgs_tol,
                                                               mg_opt->patch_bcgs_max_it);
            }else if(strcmp(mg_opt->patch_solver_type , "FFT") == 0){
                smoother = make_shared<Poisson::FFTWPatchSolver<2>>(patch_operator);
            }

            //restrictor
            auto restrictor = make_shared<GMG::LinearRestrictor<2>>(curr_domain, next_domain, clawpatch_opt->meqn);

            //interpolator
            auto interpolator = make_shared<GMG::DirectInterpolator<2>>(curr_domain, prev_domain, clawpatch_opt->meqn);

            //vector generator
            vg = make_shared<ValVectorGenerator<2>>(curr_domain, clawpatch_opt->meqn);

            builder.addIntermediateLevel(patch_operator, smoother, restrictor, interpolator, vg);

            prev_domain = curr_domain;
            curr_domain = next_domain;
        }

        //add coarsest level

        //operator
        auto ghost_filler = make_shared<BiLinearGhostFiller>(curr_domain);
#if USE_FIVEPOINT == 1
        patch_operator = make_shared<fivePoint>(curr_domain, ghost_filler);
#else
        auto restricted_beta_vec = restrict_beta_vec(prev_beta_vec, prev_domain, curr_domain);
        patch_operator = make_shared<StarPatchOperator<2>>(restricted_beta_vec, curr_domain, ghost_filler);
#endif    
        //smoother
        if(strcmp(mg_opt->patch_solver_type , "BCGS") == 0){
            smoother = make_shared<BiCGStabPatchSolver<2>>(patch_operator,
                                                           mg_opt->patch_bcgs_tol,
                                                           mg_opt->patch_bcgs_max_it);
        }else if(strcmp(mg_opt->patch_solver_type , "FFT") == 0){
            smoother = make_shared<Poisson::FFTWPatchSolver<2>>(patch_operator);
        }

        //interpolator
        auto interpolator = make_shared<GMG::DirectInterpolator<2>>(curr_domain, prev_domain, clawpatch_opt->meqn);

        //vector generator
        vg = make_shared<ValVectorGenerator<2>>(curr_domain, clawpatch_opt->meqn);

        builder.addCoarsestLevel(patch_operator, smoother, interpolator, vg);

        M = builder.getCycle();
    }

    // solve
    auto vg = make_shared<ValVectorGenerator<2>>(te_domain, clawpatch_opt->meqn);
    shared_ptr<Vector<2>> u = vg->getNewVector();

    int its = BiCGStab<2>::solve(vg, A, u, f, M, mg_opt->max_it, mg_opt->tol);

    fclaw_global_productionf("Iterations: %i\n", its);
    fclaw_global_productionf("f-2norm: %f\n", f->twoNorm());
    fclaw_global_productionf("f-infnorm: %f\n", f->infNorm());
    fclaw_global_productionf("u-2norm: %f\n", u->twoNorm());
    fclaw_global_productionf("u-infnorm: %f\n\n", u->infNorm());

    // copy solution into rhs
    f->copy(u);
    fclaw_global_productionf("Checking if copy function works:\n");
    fclaw_global_productionf("fcopy-2norm: %f\n", f->twoNorm());
    fclaw_global_productionf("fcopy-infnorm: %f\n\n", f->infNorm());
}

