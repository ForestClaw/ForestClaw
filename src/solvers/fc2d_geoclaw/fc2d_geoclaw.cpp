/*
Copyright (c) 2012 Carsten Burstedde, Donna Calhoun
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

#include <fclaw2d_global.h>

#include <fclaw2d_forestclaw.h>
#include <fclaw2d_clawpatch.hpp>

#include "fc2d_geoclaw.h"
#include "fc2d_geoclaw_options.h"

static int s_geoclaw_package_id = -1;

static fc2d_geoclaw_vtable_t geoclaw_vt;



void fc2d_geoclaw_set_vtable(const fc2d_geoclaw_vtable_t* user_vt)
{
    geoclaw_vt = *user_vt;
}

void fc2d_geoclaw_init_vtable(fc2d_geoclaw_vtable_t* vt)
{
    /* Required functions  - error if NULL*/
    vt->bc2 = &GEOCLAW_BC2;
    vt->qinit = &QINIT;
    vt->rpn2 = NULL;
    vt->rpt2 = NULL;

    /* Optional functions - call only if non-NULL */
    vt->setprob = NULL;
    vt->setaux = &SETAUX;
    vt->b4step2 = &B4STEP2;
    vt->src2 = &SRC2;
}





/* Patch data is stored in each ClawPatch */
struct patch_aux_data
{
    FArrayBox auxarray;
    int maux;
};

fc2d_geoclaw_options_t* fc2d_geoclaw_get_options(fclaw2d_domain_t *domain)
{
    int id;
    fclaw_app_t* app;
    app = fclaw2d_domain_get_app(domain);
    id = fc2d_geoclaw_get_package_id();
    return (fc2d_geoclaw_options_t*) fclaw_package_get_options(app,id);
}

static patch_aux_data*
get_patch_data(ClawPatch *cp)
{
    patch_aux_data *wp =
        (patch_aux_data*) cp->clawpack_patch_data(s_geoclaw_package_id);
    return wp;
}

static void*
patch_data_new()
{
    patch_aux_data* data;
    data = new patch_aux_data;
    return (void*) data;
}

static void
patch_data_delete(void *data)
{
    patch_aux_data *pd = (patch_aux_data*) data;
    FCLAW_ASSERT(pd != NULL);
    delete pd;
}

static const fclaw_package_vtable_t geoclaw_patch_vtable = {
    patch_data_new,
    patch_data_delete
};


/* -----------------------------------------------------------
   Public interface to routines in this file
   ----------------------------------------------------------- */
void
fc2d_geoclaw_register_vtable (fclaw_package_container_t * pkg_container,
                                 fc2d_geoclaw_options_t * clawopt)
{
    FCLAW_ASSERT(s_geoclaw_package_id == -1);

    s_geoclaw_package_id =
      fclaw_package_container_add (pkg_container, clawopt,
                                   &geoclaw_patch_vtable);
}

void fc2d_geoclaw_register (fclaw_app_t* app, const char *configfile)
{
    fc2d_geoclaw_options_t* clawopt;
    int id;

    /* Register the options */
    clawopt = fc2d_geoclaw_options_register(app,configfile);

    /* And the package */

    /* Don't register a package more than once */
    FCLAW_ASSERT(s_geoclaw_package_id == -1);

    id = fclaw_package_container_add_pkg(app,clawopt,
                                         &geoclaw_patch_vtable);

    s_geoclaw_package_id = id;
}

int fc2d_geoclaw_get_package_id (void)
{
    return s_geoclaw_package_id;
}

