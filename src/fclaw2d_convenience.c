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

#include <fclaw2d_convenience.h>
#include <p4est_bits.h>
#include <p4est_wrap.h>

const double fclaw2d_smallest_h = 1. / (double) P4EST_ROOT_LEN;

static void
fclaw2d_patch_set_boundary_xylower (fclaw2d_patch_t * patch,
                                    p4est_quadrant_t * quad)
{
    p4est_qcoord_t qh;

    qh = P4EST_QUADRANT_LEN (quad->level);
    if (quad->x == 0)
    {
        patch->flags |= FCLAW2D_PATCH_ON_BLOCK_FACE_0;
    }
    if (quad->x + qh == P4EST_ROOT_LEN)
    {
        patch->flags |= FCLAW2D_PATCH_ON_BLOCK_FACE_1;
    }
    if (quad->y == 0)
    {
        patch->flags |= FCLAW2D_PATCH_ON_BLOCK_FACE_2;
    }
    if (quad->y + qh == P4EST_ROOT_LEN)
    {
        patch->flags |= FCLAW2D_PATCH_ON_BLOCK_FACE_3;
    }
    /* This suffices to test for block corners by using bitwise and */

    patch->xlower = quad->x * fclaw2d_smallest_h;
    patch->xupper = (quad->x + qh) * fclaw2d_smallest_h;
    patch->ylower = quad->y * fclaw2d_smallest_h;
    patch->yupper = (quad->y + qh) * fclaw2d_smallest_h;
}

/** Domain constructor takes ownership of wrap.
 */
