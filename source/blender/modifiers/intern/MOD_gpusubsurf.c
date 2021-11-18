// adapted from the instructions on https://blog.exppad.com/article/writing-blender-modifier

// code copied from MOD_subsurf.c with a few modifications by katznboyz

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_deform.h"
#include "BKE_subdiv_mesh.h"
#include "BKE_subdiv_mesh_gpu.cuh"
#include "BKE_subsurf.h"
#include "BKE_subdiv_foreach.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RE_engine.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "BLO_read_write.h"

#include "intern/CCGSubSurf.h"

typedef struct SubsurfRuntimeData {
  /* Cached subdivision surface descriptor, with topology and settings. */
  struct Subdiv *subdiv;
} SubsurfRuntimeData;

static int subdiv_levels_for_modifier_get(const SubsurfModifierData *smd,
                                          const ModifierEvalContext *ctx)
{
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
  const int requested_levels = (use_render_params) ? smd->renderLevels : smd->levels;
  return get_render_subsurf_level(&scene->r, requested_levels, use_render_params);
}

static void subdiv_settings_init(SubdivSettings *settings,
                                 const SubsurfModifierData *smd,
                                 const ModifierEvalContext *ctx)
{
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
  const int requested_levels = (use_render_params) ? smd->renderLevels : smd->levels;

  settings->is_simple = (smd->subdivType == SUBSURF_TYPE_SIMPLE);
  settings->is_adaptive = !(smd->flags & eSubsurfModifierFlag_UseRecursiveSubdivision);
  settings->level = settings->is_simple ?
                        1 :
                        (settings->is_adaptive ? smd->quality : requested_levels);
  settings->use_creases = (smd->flags & eSubsurfModifierFlag_UseCrease);
  settings->vtx_boundary_interpolation = BKE_subdiv_vtx_boundary_interpolation_from_subsurf(
      smd->boundary_smooth);
  settings->fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
      smd->uv_smooth);
}

/* Main goal of this function is to give usable subdivision surface descriptor
 * which matches settings and topology. */
static Subdiv *subdiv_descriptor_ensure(SubsurfModifierData *smd,
                                        const SubdivSettings *subdiv_settings,
                                        const Mesh *mesh)
{
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)smd->modifier.runtime;
  Subdiv *subdiv = BKE_subdiv_update_from_mesh(runtime_data->subdiv, subdiv_settings, mesh);
  runtime_data->subdiv = subdiv;
  return subdiv;
}

/* Subdivide into fully qualified mesh. */

static void subdiv_mesh_settings_init(SubdivToMeshSettings *settings,
                                      const SubsurfModifierData *smd,
                                      const ModifierEvalContext *ctx)
{
  const int level = subdiv_levels_for_modifier_get(smd, ctx);
  settings->resolution = (1 << level) + 1;
  settings->use_optimal_display = (smd->flags & eSubsurfModifierFlag_ControlEdges) &&
                                  !(ctx->flag & MOD_APPLY_TO_BASE_MESH);
}

static Mesh *subdiv_as_mesh_cuda(SubsurfModifierData *smd,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            Subdiv *subdiv)
{
  Mesh *result = mesh;
  SubdivToMeshSettings mesh_settings;
  subdiv_mesh_settings_init(&mesh_settings, smd, ctx);
  if (mesh_settings.resolution < 3) {
    return result;
  }
  result = BKE_subdiv_to_mesh_cuda(subdiv, &mesh_settings, mesh);
  return result;
}

/* Subdivide into CCG. */

static void subdiv_ccg_settings_init(SubdivToCCGSettings *settings,
                                     const SubsurfModifierData *smd,
                                     const ModifierEvalContext *ctx)
{
  const int level = subdiv_levels_for_modifier_get(smd, ctx);
  settings->resolution = (1 << level) + 1;
  settings->need_normal = true;
  settings->need_mask = false;
}

static SubsurfRuntimeData *subsurf_ensure_runtime(SubsurfModifierData *smd)
{
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)smd->modifier.runtime;
  if (runtime_data == NULL) {
    runtime_data = MEM_callocN(sizeof(*runtime_data), "subsurf runtime");
    smd->modifier.runtime = runtime_data;
  }
  return runtime_data;
}