static void
fc2d_geoclaw_define_auxarray_cp (fclaw2d_domain_t* domain, ClawPatch *cp)
{
    int maux;
    fc2d_geoclaw_options_t *clawpack_options;
    const amr_options_t* gparms;
    int mx,my,mbc;
    int ll[2], ur[2];

    gparms = get_domain_parms(domain);
    mx = gparms->mx;
    my = gparms->my;
    mbc = gparms->mbc;

    clawpack_options = fc2d_geoclaw_get_options(domain);
    maux = clawpack_options->maux;

    /* Construct an index box to hold the aux array */
    ll[0] = 1-mbc;
    ll[1] = 1-mbc;
    ur[0] = mx + mbc;
    ur[1] = my + mbc;    Box box(ll,ur);

    /* get solver specific data stored on this patch */
    patch_aux_data *clawpack_patch_data = get_patch_data(cp);
    clawpack_patch_data->auxarray.define(box,maux);
    clawpack_patch_data->maux = maux; // set maux in solver specific patch data (for completeness)
}

void fc2d_geoclaw_define_auxarray (fclaw2d_domain_t* domain,
                                      fclaw2d_patch_t* this_patch)
{
    ClawPatch *cp = fclaw2d_clawpatch_get_cp (this_patch);
    fc2d_geoclaw_define_auxarray_cp (domain, cp);
}

static void
fc2d_geoclaw_get_auxarray_cp (fclaw2d_domain_t* domain,
                                 ClawPatch *cp, double **aux, int* maux)
{
    fc2d_geoclaw_options_t* clawpack_options;
    clawpack_options = fc2d_geoclaw_get_options(domain);
    *maux = clawpack_options->maux;

    patch_aux_data *clawpack_patch_data = get_patch_data(cp);
    *aux = clawpack_patch_data->auxarray.dataPtr();
}

void fc2d_geoclaw_aux_data(fclaw2d_domain_t* domain,
                              fclaw2d_patch_t *this_patch,
                              double **aux, int* maux)
{
    ClawPatch *cp = fclaw2d_clawpatch_get_cp (this_patch);
    fc2d_geoclaw_get_auxarray_cp (domain,cp,aux,maux);
}

void fc2d_geoclaw_maux(fclaw2d_domain_t* domain,int* maux)
{
    *maux = fc2d_geoclaw_get_maux(domain);
}

int fc2d_geoclaw_get_maux(fclaw2d_domain_t* domain)
{
    fc2d_geoclaw_options_t *clawpack_options;
    clawpack_options = fc2d_geoclaw_get_options(domain);
    return clawpack_options->maux;
}

void fc2d_geoclaw_setup(fclaw2d_domain_t *domain)
{
    fc2d_geoclaw_options_t *geoclaw_options;
    const amr_options_t* gparms = get_domain_parms(domain);
    geoclaw_options = fc2d_geoclaw_get_options(domain);
    GEOCLAW_SET_MODULES(&geoclaw_options->mwaves, &geoclaw_options->mcapa,
                   geoclaw_options->mthlim, geoclaw_options->method,
                   &gparms->ax, &gparms->bx, &gparms->ay, &gparms->by);
}


void fc2d_geoclaw_setprob(fclaw2d_domain_t *domain)
{
    if (geoclaw_vt.setprob != NULL)
    {
        geoclaw_vt.setprob();
    }
}


void fc2d_geoclaw_patch_setup(fclaw2d_domain_t *domain,
                              fclaw2d_patch_t *this_patch,
                              int this_block_idx,
                              int this_patch_idx)
{
    /* Dummy setup - use multiple libraries */
    fc2d_geoclaw_setaux(domain,this_patch,this_block_idx,this_patch_idx);
}

/* This should only be called when a new ClawPatch is created. */
void fc2d_geoclaw_setaux(fclaw2d_domain_t *domain,
                            fclaw2d_patch_t *this_patch,
                            int this_block_idx,
                            int this_patch_idx)
{
    FCLAW_ASSERT(geoclaw_vt.setaux != NULL);
    int mx,my,mbc,maux;
    double xlower,ylower,dx,dy;
    double *aux;

    fclaw2d_clawpatch_grid_data(domain,this_patch, &mx,&my,&mbc,
                                &xlower,&ylower,&dx,&dy);

    fc2d_geoclaw_define_auxarray (domain,this_patch);

    fc2d_geoclaw_aux_data(domain,this_patch,&aux,&maux);

    GEOCLAW_SET_BLOCK(&this_block_idx);
    geoclaw_vt.setaux(&mbc,&mx,&my,&xlower,&ylower,&dx,&dy,
                      &maux,aux);
    GEOCLAW_UNSET_BLOCK();
}