static fclaw2d_domain_t *
fclaw2d_domain_new (p4est_wrap_t * wrap, sc_keyvalue_t * attributes)
{
    int mpiret;
    int i, j;
    int level;
    int face;
    int nb, nm, mirror_quadrant_num;
    int block_nm_pre;
    int local_num_patches;
    int tree_minlevel, local_minlevel;
    int tree_maxlevel, local_maxlevel;
    int levels[2], global_levels[2];
    p4est_topidx_t vnum;
    p4est_connectivity_t *conn = wrap->conn;
    p4est_ghost_t *ghost = wrap->match_aux ? wrap->ghost_aux : wrap->ghost;
#ifdef FCLAW_ENABLE_DEBUG
    p4est_mesh_t *mesh = wrap->match_aux ? wrap->mesh_aux : wrap->mesh;
#endif
    p4est_tree_t *tree;
    p4est_quadrant_t *quad, *mirror;
    fclaw2d_domain_t *domain;
    fclaw2d_block_t *block;
    fclaw2d_patch_t *patch;
    fclaw2d_patch_t *currentbylevel[P4EST_MAXLEVEL + 1];

#ifdef FCLAW_ENABLE_DEBUG
    memset (currentbylevel, 0,
            sizeof (fclaw2d_patch_t *) * (P4EST_MAXLEVEL + 1));
#endif
    domain = FCLAW_ALLOC_ZERO (fclaw2d_domain_t, 1);
    domain->mpicomm = wrap->p4est->mpicomm;
    domain->mpisize = wrap->p4est->mpisize;
    domain->mpirank = wrap->p4est->mpirank;
    domain->pp = (void *) wrap;
    domain->pp_owned = 1;
    domain->attributes = attributes != NULL ? attributes : sc_keyvalue_new ();
    nm = 0;
    domain->num_exchange_patches = (int) ghost->mirrors.elem_count;
    if (domain->num_exchange_patches > 0)
    {
        domain->exchange_patches = FCLAW_ALLOC (fclaw2d_patch_t *,
                                                domain->num_exchange_patches);
        mirror = p4est_quadrant_array_index (&ghost->mirrors, nm);
        mirror_quadrant_num = (int) mirror->p.piggy3.local_num;
        FCLAW_ASSERT (mirror_quadrant_num >= 0);
        FCLAW_ASSERT (mirror_quadrant_num <
                      (int) wrap->p4est->local_num_quadrants);
    }
    else
    {
        mirror = NULL;
        mirror_quadrant_num = -1;
    }
    domain->num_ghost_patches = (int) ghost->ghosts.elem_count;
    FCLAW_ASSERT (domain->num_ghost_patches ==
                  (int) mesh->ghost_num_quadrants);
    domain->num_blocks = nb = (int) conn->num_trees;
    domain->blocks = FCLAW_ALLOC_ZERO (fclaw2d_block_t, domain->num_blocks);
    domain->possible_maxlevel = P4EST_QMAXLEVEL;
    local_num_patches = 0;
    local_minlevel = domain->possible_maxlevel;
    local_maxlevel = -1;

    /* prepare propagation of refinement/coarsening marks */
    domain->smooth_refine = 0;
    domain->mirror_target_levels =
        FCLAW_ALLOC (void *, domain->num_exchange_patches);
    domain->ghost_target_levels =
        FCLAW_ALLOC (int, domain->num_ghost_patches);

    /* loop through all local patches, block by block */
    for (i = 0; i < nb; ++i)
    {
        block = domain->blocks + i;
        block->num_patches_before = local_num_patches;
        tree =
            p4est_tree_array_index (wrap->p4est->trees, (p4est_topidx_t) i);
        tree_minlevel = domain->possible_maxlevel;
        tree_maxlevel = -1;
        block->xlower = 0.;
        block->xupper = 1.;
        block->ylower = 0.;
        block->yupper = 1.;
        if (conn->vertices != NULL && conn->tree_to_vertex != NULL)
        {
            for (j = 0; j < P4EST_CHILDREN; ++j)
            {
                vnum = conn->tree_to_vertex[P4EST_CHILDREN * i + j];
                FCLAW_ASSERT (0 <= vnum && vnum < conn->num_vertices);
                memcpy (block->vertices + 3 * j, conn->vertices + 3 * vnum,
                        3 * sizeof (double));
            }
        }
        else
        {
            memset (block->vertices, 0, P4EST_CHILDREN * 3 * sizeof (double));
        }
        for (face = 0; face < P4EST_FACES; ++face)
        {
            if (conn->tree_to_tree[P4EST_FACES * i + face] ==
                (p4est_topidx_t) i
                && conn->tree_to_face[P4EST_FACES * i + face] ==
                (int8_t) face)
            {
                block->is_boundary[face] = 1;
            }
        }
        block->num_patches = (int) tree->quadrants.elem_count;
        block->patches =
            FCLAW_ALLOC_ZERO (fclaw2d_patch_t, block->num_patches);
        block->patchbylevel =
            FCLAW_ALLOC_ZERO (fclaw2d_patch_t *,
                              domain->possible_maxlevel + 1);

        block_nm_pre = nm;
        for (j = 0; j < block->num_patches; ++j)
        {
            patch = block->patches + j;
            quad = p4est_quadrant_array_index (&tree->quadrants, (size_t) j);
            patch->target_level = patch->level = level = (int) quad->level;
            patch->flags = p4est_quadrant_child_id (quad);
            if (j + P4EST_CHILDREN <= block->num_patches &&
                p4est_quadrant_is_familyv (quad))
            {
                patch->flags |= FCLAW2D_PATCH_FIRST_SIBLING;
            }
            FCLAW_ASSERT (0 <= level && level <= domain->possible_maxlevel);
            if (mirror_quadrant_num == local_num_patches)
            {
                patch->flags |= FCLAW2D_PATCH_ON_PARALLEL_BOUNDARY;
                domain->exchange_patches[nm] = patch;
                domain->mirror_target_levels[nm] = &patch->target_level;
                if (++nm < domain->num_exchange_patches)
                {
                    mirror = p4est_quadrant_array_index (&ghost->mirrors, nm);
                    mirror_quadrant_num = (int) mirror->p.piggy3.local_num;
                    FCLAW_ASSERT (mirror_quadrant_num > local_num_patches);
                    FCLAW_ASSERT (mirror_quadrant_num <
                                  (int) wrap->p4est->local_num_quadrants);
                }
                else
                {
                    mirror = NULL;
                    mirror_quadrant_num = -1;
                }
            }
            fclaw2d_patch_set_boundary_xylower (patch, quad);
            if (block->patchbylevel[level] == NULL)
            {
                /* this is the first patch of this level in this block */
                block->patchbylevel[level] = currentbylevel[level] = patch;
            }
            else
            {
                /* next pointer of previous patch by level in this block */
                FCLAW_ASSERT (currentbylevel[level] != NULL);
                currentbylevel[level]->u.next = patch;
                currentbylevel[level] = patch;
            }
            FCLAW_ASSERT (i == (int) mesh->quad_to_tree[local_num_patches]);
            tree_minlevel = SC_MIN (tree_minlevel, level);
            tree_maxlevel = SC_MAX (tree_maxlevel, level);
            local_num_patches++;
        }
        FCLAW_ASSERT (block->num_patches == 0 ||
                      tree_maxlevel == (int) tree->maxlevel);
        local_minlevel = SC_MIN (local_minlevel, tree_minlevel);
        local_maxlevel = SC_MAX (local_maxlevel, tree_maxlevel);
        block->minlevel = tree_minlevel;
        block->maxlevel = tree_maxlevel;
        block->num_exchange_patches = nm - block_nm_pre;
        if (block->num_exchange_patches > 0)
        {
            block->exchange_patches = domain->exchange_patches + block_nm_pre;
        }
    }
    FCLAW_ASSERT (local_num_patches ==
                  (int) wrap->p4est->local_num_quadrants);
    FCLAW_ASSERT (nm == domain->num_exchange_patches);
    domain->local_num_patches = local_num_patches;
    domain->local_minlevel = local_minlevel;
    domain->local_maxlevel = local_maxlevel;

    /* allocate ghost patches */
    domain->ghost_patches =
        FCLAW_ALLOC_ZERO (fclaw2d_patch_t, domain->num_ghost_patches);
    for (i = 0; i < domain->num_ghost_patches; ++i)
    {
        patch = domain->ghost_patches + i;
        quad = p4est_quadrant_array_index (&ghost->ghosts, (size_t) i);
        patch->target_level = patch->level = level = (int) quad->level;
        patch->flags =
            p4est_quadrant_child_id (quad) | FCLAW2D_PATCH_IS_GHOST;
        FCLAW_ASSERT (0 <= level && level <= domain->possible_maxlevel);
        fclaw2d_patch_set_boundary_xylower (patch, quad);
        patch->u.blockno = (int) quad->p.which_tree;
    }

    /* parallel communication of minimum and maximum levels */
    levels[0] = domain->local_minlevel;
    levels[1] = -domain->local_maxlevel;
    mpiret =
        sc_MPI_Allreduce (levels, global_levels, 2, sc_MPI_INT, sc_MPI_MIN,
                          domain->mpicomm);
    SC_CHECK_MPI (mpiret);
    domain->global_minlevel = global_levels[0];
    domain->global_maxlevel = -global_levels[1];
    FCLAW_ASSERT (0 <= domain->global_minlevel);
    FCLAW_ASSERT (domain->global_minlevel <= domain->global_maxlevel);
    FCLAW_ASSERT (domain->global_maxlevel <= domain->possible_maxlevel);
    domain->global_num_patches = (int64_t) wrap->p4est->global_num_quadrants;
    domain->global_num_patches_before =
        (int64_t) wrap->p4est->global_first_quadrant[domain->mpirank];

    return domain;
}