static void panel_draw(const bContext *C, Panel *panel) {

    uiLayout *layout = panel->layout;

    PointerRNA ob_ptr;
    PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

    uiLayoutSetPropSep(layout, true);

    uiItemR(layout, ptr, "gpusubsurf_iterations", 0, IFACE_("Levels Viewport"), ICON_NONE);
    uiItemR(layout, ptr, "gpusubsurf_iterationsrender", 0, IFACE_("Levels Render"), ICON_NONE);
    uiItemR(layout, ptr, "subdiv_type", 0, IFACE_("Simple Subdivision"), ICON_NONE);
    uiItemR(layout, ptr, "boundary_smooth", 0, IFACE_("Smooth Boundaries"), ICON_NONE);
    uiItemR(layout, ptr, "gpusubsurf_mergebydistance", 0, IFACE_("Merge By Distance (Using CUDA)"), ICON_NONE);

    modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type) {modifier_panel_register(region_type, eModifierType_GPUSubsurf, panel_draw);}

static bool dependsOnNormals(struct ModifierData *md) {return false;}

static void copyData(const ModifierData *md, ModifierData *target, const int flag) {BKE_modifier_copydata_generic(md, target, flag);}

static void deformMatrices(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh, float (*vertex_cos)[3], float (*deform_matrices)[3][3], int num_verts) {}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh) {

    GPUSubsurfData *gsd = (GPUSubsurfData *)md;

    if (gsd->gpusubsurf_iterations == 0) return mesh;

    SubsurfModifierData *smd = (SubsurfModifierData *)md;
    SubdivSettings subdiv_settings;

    smd->levels = gsd->gpusubsurf_iterations;
    smd->renderLevels = gsd->gpusubsurf_iterationsrender;
    smd->subdivType = (gsd->subdiv_type == SUBSURF_TYPE_SIMPLE); // add an option for users to toggle between simple and catmull clark
    smd->flags = eSubsurfModifierFlag_UseCrease | eSubsurfModifierFlag_UseRecursiveSubdivision;
    smd->uv_smooth = 0; // not sure what this does, ill modify it later
    smd->quality = 0; // also not sure what this does
    smd->boundary_smooth = (gsd->boundary_smooth == SUBSURF_BOUNDARY_SMOOTH_PRESERVE_CORNERS); // add an option for users to toggle this between preserve corners and dont preserve corners

    Mesh *result = mesh;

    subdiv_settings_init(&subdiv_settings, smd, ctx);
    if (subdiv_settings.level == 0) {
        return result;
    }
    SubsurfRuntimeData *runtime_data = subsurf_ensure_runtime(smd);
    Subdiv *subdiv = subdiv_descriptor_ensure(smd, &subdiv_settings, mesh);
    if (subdiv == NULL) {
        /* Happens on bad topology, but also on empty input mesh. */
        return result;
    }
    const bool use_clnors = (smd->flags & eSubsurfModifierFlag_UseCustomNormals) &&
                            (mesh->flag & ME_AUTOSMOOTH) &&
                            CustomData_has_layer(&mesh->ldata, CD_CUSTOMLOOPNORMAL);
    if (use_clnors) {
        /* If custom normals are present and the option is turned on calculate the split
        * normals and clear flag so the normals get interpolated to the result mesh. */
        BKE_mesh_calc_normals_split(mesh);
        CustomData_clear_layer_flag(&mesh->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
    }

    result = subdiv_as_mesh_cuda(smd, ctx, mesh, subdiv);

    if (use_clnors) {
        float(*lnors)[3] = CustomData_get_layer(&result->ldata, CD_NORMAL);
        BLI_assert(lnors != NULL);
        BKE_mesh_set_custom_normals(result, lnors);
        CustomData_set_layer_flag(&mesh->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
        CustomData_set_layer_flag(&result->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
    }
    // BKE_subdiv_stats_print(&subdiv->stats);
    if (subdiv != runtime_data->subdiv) {
        BKE_subdiv_free_cuda(subdiv);
    }

    return result;
}

ModifierTypeInfo modifierType_GPUSubsurf = {
    /* name */ "Subdivision Surface (CUDA)",
    /* structName */ "GPUSubsurfData",
    /* structSize */ sizeof(GPUSubsurfData),
    /* srna */ &RNA_GPUSubsurf,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /* icon */ ICON_MOD_SUBSURF,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ deformMatrices,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyGeometrySet */ NULL,

    /* initData */ NULL,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};