/* This should only be called when a new ClawPatch is created. */
void fc2d_geoclaw_set_capacity(fclaw2d_domain_t *domain,
                                  fclaw2d_patch_t *this_patch,
                                  int this_block_idx,
                                  int this_patch_idx)
{
#if 0
    int mx,my,mbc,maux,mcapa;
    double dx,dy,xlower,ylower;
    double *aux, *area;
    fc2d_geoclaw_options_t *clawopt;

    clawopt = fc2d_geoclaw_get_options(domain);
    mcapa = clawopt->mcapa;

    fclaw2d_clawpatch_grid_data(domain,this_patch, &mx,&my,&mbc,
                                &xlower,&ylower,&dx,&dy);

    area = fclaw2d_clawpatch_get_area(domain,this_patch);

    fc2d_geoclaw_aux_data(domain,this_patch,&aux,&maux);
    FCLAW_ASSERT(maux >= mcapa && mcapa > 0);

    GEOCLAW_SET_CAPACITY(&mx,&my,&mbc,&dx,&dy,area,&mcapa,
                            &maux,aux);
#endif
}


void fc2d_geoclaw_qinit(fclaw2d_domain_t *domain,
                           fclaw2d_patch_t *this_patch,
                           int this_block_idx,
                           int this_patch_idx)
{
    FCLAW_ASSERT(geoclaw_vt.qinit != NULL); /* Must initialized */
    int mx,my,mbc,meqn,maux;
    double dx,dy,xlower,ylower;
    double *q, *aux;

    fclaw2d_clawpatch_grid_data(domain,this_patch,&mx,&my,&mbc,
                                &xlower,&ylower,&dx,&dy);

    fclaw2d_clawpatch_soln_data(domain,this_patch,&q,&meqn);
    fc2d_geoclaw_aux_data(domain,this_patch,&aux,&maux);

    /* Call to classic Clawpack 'qinit' routine.  This must be user defined */
    GEOCLAW_SET_BLOCK(&this_block_idx);
    geoclaw_vt.qinit(&meqn,&mbc,&mx,&my,&xlower,&ylower,&dx,&dy,q,
                     &maux,aux);
    GEOCLAW_UNSET_BLOCK();
}

void fc2d_geoclaw_b4step2(fclaw2d_domain_t *domain,
                             fclaw2d_patch_t *this_patch,
                             int this_block_idx,
                             int this_patch_idx,
                             double t, double dt)

{
    FCLAW_ASSERT(geoclaw_vt.b4step2 != NULL);

    int mx,my,mbc,meqn, maux;
    double xlower,ylower,dx,dy;
    double *aux,*q;

    fclaw2d_clawpatch_grid_data(domain,this_patch, &mx,&my,&mbc,
                                &xlower,&ylower,&dx,&dy);

    fclaw2d_clawpatch_soln_data(domain,this_patch,&q,&meqn);
    fc2d_geoclaw_aux_data(domain,this_patch,&aux,&maux);

    GEOCLAW_SET_BLOCK(&this_block_idx);
    geoclaw_vt.b4step2(&mbc,&mx,&my,&meqn,q,&xlower,&ylower,
                       &dx,&dy,&t,&dt,&maux,aux);
    GEOCLAW_UNSET_BLOCK();
}