static void
fclaw2d_check_initial_level (sc_MPI_Comm mpicomm, int initial_level)
{
    int mpiret;
    int rank;

    mpiret = sc_MPI_Comm_rank (mpicomm, &rank);
    SC_CHECK_MPI (mpiret);
    SC_CHECK_ABORTF (rank != 0 || initial_level <= P4EST_QMAXLEVEL,
                     "Initial level %d too fine for p4est", initial_level);
}

fclaw2d_domain_t *
fclaw2d_domain_new_unitsquare (sc_MPI_Comm mpicomm, int initial_level)
{
    fclaw2d_check_initial_level (mpicomm, initial_level);
    return fclaw2d_domain_new (p4est_wrap_new_unitsquare (mpicomm,
                                                          initial_level),
                               NULL);
}

fclaw2d_domain_t *
fclaw2d_domain_new_torus (sc_MPI_Comm mpicomm, int initial_level)
{
    fclaw2d_check_initial_level (mpicomm, initial_level);
    return
        fclaw2d_domain_new (p4est_wrap_new_periodic (mpicomm, initial_level),
                            NULL);
}

fclaw2d_domain_t *
fclaw2d_domain_new_twosphere (sc_MPI_Comm mpicomm, int initial_level)
{
    fclaw2d_check_initial_level (mpicomm, initial_level);
    return
        fclaw2d_domain_new (p4est_wrap_new_pillow (mpicomm, initial_level),
                            NULL);
}

