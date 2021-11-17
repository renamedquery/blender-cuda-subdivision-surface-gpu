/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

// this will be rewritten for nvcc

#include "BKE_subdiv_foreach.h"

#include "atomic_ops.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_bitmap.h"
#include "BLI_task.h"

#include "BKE_customdata.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.h"

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name General helpers
 * \{ */

/* Number of ptex faces for a given polygon. */

static void *subdiv_foreach_tls_alloc_cuda(SubdivForeachTaskContext *ctx)
{
  const SubdivForeachContext *foreach_context = ctx->foreach_context;
  void *tls = NULL;
  if (foreach_context->user_data_tls_size != 0) {
    tls = MEM_mallocN(foreach_context->user_data_tls_size, "tls");
    memcpy(tls, foreach_context->user_data_tls, foreach_context->user_data_tls_size);
  }
  return tls;
}

static void subdiv_foreach_tls_free_cuda(SubdivForeachTaskContext *ctx, void *tls)
{
  if (tls == NULL) {
    return;
  }
  if (ctx->foreach_context != NULL) {
    ctx->foreach_context->user_data_tls_free(tls);
  }
  MEM_freeN(tls);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initialization
 * \{ */

/* NOTE: Expects edge map to be zeroed. */
static void subdiv_foreach_ctx_count_cuda(SubdivForeachTaskContext *ctx)
{
  /* Reset counters. */
  ctx->num_subdiv_vertices = 0;
  ctx->num_subdiv_edges = 0;
  ctx->num_subdiv_loops = 0;
  ctx->num_subdiv_polygons = 0;
  /* Static geometry counters. */
  const int resolution = ctx->settings->resolution;
  const int no_quad_patch_resolution = ((resolution >> 1) + 1);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_inner_vertices_per_quad = (resolution - 2) * (resolution - 2);
  const int num_inner_vertices_per_noquad_patch = (no_quad_patch_resolution - 2) *
                                                  (no_quad_patch_resolution - 2);
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  ctx->num_subdiv_vertices = coarse_mesh->totvert;
  ctx->num_subdiv_edges = coarse_mesh->totedge * (num_subdiv_vertices_per_coarse_edge + 1);
  /* Calculate extra vertices and edges created by non-loose geometry. */
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    const int num_ptex_faces_per_poly = num_ptex_faces_per_poly_get(coarse_poly);
    for (int corner = 0; corner < coarse_poly->totloop; corner++) {
      const MLoop *loop = &coarse_mloop[coarse_poly->loopstart + corner];
      const bool is_edge_used = BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, loop->e);
      /* Edges which aren't counted yet. */
      if (!is_edge_used) {
        BLI_BITMAP_ENABLE(ctx->coarse_edges_used_map, loop->e);
        ctx->num_subdiv_vertices += num_subdiv_vertices_per_coarse_edge;
      }
    }
    /* Inner vertices of polygon. */
    if (num_ptex_faces_per_poly == 1) {
      ctx->num_subdiv_vertices += num_inner_vertices_per_quad;
      ctx->num_subdiv_edges += num_edges_per_ptex_face_get(resolution - 2) +
                               4 * num_subdiv_vertices_per_coarse_edge;
      ctx->num_subdiv_polygons += num_polys_per_ptex_get(resolution);
    }
    else {
      ctx->num_subdiv_vertices += 1 + num_ptex_faces_per_poly * (no_quad_patch_resolution - 2) +
                                  num_ptex_faces_per_poly * num_inner_vertices_per_noquad_patch;
      ctx->num_subdiv_edges += num_ptex_faces_per_poly *
                               (num_inner_edges_per_ptex_face_get(no_quad_patch_resolution - 1) +
                                (no_quad_patch_resolution - 2) +
                                num_subdiv_vertices_per_coarse_edge);
      if (no_quad_patch_resolution >= 3) {
        ctx->num_subdiv_edges += coarse_poly->totloop;
      }
      ctx->num_subdiv_polygons += num_ptex_faces_per_poly *
                                  num_polys_per_ptex_get(no_quad_patch_resolution);
    }
  }
  /* Calculate extra vertices created by loose edges. */
  for (int edge_index = 0; edge_index < coarse_mesh->totedge; edge_index++) {
    if (!BLI_BITMAP_TEST_BOOL(ctx->coarse_edges_used_map, edge_index)) {
      ctx->num_subdiv_vertices += num_subdiv_vertices_per_coarse_edge;
    }
  }
  ctx->num_subdiv_loops = ctx->num_subdiv_polygons * 4;
}

static void subdiv_foreach_ctx_init_offsets(SubdivForeachTaskContext *ctx)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const int resolution = ctx->settings->resolution;
  const int resolution_2 = resolution - 2;
  const int resolution_2_squared = resolution_2 * resolution_2;
  const int no_quad_patch_resolution = ((resolution >> 1) + 1);
  const int num_irregular_vertices_per_patch = (no_quad_patch_resolution - 2) *
                                               (no_quad_patch_resolution - 1);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_subdiv_edges_per_coarse_edge = resolution - 1;
  /* Constant offsets in arrays. */
  ctx->vertices_corner_offset = 0;
  ctx->vertices_edge_offset = coarse_mesh->totvert;
  ctx->vertices_inner_offset = ctx->vertices_edge_offset +
                               coarse_mesh->totedge * num_subdiv_vertices_per_coarse_edge;
  ctx->edge_boundary_offset = 0;
  ctx->edge_inner_offset = ctx->edge_boundary_offset +
                           coarse_mesh->totedge * num_subdiv_edges_per_coarse_edge;
  /* "Indexed" offsets. */
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  int vertex_offset = 0;
  int edge_offset = 0;
  int polygon_offset = 0;
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    const int num_ptex_faces_per_poly = num_ptex_faces_per_poly_get(coarse_poly);
    ctx->subdiv_vertex_offset[poly_index] = vertex_offset;
    ctx->subdiv_edge_offset[poly_index] = edge_offset;
    ctx->subdiv_polygon_offset[poly_index] = polygon_offset;
    if (num_ptex_faces_per_poly == 1) {
      vertex_offset += resolution_2_squared;
      edge_offset += num_edges_per_ptex_face_get(resolution - 2) +
                     4 * num_subdiv_vertices_per_coarse_edge;
      polygon_offset += num_polys_per_ptex_get(resolution);
    }
    else {
      vertex_offset += 1 + num_ptex_faces_per_poly * num_irregular_vertices_per_patch;
      edge_offset += num_ptex_faces_per_poly *
                     (num_inner_edges_per_ptex_face_get(no_quad_patch_resolution - 1) +
                      (no_quad_patch_resolution - 2) + num_subdiv_vertices_per_coarse_edge);
      if (no_quad_patch_resolution >= 3) {
        edge_offset += coarse_poly->totloop;
      }
      polygon_offset += num_ptex_faces_per_poly * num_polys_per_ptex_get(no_quad_patch_resolution);
    }
  }
}

