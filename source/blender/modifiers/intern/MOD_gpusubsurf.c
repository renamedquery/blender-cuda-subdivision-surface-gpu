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
    uiLayoutSetActive(col, RNA_boolean_get(ptr, "gpusubsurf_mergebydistance"));

    modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type) {
    
    printf("[DEBUG] [GPUSubsurf::panelRegister] STARTING\n");

    PanelType *panel_type = modifier_panel_register(region_type, eModifierType_GPUSubsurf, panel_draw);

    printf("[DEBUG] [GPUSubsurf::panelRegister] FINISHED\n");
}

static void deformVerts(struct ModifierData *md, const struct ModifierEvalContext *ctx, struct Mesh *mesh, float *vertexCos[3], int numVerts) {
    
    printf("[DEBUG] [GPUSubsurf::deformVerts] PRINTF\n");

    // function will be empty for now
    return mesh;
}

static Mesh *gpusubsurf_applyModifier(struct ModifierData *md, const struct ModifierEvalContext *ctx, struct Mesh *mesh) {
    
    printf("[DEBUG] [GPUSubsurf::gpusubsurf_applyModifier] PRINTF\n");

    // function will be empty for now
    return mesh;
}

ModifierTypeInfo modifierType_GPUSubsurf = {
    /* name */ "GPU Subdivision Surface",
    /* structName */ "GPUSubsurfData",
    /* structSize */ sizeof(GPUSubsurfData),
    /* srna */ &RNA_GPUSubsurf,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeType_OnlyDeform,
    /* icon */ NULL,
    /* copyData */ NULL,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyGeometrySet */ NULL,

    /* initData */ NULL,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};