fclaw2d_domain_t *
fclaw2d_domain_new_cubedsphere (sc_MPI_Comm mpicomm, int initial_level)
{
    fclaw2d_check_initial_level (mpicomm, initial_level);
    return fclaw2d_domain_new (p4est_wrap_new_cubed (mpicomm, initial_level),
                               NULL);
}

fclaw2d_domain_t *
fclaw2d_domain_new_disk (sc_MPI_Comm mpicomm, int initial_level)
{
    fclaw2d_check_initial_level (mpicomm, initial_level);
    return fclaw2d_domain_new (p4est_wrap_new_disk (mpicomm, initial_level),
                               NULL);
}

fclaw2d_domain_t *
fclaw2d_domain_new_brick_map (sc_MPI_Comm mpicomm,
                              int blocks_in_x, int blocks_in_y,
                              int periodic_in_x, int periodic_in_y,
                              int initial_level, fclaw2d_map_context_t * cont)
{
    p4est_wrap_t *wrap;
    fclaw2d_domain_t *domain;

    fclaw2d_check_initial_level (mpicomm, initial_level);
    wrap = p4est_wrap_new_brick (mpicomm, blocks_in_x, blocks_in_y,
                                 periodic_in_x, periodic_in_y, initial_level);
    domain = fclaw2d_domain_new (wrap, NULL);
    if (cont != NULL)
    {
        fclaw2d_domain_attribute_add (domain, "fclaw_map_context", cont);
    }

    return domain;
}

fclaw2d_domain_t *
fclaw2d_domain_new_conn_map (sc_MPI_Comm mpicomm, int initial_level,
                             p4est_connectivity_t * conn,
                             fclaw2d_map_context_t * cont)
{
    p4est_wrap_t *wrap;
    fclaw2d_domain_t *domain;

    fclaw2d_check_initial_level (mpicomm, initial_level);
    wrap = p4est_wrap_new_conn (mpicomm, conn, initial_level);
    domain = fclaw2d_domain_new (wrap, NULL);
    fclaw2d_domain_attribute_add (domain, "fclaw_map_context", cont);

    return domain;
}

void
fclaw2d_domain_destroy (fclaw2d_domain_t * domain)
{
    int i;
    fclaw2d_block_t *block;

    FCLAW_ASSERT (!domain->just_adapted);
    FCLAW_ASSERT (!domain->just_partitioned);

    for (i = 0; i < domain->num_blocks; ++i)
    {
        block = domain->blocks + i;
        FCLAW_FREE (block->patches);
        FCLAW_FREE (block->patchbylevel);
    }
    FCLAW_FREE (domain->blocks);

    FCLAW_FREE (domain->ghost_patches);
    FCLAW_FREE (domain->ghost_target_levels);
    FCLAW_FREE (domain->mirror_target_levels);
    FCLAW_FREE (domain->exchange_patches);

    if (domain->pp_owned)
    {
        p4est_wrap_t *wrap = (p4est_wrap_t *) domain->pp;
        p4est_wrap_destroy (wrap);
        sc_keyvalue_destroy (domain->attributes);
    }
    FCLAW_FREE (domain);
}

