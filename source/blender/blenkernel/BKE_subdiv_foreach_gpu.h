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

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct Subdiv;
struct SubdivForeachContext;
struct SubdivToMeshSettings;

/* Invokes callbacks in the order and with values which corresponds to creation
 * of final subdivided mesh.
 *
 * Main goal is to abstract all the traversal routines to give geometry element
 * indices (for vertices, edges, loops, polygons) in the same way as subdivision
 * modifier will do for a dense mesh.
 *
 * Returns true if the whole topology was traversed, without any early exits.
 *
 * TODO(sergey): Need to either get rid of subdiv or of coarse_mesh.
 * The main point here is to be able to get base level topology, which can be
 * done with either of those. Having both of them is kind of redundant.
 */
bool BKE_subdiv_foreach_subdiv_geometry_cuda(struct Subdiv *subdiv,
                                        const struct SubdivForeachContext *context,
                                        const struct SubdivToMeshSettings *mesh_settings,
                                        const struct Mesh *coarse_mesh);

#ifdef __cplusplus
}
#endif