void fc2d_geoclaw_src2(fclaw2d_domain_t *domain,
                          fclaw2d_patch_t *this_patch,
                          int this_block_idx,
                          int this_patch_idx,
                          double t,
                          double dt)
{
    FCLAW_ASSERT(geoclaw_vt.src2 != NULL);

    int mx,my,mbc,meqn, maux;
    double xlower,ylower,dx,dy;
    double *aux,*q;

    fclaw2d_clawpatch_grid_data(domain,this_patch, &mx,&my,&mbc,
                                &xlower,&ylower,&dx,&dy);

    fclaw2d_clawpatch_soln_data(domain,this_patch,&q,&meqn);
    fc2d_geoclaw_aux_data(domain,this_patch,&aux,&maux);

    GEOCLAW_SET_BLOCK(&this_block_idx);
    geoclaw_vt.src2(&meqn,&mbc,&mx,&my,&xlower,&ylower,
                    &dx,&dy,q,&maux,aux,&t,&dt);
    GEOCLAW_UNSET_BLOCK();
}



/* Use this to return only the right hand side of the clawpack algorithm */
double fc2d_geoclaw_step2_rhs(fclaw2d_domain_t *domain,
                                 fclaw2d_patch_t *this_patch,
                                 int this_block_idx,
                                 int this_patch_idx,
                                 double t,
                                 double *rhs)
{
    /* This should evaluate the right hand side, but not actually do the update.
       This will be useful in cases where we want to use something other than
       a single step method.  For example, in a RK scheme, one might want to
       call the right hand side to evaluate stages. */
    return 0;
}


void fc2d_geoclaw_bc2(fclaw2d_domain *domain,
                         fclaw2d_patch_t *this_patch,
                         int this_block_idx,
                         int this_patch_idx,
                         double t,
                         double dt,
                         fclaw_bool intersects_phys_bdry[],
                         fclaw_bool time_interp)
{
    FCLAW_ASSERT(geoclaw_vt.bc2 != NULL);

    int mx,my,mbc,meqn, maux;
    double xlower,ylower,dx,dy;
    double *aux,*q;

    fclaw2d_clawpatch_grid_data(domain,this_patch, &mx,&my,&mbc,
                                &xlower,&ylower,&dx,&dy);

    fc2d_geoclaw_aux_data(domain,this_patch,&aux,&maux);

    fclaw2d_block_t *this_block = &domain->blocks[this_block_idx];
    fclaw2d_block_data_t *bdata = fclaw2d_block_get_data(this_block);
    int *block_mthbc = bdata->mthbc;

    /* Set a local copy of mthbc that can be used for a patch. */
    int mthbc[NumFaces];
    for(int i = 0; i < NumFaces; i++)
    {
        if (intersects_phys_bdry[i])
        {
            mthbc[i] = block_mthbc[i];
        }
        else
        {
            mthbc[i] = -1;
        }
    }

    /*
      We may be imposing boundary conditions on time-interpolated data;
      and is being done just to help with fine grid interpolation.
      In this case, this boundary condition won't be used to update
      anything
    */
#if 0
    fclaw2d_clawpatch_soln_data(domain,this_patch,&q,&meqn);
    q = cp->q_time_sync(time_interp);
#endif
    fclaw2d_clawpatch_timesync_data(domain,this_patch,time_interp,&q,&meqn);

    GEOCLAW_SET_BLOCK(&this_block_idx);
    /*
    geoclaw_vt.bc2(q, aux, nrow, ncol ,meqn, maux, hx, hy,
                   level, time, xleft, xright, ybot, ytop,
                   &xlower, &ylower, xupper, yupper, 0,0,0);
    */
    geoclaw_vt.bc2(&meqn,&mbc,&mx,&my,&xlower,&ylower,
                   &dx,&dy,q,&maux,aux,&t,&dt,mthbc);
    GEOCLAW_UNSET_BLOCK();
}