static void
fclaw2d_domain_copy_parameters (fclaw2d_domain_t * domain_dest,
                                fclaw2d_domain_t * domain_src)
{
    domain_dest->smooth_refine = domain_src->smooth_refine;
}

static fclaw2d_patch_t *
fclaw2d_domain_get_neighbor_patch (fclaw2d_domain_t * domain,
                                   int nproc, int nblockno, int npatchno)
{
    const int nb = (nproc == domain->mpirank ? nblockno : -1);

    return fclaw2d_domain_get_patch (domain, nb, npatchno);
}

fclaw2d_domain_t *
fclaw2d_domain_adapt (fclaw2d_domain_t * domain)
{
    p4est_wrap_t *wrap = (p4est_wrap_t *) domain->pp;

    FCLAW_ASSERT (domain->pp_owned);
    P4EST_ASSERT (!domain->just_adapted);
    P4EST_ASSERT (!domain->just_partitioned);

    /* propagate desired refinement level to neighbors */
    if (domain->smooth_refine)
    {
        int ng, nb, np;
        int face, corner;
        int nprocs[2], nblockno, npatchno[2], nfc;
        int level, max_tlevel;
        int exists;
        int k;
        fclaw2d_patch_relation_t nrel;
        fclaw2d_block_t *block;
        fclaw2d_patch_t *gpatch, *patch, *npatch;

        /* exchange target refinement level with parallel neighbors */
        p4est_ghost_exchange_custom (wrap->p4est, p4est_wrap_get_ghost (wrap),
                                     sizeof (int),
                                     domain->mirror_target_levels,
                                     domain->ghost_target_levels);

        /* loop through remote patches to process their target level */
        for (ng = 0; ng < domain->num_ghost_patches; ++ng)
        {
            gpatch = domain->ghost_patches + ng;
            gpatch->target_level = domain->ghost_target_levels[ng];
        }

        /* loop through local patches to determine their target level */
        for (nb = 0; nb < domain->num_blocks; ++nb)
        {
            block = domain->blocks + nb;
            for (np = 0; np < block->num_patches; ++np)
            {
                /* compute maximum target level between patch and neighbors */
                patch = block->patches + np;
                level = patch->level;
                max_tlevel = patch->target_level;

                /* loop through face neighbors of this patch */
                for (face = 0; max_tlevel <= level &&
                     face < wrap->p4est_faces; ++face)
                {
                    nrel = fclaw2d_patch_face_neighbors (domain, nb, np, face,
                                                         nprocs, &nblockno,
                                                         npatchno, &nfc);

                    /* we refine ourself if the neighbor wants to be finer */
                    if (nrel == FCLAW2D_PATCH_SAMESIZE)
                    {
                        npatch = fclaw2d_domain_get_neighbor_patch (domain,
                                                                    nprocs[0],
                                                                    nblockno,
                                                                    npatchno
                                                                    [0]);
                        P4EST_ASSERT (npatch->level == patch->level);
                        max_tlevel =
                            SC_MAX (max_tlevel, npatch->target_level);
                    }
                    else if (nrel == FCLAW2D_PATCH_DOUBLESIZE)
                    {
                        npatch = fclaw2d_domain_get_neighbor_patch (domain,
                                                                    nprocs[0],
                                                                    nblockno,
                                                                    npatchno
                                                                    [0]);
                        P4EST_ASSERT (npatch->level == patch->level - 1);
                        max_tlevel =
                            SC_MAX (max_tlevel, npatch->target_level);
                    }
                    else if (nrel == FCLAW2D_PATCH_HALFSIZE)
                    {
                        for (k = 0; k < 2; ++k)
                        {
                            npatch =
                                fclaw2d_domain_get_neighbor_patch (domain,
                                                                   nprocs[k],
                                                                   nblockno,
                                                                   npatchno
                                                                   [k]);
                            P4EST_ASSERT (npatch->level == patch->level + 1);
                            max_tlevel =
                                SC_MAX (max_tlevel, npatch->target_level);
                        }
                    }
                }

                /* loop through corner neighbors of this patch */
                for (corner = 0; max_tlevel <= level &&
                     corner < wrap->p4est_children; ++corner)
                {
                    exists = fclaw2d_patch_corner_neighbors (domain, nb, np,
                                                             corner, nprocs,
                                                             &nblockno,
                                                             npatchno,
                                                             &nfc, &nrel);
                    if (exists)
                    {
                        FCLAW_ASSERT (nrel != FCLAW2D_PATCH_BOUNDARY);
                        npatch =
                            fclaw2d_domain_get_neighbor_patch (domain,
                                                               nprocs[0],
                                                               nblockno,
                                                               npatchno[0]);
                        max_tlevel =
                            SC_MAX (max_tlevel, npatch->target_level);
                    }
                }

                /* pass refinement marker into the plumbing */
                if (max_tlevel < level)
                {
                    FCLAW_ASSERT (max_tlevel == level - 1);
                    p4est_wrap_mark_coarsen (wrap,
                                             (p4est_locidx_t) nb,
                                             (p4est_locidx_t) np);
                }
                else if (max_tlevel > level)
                {
                    /* max target level may be bigger by one or two */
                    p4est_wrap_mark_refine (wrap,
                                            (p4est_locidx_t) nb,
                                            (p4est_locidx_t) np);
                }
            }
        }
    }

    /* do the adaptation */
    if (p4est_wrap_adapt (wrap))
    {
        fclaw2d_domain_t *newd;

        domain->pp_owned = 0;
        newd = fclaw2d_domain_new (wrap, domain->attributes);
        newd->just_adapted = 1;

        fclaw2d_domain_copy_parameters (newd, domain);
        return newd;
    }
    else
    {
        return NULL;
    }
}

