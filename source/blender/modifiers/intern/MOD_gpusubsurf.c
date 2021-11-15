// adapted from the instructions on https://blog.exppad.com/article/writing-blender-modifier

// created by katznboyz, but adapted from the instructions above, and integrated into code by the Blender Foundation

/** \file
 * \ingroup modifiers
 */

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BLT_translation.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

static void panel_draw(const bContext *C, Panel *panel) {

    uiLayout *layout = panel->layout;

    PointerRNA ob_ptr;
    PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

    uiLayoutSetPropSep(layout, true);

    uiLayout *col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "gpusubsurf_iterations", 0, IFACE_("Levels Viewport"), ICON_NONE);

    modifier_panel_end(layout, ptr);
}

static void advanced_panel_draw(const bContext *C, Panel *panel) {

    uiLayout *layout = panel->layout;
    
    PointerRNA ob_ptr;
    PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

    uiLayoutSetPropSep(layout, true);

    uiLayout *col = uiLayoutColumn(layout, true);
    uiLayoutSetActive(col, RNA_boolean_get(ptr, "gpusubsurf_mergebydistance"));
    uiItemR(col, ptr, "merge_by_distance", 0, NULL, ICON_NONE);
}

static void initData(ModifierData *md) {

    GPUSubsurfData *gsd = (GPUSubsurfData *)md;

    BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gsd, modifier));

    MEMCPY_STRUCT_AFTER(gsd, DNA_struct_default_get(GPUSubsurfData), modifier);
}

static void panelRegister(ARegionType *region_type) {

    PanelType *panel_type = modifier_panel_register(region_type, eModifierType_GPUSubsurf, panel_draw);
    modifier_subpanel_register(region_type, "advanced", "Advanced", NULL, advanced_panel_draw, panel_type);
}

static Mesh *gpusubsurf_applyModifier(struct ModifierData *md, const struct ModifierEvalContext *ctx, struct Mesh *mesh) {
    
    // function will be empty for now
    return mesh;
}

ModifierTypeInfo modifierType_GPUSubsurf = {
    /* name */ "GPU Subdivision Surface",
    /* structName */ "GPUSubsurfData",
    /* structSize */ sizeof(GPUSubsurfData),
    /* srna */ &RNA_GPUSubsurf,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /* icon */ NULL,
    /* copyData */ NULL,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyGeometrySet */ NULL,

    /* initData */ initData,
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