static void subdiv_foreach_ctx_init_cuda(Subdiv *subdiv, SubdivForeachTaskContext *ctx)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  /* Allocate maps and offsets. */
  ctx->coarse_vertices_used_map = BLI_BITMAP_NEW(coarse_mesh->totvert, "vertices used map");
  ctx->coarse_edges_used_map = BLI_BITMAP_NEW(coarse_mesh->totedge, "edges used map");
  ctx->subdiv_vertex_offset = MEM_malloc_arrayN(
      coarse_mesh->totpoly, sizeof(*ctx->subdiv_vertex_offset), "vertex_offset");
  ctx->subdiv_edge_offset = MEM_malloc_arrayN(
      coarse_mesh->totpoly, sizeof(*ctx->subdiv_edge_offset), "subdiv_edge_offset");
  ctx->subdiv_polygon_offset = MEM_malloc_arrayN(
      coarse_mesh->totpoly, sizeof(*ctx->subdiv_polygon_offset), "subdiv_edge_offset");
  /* Initialize all offsets. */
  subdiv_foreach_ctx_init_offsets(ctx);
  /* Calculate number of geometry in the result subdivision mesh. */
  subdiv_foreach_ctx_count(ctx);
  /* Re-set maps which were used at this step. */
  BLI_bitmap_set_all(ctx->coarse_edges_used_map, false, coarse_mesh->totedge);
  ctx->face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv);
}