fclaw2d_domain_t *
fclaw2d_domain_partition (fclaw2d_domain_t * domain, int weight_exponent)
{
    p4est_wrap_t *wrap = (p4est_wrap_t *) domain->pp;
    p4est_locidx_t uf, ul, uof;

    FCLAW_ASSERT (domain->pp_owned);
    FCLAW_ASSERT (domain->just_adapted);
    FCLAW_ASSERT (!domain->just_partitioned);

    domain->just_adapted = 0;
    if (p4est_wrap_partition (wrap, weight_exponent, &uf, &ul, &uof))
    {
        fclaw2d_domain_t *newd;

        domain->pp_owned = 0;
        newd = fclaw2d_domain_new (wrap, domain->attributes);
        newd->just_partitioned = 1;
        newd->partition_unchanged_first = (int) uf;
        newd->partition_unchanged_length = (int) ul;
        newd->partition_unchanged_old_first = (int) uof;

        fclaw2d_domain_copy_parameters (newd, domain);
        return newd;
    }
    else
    {
        return NULL;
    }
}

void
fclaw2d_domain_partition_unchanged (fclaw2d_domain_t * domain,
                                    int *unchanged_first,
                                    int *unchanged_length,
                                    int *unchanged_old_first)
{
    FCLAW_ASSERT (domain->pp_owned);
    FCLAW_ASSERT (!domain->just_adapted);
    FCLAW_ASSERT (domain->just_partitioned);

    if (unchanged_first != NULL)
    {
        *unchanged_first = domain->partition_unchanged_first;
    }
    if (unchanged_length != NULL)
    {
        *unchanged_length = domain->partition_unchanged_length;
    }
    if (unchanged_old_first != NULL)
    {
        *unchanged_old_first = domain->partition_unchanged_old_first;
    }
}

