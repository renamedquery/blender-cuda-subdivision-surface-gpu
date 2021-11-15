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

#include "BKE_action.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "RNA_access.h"

ModifierTypeInfo modifierType_GPUSubsurf = {
    /* name */ "GPU Subdivision Surface",
    /* structName */ "GPUSubsurfData",
    /* structSize */ sizeof(GPUSubsurfData),
    /* srna */ NULL,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeType_None,
    /* icon */ NULL,
    /* copyData */ NULL,

    /* deformVerts */ NULL,
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
    /* panelRegister */ NULL,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};


static Mesh *gpusubsurf_applyModifier(struct ModifierData *md, const struct ModifierEvalContext *ctx, struct Mesh *mesh) {
    
    // function will be empty for now
    return mesh;
}