static void subdiv_foreach_ctx_free_cuda(SubdivForeachTaskContext *ctx)
{
  MEM_freeN(ctx->coarse_vertices_used_map);
  MEM_freeN(ctx->coarse_edges_used_map);
  MEM_freeN(ctx->subdiv_vertex_offset);
  MEM_freeN(ctx->subdiv_edge_offset);
  MEM_freeN(ctx->subdiv_polygon_offset);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex traversal process
 * \{ */

/* Traversal of corner vertices. They are coming from coarse vertices. */

static void subdiv_foreach_corner_vertices_regular_do(
    SubdivForeachTaskContext *ctx,
    void *tls,
    const MPoly *coarse_poly,
    SubdivForeachVertexFromCornerCb vertex_corner,
    bool check_usage)
{
  const float weights[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const int coarse_poly_index = coarse_poly - coarse_mesh->mpoly;
  const int ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    if (check_usage &&
        BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_vertices_used_map, coarse_loop->v)) {
      continue;
    }
    const int coarse_vertex_index = coarse_loop->v;
    const int subdiv_vertex_index = ctx->vertices_corner_offset + coarse_vertex_index;
    const float u = weights[corner][0];
    const float v = weights[corner][1];
    vertex_corner(ctx->foreach_context,
                  tls,
                  ptex_face_index,
                  u,
                  v,
                  coarse_vertex_index,
                  coarse_poly_index,
                  0,
                  subdiv_vertex_index);
  }
}

static void subdiv_foreach_corner_vertices_regular_cuda(SubdivForeachTaskContext *ctx,
                                                   void *tls,
                                                   const MPoly *coarse_poly)
{
  subdiv_foreach_corner_vertices_regular_do_cuda(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_corner, true);
}

static void subdiv_foreach_corner_vertices_special_do_cuda(
    SubdivForeachTaskContext *ctx,
    void *tls,
    const MPoly *coarse_poly,
    SubdivForeachVertexFromCornerCb vertex_corner,
    bool check_usage)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const int coarse_poly_index = coarse_poly - coarse_mesh->mpoly;
  int ptex_face_index = ctx->face_ptex_offset[coarse_poly_index];
  for (int corner = 0; corner < coarse_poly->totloop; corner++, ptex_face_index++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    if (check_usage &&
        BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_vertices_used_map, coarse_loop->v)) {
      continue;
    }
    const int coarse_vertex_index = coarse_loop->v;
    const int subdiv_vertex_index = ctx->vertices_corner_offset + coarse_vertex_index;
    vertex_corner(ctx->foreach_context,
                  tls,
                  ptex_face_index,
                  0.0f,
                  0.0f,
                  coarse_vertex_index,
                  coarse_poly_index,
                  corner,
                  subdiv_vertex_index);
  }
}

static void subdiv_foreach_corner_vertices_special_cuda(SubdivForeachTaskContext *ctx,
                                                   void *tls,
                                                   const MPoly *coarse_poly)
{
  subdiv_foreach_corner_vertices_special_do_cuda(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_corner, true);
}

static void subdiv_foreach_corner_vertices_cuda(SubdivForeachTaskContext *ctx,
                                           void *tls,
                                           const MPoly *coarse_poly)
{
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_corner_vertices_regular_cuda(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_corner_vertices_special_cuda(ctx, tls, coarse_poly);
  }
}

static void subdiv_foreach_every_corner_vertices_regular_cuda(SubdivForeachTaskContext *ctx,
                                                         void *tls,
                                                         const MPoly *coarse_poly)
{
  subdiv_foreach_corner_vertices_regular_do_cuda(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_every_corner, false);
}

static void subdiv_foreach_every_corner_vertices_special_cuda(SubdivForeachTaskContext *ctx,
                                                         void *tls,
                                                         const MPoly *coarse_poly)
{
  subdiv_foreach_corner_vertices_special_do_cuda(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_every_corner, false);
}

static void subdiv_foreach_every_corner_vertices_cuda(SubdivForeachTaskContext *ctx, void *tls)
{
  if (ctx->foreach_context->vertex_every_corner == NULL) {
    return;
  }
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    if (coarse_poly->totloop == 4) {
      subdiv_foreach_every_corner_vertices_regular_cuda(ctx, tls, coarse_poly);
    }
    else {
      subdiv_foreach_every_corner_vertices_special_cuda(ctx, tls, coarse_poly);
    }
  }
}

/* Traverse of edge vertices. They are coming from coarse edges. */

static void subdiv_foreach_edge_vertices_regular_do_cuda(SubdivForeachTaskContext *ctx,
                                                    void *tls,
                                                    const MPoly *coarse_poly,
                                                    SubdivForeachVertexFromEdgeCb vertex_edge,
                                                    bool check_usage)
{
  const int resolution = ctx->settings->resolution;
  const int resolution_1 = resolution - 1;
  const float inv_resolution_1 = 1.0f / (float)resolution_1;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int coarse_poly_index = coarse_poly - coarse_mpoly;
  const int poly_index = coarse_poly - coarse_mesh->mpoly;
  const int ptex_face_index = ctx->face_ptex_offset[poly_index];
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    const int coarse_edge_index = coarse_loop->e;
    if (check_usage &&
        BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_edges_used_map, coarse_edge_index)) {
      continue;
    }
    const MEdge *coarse_edge = &coarse_medge[coarse_edge_index];
    const bool flip = (coarse_edge->v2 == coarse_loop->v);
    int subdiv_vertex_index = ctx->vertices_edge_offset +
                              coarse_edge_index * num_subdiv_vertices_per_coarse_edge;
    for (int vertex_index = 0; vertex_index < num_subdiv_vertices_per_coarse_edge;
         vertex_index++, subdiv_vertex_index++) {
      float fac = (vertex_index + 1) * inv_resolution_1;
      if (flip) {
        fac = 1.0f - fac;
      }
      if (corner >= 2) {
        fac = 1.0f - fac;
      }
      float u, v;
      if ((corner & 1) == 0) {
        u = fac;
        v = (corner == 2) ? 1.0f : 0.0f;
      }
      else {
        u = (corner == 1) ? 1.0f : 0.0f;
        v = fac;
      }
      vertex_edge(ctx->foreach_context,
                  tls,
                  ptex_face_index,
                  u,
                  v,
                  coarse_edge_index,
                  coarse_poly_index,
                  0,
                  subdiv_vertex_index);
    }
  }
}