void
fclaw2d_domain_complete (fclaw2d_domain_t * domain)
{
    p4est_wrap_t *wrap = (p4est_wrap_t *) domain->pp;

    FCLAW_ASSERT (domain->pp_owned);
    FCLAW_ASSERT (!domain->just_adapted);
    FCLAW_ASSERT (domain->just_partitioned);

    domain->just_partitioned = 0;
    domain->partition_unchanged_first = 0;
    domain->partition_unchanged_length = 0;
    domain->partition_unchanged_old_first = 0;

    p4est_wrap_complete (wrap);
}

static void
fclaw2d_domain_list_level_callback (fclaw2d_domain_t * domain,
                                    fclaw2d_patch_t * patch, int block_no,
                                    int patch_no, void *user)
{
    FCLAW_ASSERT (0 <= block_no && block_no < domain->num_blocks);
    FCLAW_ASSERT (0 <= patch_no &&
                  patch_no < domain->blocks[block_no].num_patches);
    FCLAW_ASSERT (patch == domain->blocks[block_no].patches + patch_no);

    (*(int *) user)++;
}

void
fclaw2d_domain_list_levels (fclaw2d_domain_t * domain, int lp)
{
    int level;
    int count, count_all;

    P4EST_LOGF (lp, "Local minimum/maximum levels: %2d %2d\n",
                domain->local_minlevel, domain->local_maxlevel);
    P4EST_GLOBAL_LOGF (lp, "Global minimum/maximum levels: %2d %2d\n",
                       domain->global_minlevel, domain->global_maxlevel);
    count_all = 0;
    for (level = domain->local_minlevel; level <= domain->local_maxlevel;
         ++level)
    {
        count = 0;
        fclaw2d_domain_iterate_level (domain, level,
                                      fclaw2d_domain_list_level_callback,
                                      &count);
        P4EST_LOGF (lp, "Patches on level %2d: %9d\n", level, count);
        count_all += count;
    }
    FCLAW_ASSERT (count_all == domain->local_num_patches);
}

typedef struct fclaw2d_domain_list_neighbors
{
    int lp;
    int count;
}
fclaw2d_domain_list_neighbors_t;

static void
fclaw2d_domain_list_neighbors_callback (fclaw2d_domain_t * domain,
                                        fclaw2d_patch_t * patch, int block_no,
                                        int patch_no, void *user)
{
    fclaw2d_domain_list_neighbors_t *ln =
        (fclaw2d_domain_list_neighbors_t *) user;
    fclaw2d_patch_relation_t fnt;
    int faceno, cornerno, rcorner;
    int rproc[2], rblockno, rpatchno[2], rfaceno;

    FCLAW_ASSERT (0 <= block_no && block_no < domain->num_blocks);
    FCLAW_ASSERT (0 <= patch_no &&
                  patch_no < domain->blocks[block_no].num_patches);
    FCLAW_ASSERT (patch == domain->blocks[block_no].patches + patch_no);

    for (faceno = 0; faceno < P4EST_FACES; ++faceno)
    {
        fnt = fclaw2d_patch_face_neighbors (domain, block_no, patch_no,
                                            faceno, rproc, &rblockno,
                                            rpatchno, &rfaceno);
        P4EST_LOGF (ln->lp, "Block %d patch %d face %d neighbor %d\n",
                    block_no, patch_no, faceno, (int) fnt);
    }
    for (cornerno = 0; cornerno < P4EST_CHILDREN; ++cornerno)
    {
        (void) fclaw2d_patch_corner_neighbors (domain, block_no, patch_no,
                                               cornerno, rproc, &rblockno,
                                               rpatchno, &rcorner, &fnt);
        P4EST_LOGF (ln->lp, "Block %d patch %d corner %d %d neighbor %d\n",
                    block_no, patch_no, cornerno, rcorner, (int) fnt);
    }

    ++ln->count;
}

