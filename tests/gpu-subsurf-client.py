import bpy

bl_info = {
    'name':'GPU Subsurf Client (Development Version)',
    'blender':(3, 0, 0),
    'category':'Object'
}

class GPUSubsurfClientUseMenu(bpy.types.Operator):

    bl_idname = "addon_gpusubsurf.clientmenu_use"
    bl_label = "Subdivide Mesh Using CUDA"
    bl_options = {
        'REGISTER',
        'UNDO'
    }

    def execute(self, context) -> set:

        print('[gpu-subsurf-client.py] [GPUSubsurfClientUseMenu.execute] DONE')

        return {'FINISHED'}

class GPUSubsurfClientStartMenu(bpy.types.Operator):

    bl_idname = "addon_gpusubsurf.clientmenu_start"
    bl_label = "Start CUDA Subsurf Server"
    bl_options = {
        'REGISTER',
        'UNDO'
    }

    def execute(self, context) -> set:

        print('[gpu-subsurf-client.py] [GPUSubsurfClientStartMenu.execute] DONE')

        return {'FINISHED'}

class GPUSubsurfClientStopMenu(bpy.types.Operator):

    bl_idname = "addon_gpusubsurf.clientmenu_stop"
    bl_label = "Stop CUDA Subsurf Server"
    bl_options = {
        'REGISTER',
        'UNDO'
    }

    def execute(self, context) -> set:

        print('[gpu-subsurf-client.py] [GPUSubsurfClientStopMenu.execute] DONE')

        return {'FINISHED'}

def addFunctionsToMenu(self, context) -> None:

    self.layout.operator(GPUSubsurfClientUseMenu.bl_idname)
    self.layout.operator(GPUSubsurfClientStartMenu.bl_idname)
    self.layout.operator(GPUSubsurfClientStopMenu.bl_idname)

def register() -> None:

    bpy.utils.register_class(GPUSubsurfClientUseMenu)
    bpy.utils.register_class(GPUSubsurfClientStartMenu)
    bpy.utils.register_class(GPUSubsurfClientStopMenu)
    bpy.types.VIEW3D_MT_object.append(addFunctionsToMenu)

    print('[gpu-subsurf-client.py] [register] DONE')

def unregister() -> None:
    
    print('[gpu-subsurf-client.py] [unregister] DONE')