static void subdiv_foreach_edge_vertices_regular_cuda(SubdivForeachTaskContext *ctx,
                                                 void *tls,
                                                 const MPoly *coarse_poly)
{
  subdiv_foreach_edge_vertices_regular_do_cuda(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_edge, true);
}

static void subdiv_foreach_edge_vertices_special_do_cuda(SubdivForeachTaskContext *ctx,
                                                    void *tls,
                                                    const MPoly *coarse_poly,
                                                    SubdivForeachVertexFromEdgeCb vertex_edge,
                                                    bool check_usage)
{
  const int resolution = ctx->settings->resolution;
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int num_vertices_per_ptex_edge = ((resolution >> 1) + 1);
  const float inv_ptex_resolution_1 = 1.0f / (float)(num_vertices_per_ptex_edge - 1);
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int coarse_poly_index = coarse_poly - coarse_mpoly;
  const int poly_index = coarse_poly - coarse_mesh->mpoly;
  const int ptex_face_start_index = ctx->face_ptex_offset[poly_index];
  int ptex_face_index = ptex_face_start_index;
  for (int corner = 0; corner < coarse_poly->totloop; corner++, ptex_face_index++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    const int coarse_edge_index = coarse_loop->e;
    if (check_usage &&
        BLI_BITMAP_TEST_AND_SET_ATOMIC(ctx->coarse_edges_used_map, coarse_edge_index)) {
      continue;
    }
    const MEdge *coarse_edge = &coarse_medge[coarse_edge_index];
    const bool flip = (coarse_edge->v2 == coarse_loop->v);
    int subdiv_vertex_index = ctx->vertices_edge_offset +
                              coarse_edge_index * num_subdiv_vertices_per_coarse_edge;
    int vertex_delta = 1;
    if (flip) {
      subdiv_vertex_index += num_subdiv_vertices_per_coarse_edge - 1;
      vertex_delta = -1;
    }
    for (int vertex_index = 1; vertex_index < num_vertices_per_ptex_edge;
         vertex_index++, subdiv_vertex_index += vertex_delta) {
      const float u = vertex_index * inv_ptex_resolution_1;
      vertex_edge(ctx->foreach_context,
                  tls,
                  ptex_face_index,
                  u,
                  0.0f,
                  coarse_edge_index,
                  coarse_poly_index,
                  corner,
                  subdiv_vertex_index);
    }
    const int next_corner = (corner + 1) % coarse_poly->totloop;
    const int next_ptex_face_index = ptex_face_start_index + next_corner;
    for (int vertex_index = 1; vertex_index < num_vertices_per_ptex_edge - 1;
         vertex_index++, subdiv_vertex_index += vertex_delta) {
      const float v = 1.0f - vertex_index * inv_ptex_resolution_1;
      vertex_edge(ctx->foreach_context,
                  tls,
                  next_ptex_face_index,
                  0.0f,
                  v,
                  coarse_edge_index,
                  coarse_poly_index,
                  next_corner,
                  subdiv_vertex_index);
    }
  }
}

static void subdiv_foreach_edge_vertices_special_cuda(SubdivForeachTaskContext *ctx,
                                                 void *tls,
                                                 const MPoly *coarse_poly)
{
  subdiv_foreach_edge_vertices_special_do_cuda(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_edge, true);
}

static void subdiv_foreach_edge_vertices_cuda(SubdivForeachTaskContext *ctx,
                                         void *tls,
                                         const MPoly *coarse_poly)
{
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_edge_vertices_regular_cuda(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_edge_vertices_special_cuda(ctx, tls, coarse_poly);
  }
}

static void subdiv_foreach_every_edge_vertices_regular_cuda(SubdivForeachTaskContext *ctx,
                                                       void *tls,
                                                       const MPoly *coarse_poly)
{
  subdiv_foreach_edge_vertices_regular_do_cuda(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_every_edge, false);
}

static void subdiv_foreach_every_edge_vertices_special_cuda(SubdivForeachTaskContext *ctx,
                                                       void *tls,
                                                       const MPoly *coarse_poly)
{
  subdiv_foreach_edge_vertices_special_do_cuda(
      ctx, tls, coarse_poly, ctx->foreach_context->vertex_every_edge, false);
}