void
fclaw2d_domain_list_neighbors (fclaw2d_domain_t * domain, int lp)
{
    fclaw2d_domain_list_neighbors_t ln;

    ln.lp = lp;
    ln.count = 0;
    fclaw2d_domain_iterate_patches (domain,
                                    fclaw2d_domain_list_neighbors_callback,
                                    &ln);
    FCLAW_ASSERT (ln.count == domain->local_num_patches);
}

static void
fclaw2d_domain_list_adapted_callback (fclaw2d_domain_t * old_domain,
                                      fclaw2d_patch_t * old_patch,
                                      fclaw2d_domain_t * new_domain,
                                      fclaw2d_patch_t * new_patch,
                                      fclaw2d_patch_relation_t newsize,
                                      int blockno,
                                      int old_patchno, int new_patchno,
                                      void *user)
{
    int lp = *(int *) user;
    int k;

    FCLAW_ASSERT (old_domain->pp == new_domain->pp);
    FCLAW_ASSERT (old_domain->num_blocks == new_domain->num_blocks);
    FCLAW_ASSERT (0 <= blockno && blockno < old_domain->num_blocks);
    FCLAW_ASSERT (0 <= old_patchno &&
                  old_patchno < old_domain->blocks[blockno].num_patches);
    FCLAW_ASSERT (0 <= new_patchno &&
                  new_patchno < new_domain->blocks[blockno].num_patches);
    FCLAW_ASSERT (old_patch ==
                  old_domain->blocks[blockno].patches + old_patchno);
    FCLAW_ASSERT (new_patch ==
                  new_domain->blocks[blockno].patches + new_patchno);

    if (newsize == FCLAW2D_PATCH_HALFSIZE)
    {
        /* refinement */
        FCLAW_ASSERT (new_patchno + P4EST_CHILDREN <=
                      new_domain->blocks[blockno].num_patches);
        for (k = 0; k < P4EST_CHILDREN; ++k)
        {
            FCLAW_ASSERT (new_patch[k].level == old_patch->level + 1);
            FCLAW_ASSERT (fclaw2d_patch_childid (&new_patch[k]) == k);
        }
        P4EST_LOGF (lp, "Block %d refinement %d to %d\n",
                    blockno, old_patchno, new_patchno);
    }
    else if (newsize == FCLAW2D_PATCH_DOUBLESIZE)
    {
        /* coarsening */
        FCLAW_ASSERT (old_patchno + P4EST_CHILDREN <=
                      old_domain->blocks[blockno].num_patches);
        for (k = 0; k < P4EST_CHILDREN; ++k)
        {
            FCLAW_ASSERT (old_patch[k].level == new_patch->level + 1);
            FCLAW_ASSERT (fclaw2d_patch_childid (&old_patch[k]) == k);
        }
        P4EST_LOGF (lp, "Block %d coarsening %d to %d\n",
                    blockno, old_patchno, new_patchno);
    }
    else
    {
        /* noop */
        FCLAW_ASSERT (newsize == FCLAW2D_PATCH_SAMESIZE);
        FCLAW_ASSERT (old_patch->level == new_patch->level);
        FCLAW_ASSERT (fclaw2d_patch_childid (old_patch) ==
                      fclaw2d_patch_childid (new_patch));
        P4EST_LOGF (lp, "Block %d noop patch %d and %d\n",
                    blockno, old_patchno, new_patchno);
    }
}

void
fclaw2d_domain_list_adapted (fclaw2d_domain_t * old_domain,
                             fclaw2d_domain_t * new_domain, int log_priority)
{
    fclaw2d_domain_iterate_adapted (old_domain, new_domain,
                                    fclaw2d_domain_list_adapted_callback,
                                    (void *) &log_priority);
}