/* This is called from the single_step callback. and is of type 'flaw_single_step_t' */
double fc2d_geoclaw_step2(fclaw2d_domain_t *domain,
                             fclaw2d_patch_t *this_patch,
                             int this_block_idx,
                             int this_patch_idx,
                             double t,
                             double dt)
{
#if 0
    fc2d_geoclaw_options_t* clawpack_options;
    ClawPatch *cp;
    int level;
    double *qold, *aux;
    int mx, my, meqn, maux, mbc;
    double xlower, ylower, dx,dy;

    FCLAW_ASSERT(geoclaw_vt.rpn2 != NULL);
    FCLAW_ASSERT(geoclaw_vt.rpt2 != NULL);

    clawpack_options = fc2d_geoclaw_get_options(domain);

    cp = fclaw2d_clawpatch_get_cp(this_patch);

    level = this_patch->level;

    fc2d_geoclaw_aux_data(domain,this_patch,&aux,&maux);

    cp->save_current_step();  // Save for time interpolation

    fclaw2d_clawpatch_grid_data(domain,this_patch,&mx,&my,&mbc,
                                &xlower,&ylower,&dx,&dy);

    fclaw2d_clawpatch_soln_data(domain,this_patch,&qold,&meqn);

    int mwaves = clawpack_options->mwaves;

    int maxm = fmax(mx,my);

    double cflgrid = 0.0;

    int mwork = (maxm+2*mbc)*(12*meqn + (meqn+1)*mwaves + 3*maux + 2);
    double* work = new double[mwork];

    int size = meqn*(mx+2*mbc)*(my+2*mbc);
    double* fp = new double[size];
    double* fm = new double[size];
    double* gp = new double[size];
    double* gm = new double[size];


    int ierror = 0;
    //fc2d_geoclaw_flux2_t flux2 = clawpack_options->use_fwaves ?
    //                                GEOCLAW_FLUX2FW : GEOCLAW_FLUX2;
    fc2d_geoclaw_flux2_t flux2 = GEOCLAW_FLUX2;
    GEOCLAW_STEP2_WRAP(&maxm, &meqn, &maux, &mbc, clawpack_options->method,
                          clawpack_options->mthlim, &clawpack_options->mcapa,
                          &mwaves,&mx, &my, qold, aux, &dx, &dy, &dt, &cflgrid,
                          work, &mwork, &xlower, &ylower, &level,&t, fp, fm, gp, gm,
                          geoclaw_vt.rpn2, geoclaw_vt.rpt2,flux2,
                          cp->block_corner_count(), &ierror);

    FCLAW_ASSERT(ierror == 0);

    //GEOCLAW_STEP2(&maxm, &meqn, &maux, &mbc, &mx, &my, qold, aux, &dx, &dy, &dt, &cflgrid,
    //                fm, fp, gm, gp, geoclaw_vt.rpn2, geoclaw_vt.rpt2);
    /* Accumulate fluxes needed for conservative fix-up */
    if (geoclaw_vt.fluxfun != NULL)
    {
        /* Accumulate fluxes */
    }


    delete [] fp;
    delete [] fm;
    delete [] gp;
    delete [] gm;

    delete [] work;

    return cflgrid;
#endif
    return 0.9;  /* Change this! */
}

double fc2d_geoclaw_update(fclaw2d_domain_t *domain,
                              fclaw2d_patch_t *this_patch,
                              int this_block_idx,
                              int this_patch_idx,
                              double t,
                              double dt)
{
#if 0
    const fc2d_geoclaw_options_t* clawpack_options;
    clawpack_options = fc2d_geoclaw_get_options(domain);
    if (geoclaw_vt.b4step2 != NULL)
    {
        fc2d_geoclaw_b4step2(domain,
                                this_patch,
                                this_block_idx,
                                this_patch_idx,t,dt);
    }
    double maxcfl = fc2d_geoclaw_step2(domain,
                                          this_patch,
                                          this_block_idx,
                                          this_patch_idx,t,dt);
    if (clawpack_options->src_term > 0)
    {
        fc2d_geoclaw_src2(domain,
                             this_patch,
                             this_block_idx,
                             this_patch_idx,t,dt);
    }
    return maxcfl;
#endif
    return 0.9;
}