static void subdiv_foreach_every_edge_vertices_cuda(SubdivForeachTaskContext *ctx, void *tls)
{
  if (ctx->foreach_context->vertex_every_edge == NULL) {
    return;
  }
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    if (coarse_poly->totloop == 4) {
      subdiv_foreach_every_edge_vertices_regular_cuda(ctx, tls, coarse_poly);
    }
    else {
      subdiv_foreach_every_edge_vertices_special_cuda(ctx, tls, coarse_poly);
    }
  }
}

/* Traversal of inner vertices, they are coming from ptex patches. */

static void subdiv_foreach_inner_vertices_cuda(SubdivForeachTaskContext *ctx,
                                          void *tls,
                                          const MPoly *coarse_poly)
{
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_inner_vertices_regular(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_inner_vertices_special(ctx, tls, coarse_poly);
  }
}

/* Traverse all vertices which are emitted from given coarse polygon. */
static void subdiv_foreach_vertices_cuda(SubdivForeachTaskContext *ctx, void *tls, const int poly_index)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[poly_index];
  if (ctx->foreach_context->vertex_inner != NULL) {
    subdiv_foreach_inner_vertices_cuda(ctx, tls, coarse_poly);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge traversal process
 * \{ */

/* TODO(sergey): Coarse edge are always NONE, consider getting rid of it. */
static int subdiv_foreach_edges_row_cuda(SubdivForeachTaskContext *ctx,
                                    void *tls,
                                    const int coarse_edge_index,
                                    const int start_subdiv_edge_index,
                                    const int start_vertex_index,
                                    const int num_edges_per_row)
{
  int subdiv_edge_index = start_subdiv_edge_index;
  int vertex_index = start_vertex_index;
  for (int edge_index = 0; edge_index < num_edges_per_row - 1; edge_index++, subdiv_edge_index++) {
    const int v1 = vertex_index;
    const int v2 = vertex_index + 1;
    ctx->foreach_context->edge(
        ctx->foreach_context, tls, coarse_edge_index, subdiv_edge_index, v1, v2);
    vertex_index += 1;
  }
  return subdiv_edge_index;
}

/* TODO(sergey): Coarse edges are always NONE, consider getting rid of them. */
static int subdiv_foreach_edges_column_cuda(SubdivForeachTaskContext *ctx,
                                       void *tls,
                                       const int coarse_start_edge_index,
                                       const int coarse_end_edge_index,
                                       const int start_subdiv_edge_index,
                                       const int start_vertex_index,
                                       const int num_edges_per_row)
{
  int subdiv_edge_index = start_subdiv_edge_index;
  int vertex_index = start_vertex_index;
  for (int edge_index = 0; edge_index < num_edges_per_row; edge_index++, subdiv_edge_index++) {
    int coarse_edge_index = ORIGINDEX_NONE;
    if (edge_index == 0) {
      coarse_edge_index = coarse_start_edge_index;
    }
    else if (edge_index == num_edges_per_row - 1) {
      coarse_edge_index = coarse_end_edge_index;
    }
    const int v1 = vertex_index;
    const int v2 = vertex_index + num_edges_per_row;
    ctx->foreach_context->edge(
        ctx->foreach_context, tls, coarse_edge_index, subdiv_edge_index, v1, v2);
    vertex_index += 1;
  }
  return subdiv_edge_index;
}

/* Defines edges between inner vertices of patch, and also edges to the
 * boundary.
 */

/* Consider a subdivision of base face at level 1:
 *
 *  y
 *  ^
 *  |   (6) ---- (7) ---- (8)
 *  |    |        |        |
 *  |   (3) ---- (4) ---- (5)
 *  |    |        |        |
 *  |   (0) ---- (1) ---- (2)
 *  o---------------------------> x
 *
 * This is illustrate which parts of geometry is created by code below.
 */

static void subdiv_foreach_edges_all_patches_regular_cuda(SubdivForeachTaskContext *ctx,
                                                     void *tls,
                                                     const MPoly *coarse_poly)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int poly_index = coarse_poly - coarse_mpoly;
  const int resolution = ctx->settings->resolution;
  const int start_vertex_index = ctx->vertices_inner_offset +
                                 ctx->subdiv_vertex_offset[poly_index];
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  int subdiv_edge_index = ctx->edge_inner_offset + ctx->subdiv_edge_offset[poly_index];
  /* Traverse bottom row of edges (0-1, 1-2). */
  subdiv_edge_index = subdiv_foreach_edges_row_cuda(
      ctx, tls, ORIGINDEX_NONE, subdiv_edge_index, start_vertex_index, resolution - 2);
  /* Traverse remaining edges. */
  for (int row = 0; row < resolution - 3; row++) {
    const int start_row_vertex_index = start_vertex_index + row * (resolution - 2);
    /* Traverse vertical columns.
     *
     * At first iteration it will be edges (0-3. 1-4, 2-5), then it
     * will be (3-6, 4-7, 5-8) and so on.
     */
    subdiv_edge_index = subdiv_foreach_edges_column_cuda(ctx,
                                                    tls,
                                                    ORIGINDEX_NONE,
                                                    ORIGINDEX_NONE,
                                                    subdiv_edge_index,
                                                    start_row_vertex_index,
                                                    resolution - 2);
    /* Create horizontal edge row.
     *
     * At first iteration it will be edges (3-4, 4-5), then it will be
     * (6-7, 7-8) and so on.
     */
    subdiv_edge_index = subdiv_foreach_edges_row_cuda(ctx,
                                                 tls,
                                                 ORIGINDEX_NONE,
                                                 subdiv_edge_index,
                                                 start_row_vertex_index + resolution - 2,
                                                 resolution - 2);
  }
  /* Connect inner part of patch to boundary. */
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
    const int start_edge_vertex = ctx->vertices_edge_offset +
                                  coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
    const bool flip = (coarse_edge->v2 == coarse_loop->v);
    int side_start_index = start_vertex_index;
    int side_stride = 0;
    /* Calculate starting vertex of corresponding inner part of ptex. */
    if (corner == 0) {
      side_stride = 1;
    }
    else if (corner == 1) {
      side_start_index += resolution - 3;
      side_stride = resolution - 2;
    }
    else if (corner == 2) {
      side_start_index += num_subdiv_vertices_per_coarse_edge *
                              num_subdiv_vertices_per_coarse_edge -
                          1;
      side_stride = -1;
    }
    else if (corner == 3) {
      side_start_index += num_subdiv_vertices_per_coarse_edge *
                          (num_subdiv_vertices_per_coarse_edge - 1);
      side_stride = -(resolution - 2);
    }
    for (int i = 0; i < resolution - 2; i++, subdiv_edge_index++) {
      const int v1 = (flip) ? (start_edge_vertex + (resolution - i - 3)) : (start_edge_vertex + i);
      const int v2 = side_start_index + side_stride * i;
      ctx->foreach_context->edge(
          ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
    }
  }
}

static void subdiv_foreach_edges_all_patches_special_cuda(SubdivForeachTaskContext *ctx,
                                                     void *tls,
                                                     const MPoly *coarse_poly)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MEdge *coarse_medge = coarse_mesh->medge;
  const MLoop *coarse_mloop = coarse_mesh->mloop;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const int poly_index = coarse_poly - coarse_mpoly;
  const int resolution = ctx->settings->resolution;
  const int ptex_face_resolution = ptex_face_resolution_get(coarse_poly, resolution);
  const int ptex_face_inner_resolution = ptex_face_resolution - 2;
  const int num_inner_vertices_per_ptex = (ptex_face_resolution - 1) * (ptex_face_resolution - 2);
  const int num_subdiv_vertices_per_coarse_edge = resolution - 2;
  const int center_vertex_index = ctx->vertices_inner_offset +
                                  ctx->subdiv_vertex_offset[poly_index];
  const int start_vertex_index = center_vertex_index + 1;
  int subdiv_edge_index = ctx->edge_inner_offset + ctx->subdiv_edge_offset[poly_index];
  /* Traverse inner ptex edges. */
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const int start_ptex_face_vertex_index = start_vertex_index +
                                             corner * num_inner_vertices_per_ptex;
    /* Similar steps to regular patch case. */
    subdiv_edge_index = subdiv_foreach_edges_row_cuda(ctx,
                                                 tls,
                                                 ORIGINDEX_NONE,
                                                 subdiv_edge_index,
                                                 start_ptex_face_vertex_index,
                                                 ptex_face_inner_resolution + 1);
    for (int row = 0; row < ptex_face_inner_resolution - 1; row++) {
      const int start_row_vertex_index = start_ptex_face_vertex_index +
                                         row * (ptex_face_inner_resolution + 1);
      subdiv_edge_index = subdiv_foreach_edges_column_cuda(ctx,
                                                      tls,
                                                      ORIGINDEX_NONE,
                                                      ORIGINDEX_NONE,
                                                      subdiv_edge_index,
                                                      start_row_vertex_index,
                                                      ptex_face_inner_resolution + 1);
      subdiv_edge_index = subdiv_foreach_edges_row_cuda(ctx,
                                                   tls,
                                                   ORIGINDEX_NONE,
                                                   subdiv_edge_index,
                                                   start_row_vertex_index +
                                                       ptex_face_inner_resolution + 1,
                                                   ptex_face_inner_resolution + 1);
    }
  }
  /* Create connections between ptex faces. */
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const int next_corner = (corner + 1) % coarse_poly->totloop;
    int current_patch_vertex_index = start_vertex_index + corner * num_inner_vertices_per_ptex +
                                     ptex_face_inner_resolution;
    int next_path_vertex_index = start_vertex_index + next_corner * num_inner_vertices_per_ptex +
                                 num_inner_vertices_per_ptex - ptex_face_resolution + 1;
    for (int row = 0; row < ptex_face_inner_resolution; row++, subdiv_edge_index++) {
      const int v1 = current_patch_vertex_index;
      const int v2 = next_path_vertex_index;
      ctx->foreach_context->edge(
          ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
      current_patch_vertex_index += ptex_face_inner_resolution + 1;
      next_path_vertex_index += 1;
    }
  }
  /* Create edges from center. */
  if (ptex_face_resolution >= 3) {
    for (int corner = 0; corner < coarse_poly->totloop; corner++, subdiv_edge_index++) {
      const int current_patch_end_vertex_index = start_vertex_index +
                                                 corner * num_inner_vertices_per_ptex +
                                                 num_inner_vertices_per_ptex - 1;
      const int v1 = center_vertex_index;
      const int v2 = current_patch_end_vertex_index;
      ctx->foreach_context->edge(
          ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
    }
  }
  /* Connect inner path of patch to boundary. */
  const MLoop *prev_coarse_loop = &coarse_mloop[coarse_poly->loopstart + coarse_poly->totloop - 1];
  for (int corner = 0; corner < coarse_poly->totloop; corner++) {
    const MLoop *coarse_loop = &coarse_mloop[coarse_poly->loopstart + corner];
    {
      const MEdge *coarse_edge = &coarse_medge[coarse_loop->e];
      const int start_edge_vertex = ctx->vertices_edge_offset +
                                    coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
      const bool flip = (coarse_edge->v2 == coarse_loop->v);
      int side_start_index;
      if (ptex_face_resolution >= 3) {
        side_start_index = start_vertex_index + num_inner_vertices_per_ptex * corner;
      }
      else {
        side_start_index = center_vertex_index;
      }
      for (int i = 0; i < ptex_face_resolution - 1; i++, subdiv_edge_index++) {
        const int v1 = (flip) ? (start_edge_vertex + (resolution - i - 3)) :
                                (start_edge_vertex + i);
        const int v2 = side_start_index + i;
        ctx->foreach_context->edge(
            ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
      }
    }
    if (ptex_face_resolution >= 3) {
      const MEdge *coarse_edge = &coarse_medge[prev_coarse_loop->e];
      const int start_edge_vertex = ctx->vertices_edge_offset +
                                    prev_coarse_loop->e * num_subdiv_vertices_per_coarse_edge;
      const bool flip = (coarse_edge->v2 == coarse_loop->v);
      int side_start_index = start_vertex_index + num_inner_vertices_per_ptex * corner;
      for (int i = 0; i < ptex_face_resolution - 2; i++, subdiv_edge_index++) {
        const int v1 = (flip) ? (start_edge_vertex + (resolution - i - 3)) :
                                (start_edge_vertex + i);
        const int v2 = side_start_index + (ptex_face_inner_resolution + 1) * i;
        ctx->foreach_context->edge(
            ctx->foreach_context, tls, ORIGINDEX_NONE, subdiv_edge_index, v1, v2);
      }
    }
    prev_coarse_loop = coarse_loop;
  }
}

static void subdiv_foreach_edges_all_patches_cuda(SubdivForeachTaskContext *ctx,
                                             void *tls,
                                             const MPoly *coarse_poly)
{
  if (coarse_poly->totloop == 4) {
    subdiv_foreach_edges_all_patches_regular_cuda(ctx, tls, coarse_poly);
  }
  else {
    subdiv_foreach_edges_all_patches_special_cuda(ctx, tls, coarse_poly);
  }
}

static void subdiv_foreach_edges_cuda(SubdivForeachTaskContext *ctx, void *tls, int poly_index)
{
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  const MPoly *coarse_poly = &coarse_mpoly[poly_index];
  subdiv_foreach_edges_all_patches_cuda(ctx, tls, coarse_poly);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subdivision process entry points
 * \{ */

static void subdiv_foreach_single_geometry_vertices_cuda(SubdivForeachTaskContext *ctx, void *tls)
{
  if (ctx->foreach_context->vertex_corner == NULL) {
    return;
  }
  const Mesh *coarse_mesh = ctx->coarse_mesh;
  const MPoly *coarse_mpoly = coarse_mesh->mpoly;
  for (int poly_index = 0; poly_index < coarse_mesh->totpoly; poly_index++) {
    const MPoly *coarse_poly = &coarse_mpoly[poly_index];
    subdiv_foreach_corner_vertices_cuda(ctx, tls, coarse_poly);
    subdiv_foreach_edge_vertices_cuda(ctx, tls, coarse_poly);
  }
}

static void subdiv_foreach_single_thread_tasks(SubdivForeachTaskContext *ctx)
{
  /* NOTE: In theory, we can try to skip allocation of TLS here, but in
   * practice if the callbacks used here are not specified then TLS will not
   * be requested anyway. */
  void *tls = subdiv_foreach_tls_alloc_cuda(ctx);
  /* Passes to average displacement on the corner vertices
   * and boundary edges. */
  subdiv_foreach_every_corner_vertices_cuda(ctx, tls);
  subdiv_foreach_every_edge_vertices_cuda(ctx, tls);
  /* Run callbacks which are supposed to be run once per shared geometry. */
  subdiv_foreach_single_geometry_vertices_cuda(ctx, tls);
  subdiv_foreach_tls_free_cuda(ctx, tls);

  const SubdivForeachContext *foreach_context = ctx->foreach_context;
  const bool is_loose_geometry_tagged = (foreach_context->vertex_every_edge != NULL &&
                                         foreach_context->vertex_every_corner != NULL);
  const bool is_loose_geometry_tags_needed = (foreach_context->vertex_loose != NULL ||
                                              foreach_context->vertex_of_loose_edge != NULL);
  if (is_loose_geometry_tagged && is_loose_geometry_tags_needed) {
    subdiv_foreach_mark_non_loose_geometry(ctx);
  }
}

static void subdiv_foreach_task(void *__restrict userdata,
                                const int poly_index,
                                const TaskParallelTLS *__restrict tls)
{
  SubdivForeachTaskContext *ctx = userdata;
  /* Traverse hi-poly vertex coordinates and normals. */
  subdiv_foreach_vertices_cuda(ctx, tls->userdata_chunk, poly_index);
  /* Traverse mesh geometry for the given base poly index. */
  if (ctx->foreach_context->edge != NULL) {
    subdiv_foreach_edges_cuda(ctx, tls->userdata_chunk, poly_index);
  }
  if (ctx->foreach_context->loop != NULL) {
    subdiv_foreach_loops(ctx, tls->userdata_chunk, poly_index);
  }
  if (ctx->foreach_context->poly != NULL) {
    subdiv_foreach_polys(ctx, tls->userdata_chunk, poly_index);
  }
}

bool BKE_subdiv_foreach_subdiv_geometry_cuda(Subdiv *subdiv,
                                        const SubdivForeachContext *context,
                                        const SubdivToMeshSettings *mesh_settings,
                                        const Mesh *coarse_mesh)
{
  SubdivForeachTaskContext ctx = {0};
  ctx.coarse_mesh = coarse_mesh;
  ctx.settings = mesh_settings;
  ctx.foreach_context = context;
  subdiv_foreach_ctx_init_cuda(subdiv, &ctx);
  if (context->topology_info != NULL) {
    if (!context->topology_info(context,
                                ctx.num_subdiv_vertices,
                                ctx.num_subdiv_edges,
                                ctx.num_subdiv_loops,
                                ctx.num_subdiv_polygons)) {
      subdiv_foreach_ctx_free_cuda(&ctx);
      return false;
    }
  }
  /* Run all the code which is not supposed to be run from threads. */
  subdiv_foreach_single_thread_tasks(&ctx);
  /* Threaded traversal of the rest of topology. */
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  parallel_range_settings.userdata_chunk = context->user_data_tls;
  parallel_range_settings.userdata_chunk_size = context->user_data_tls_size;
  parallel_range_settings.min_iter_per_thread = 1;
  if (context->user_data_tls_free != NULL) {
    parallel_range_settings.func_free = subdiv_foreach_free;
  }

  /* TODO(sergey): Possible optimization is to have a single pool and push all
   * the tasks into it.
   * NOTE: Watch out for callbacks which needs to run for loose geometry as they
   * currently are relying on the fact that face/grid callbacks will tag non-
   * loose geometry. */

  // CUDA TODO below

  BLI_task_parallel_range(
      0, coarse_mesh->totpoly, &ctx, subdiv_foreach_task, &parallel_range_settings);
  if (context->vertex_loose != NULL) {
    BLI_task_parallel_range(0,
                            coarse_mesh->totvert,
                            &ctx,
                            subdiv_foreach_loose_vertices_task,
                            &parallel_range_settings);
  }
  if (context->vertex_of_loose_edge != NULL) {
    BLI_task_parallel_range(0,
                            coarse_mesh->totedge,
                            &ctx,
                            subdiv_foreach_vertices_of_loose_edges_task,
                            &parallel_range_settings);
  }
  if (context->edge != NULL) {
    BLI_task_parallel_range(0,
                            coarse_mesh->totedge,
                            &ctx,
                            subdiv_foreach_boundary_edges_task,
                            &parallel_range_settings);
  }
  subdiv_foreach_ctx_free_cuda(&ctx);
  return true;
}

/** \} */