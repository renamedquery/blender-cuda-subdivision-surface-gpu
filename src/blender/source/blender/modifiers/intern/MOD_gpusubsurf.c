// adapted from the instructions on https://blog.exppad.com/article/writing-blender-modifier

#include "BKE_modifier.h"
#include "DNA_mesh_types.h"

ModifierTypeInfo modifierType_GPUSubsurf = {
    /* name */ "GPU Subdivision Surface",
    /* structName */ "GPUSubsurfData",
    /* structSize */ sizeof(GPUSubsurfData),
    /* type */ eModifierTypeType_None,
    /* flags */ eModifierTypeFlag_AcceptsMesh,

    /* copyData */ NULL,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ NULL,

    /* initData */ NULL,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};

static Mesh *gpusubsurf_applyModifier(struct ModifierData *md, const struct ModifierEvalContext *ctx, struct Mesh *mesh) {
    
    // function will be empty for now
    return mesh;
}