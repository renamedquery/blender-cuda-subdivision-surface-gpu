// adapted from the instructions on https://blog.exppad.com/article/writing-blender-modifier

// created by katznboyz, but adapted from the instructions above, and integrated into code by the Blender Foundation

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
#include "BKE_subsurf.h"

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

static void panel_draw(const bContext *C, Panel *panel) {

    uiLayout *layout = panel->layout;

    PointerRNA ob_ptr;
    PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

    uiLayoutSetPropSep(layout, true);

    uiLayout *col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "gpusubsurf_iterations", 0, IFACE_("Levels Viewport"), ICON_NONE);
    //uiLayoutSetActive(col, RNA_boolean_get(&ptr, "gpusubsurf_mergebydistance"));

    modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type) {
    
    printf("[DEBUG] [GPUSubsurf::panelRegister] STARTING\n");
    modifier_panel_register(region_type, eModifierType_GPUSubsurf, panel_draw);
}

static bool dependsOnNormals(struct ModifierData *md) {

    printf("[DEBUG] [GPUSubsurf::dependsOnNormals] PASS\n");
    return false;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag) {

    printf("[DEBUG] [GPUSubsurf::copyData] COPYING MODIFIER DATA\n");
    BKE_modifier_copydata_generic(md, target, flag);
}

static void deformMatrices(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh, float (*vertex_cos)[3], float (*deform_matrices)[3][3], int num_verts) {

    printf("[DEBUG] [GPUSubsurf::deformMatrices] DOING NOTHING\n");
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh) {

    printf("[DEBUG] [GPUSubsurf::modifyMesh] RETURNING MESH\n");
    return mesh;
}

/*static Mesh *gpusubsurf_applyModifier(struct ModifierData *md, const struct ModifierEvalContext *ctx, struct Mesh *mesh) {
    
    printf("[DEBUG] [GPUSubsurf::gpusubsurf_applyModifier] PASS\n");

    // function will be empty for now
    return mesh;
}*/

ModifierTypeInfo modifierType_GPUSubsurf = {
    /* name */ "GPU Subdivision Surface",
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