import bpy
import bmesh
import os

# Make sure we are in Edit Mode
if bpy.context.object.mode != 'EDIT':
    raise Exception("Must be in Edit Mode")

# Get the active object and its BMesh
obj = bpy.context.edit_object
bm = bmesh.from_edit_mesh(obj.data)

# Ensure face and vertex data is up to date
bm.faces.ensure_lookup_table()
bm.verts.ensure_lookup_table()

# Check if there are any faces
if len(bm.faces) == 0:
    raise Exception("No faces found in the mesh")

# Get the first face
face = bm.faces[0]

home_directory = os.path.expanduser("~")
output_path = os.path.join(home_directory, "jeu_desprit/collision/collision_loop.h")

# Open the file for writing
with open(output_path, "w") as file:
    file.write("{\n")

    # Loop through the face’s vertices via the face loops
    for loop in face.loops:
        vert = loop.vert
        file.write(f"    {{ {vert.co.x}, {vert.co.y}, {vert.co.z} }},\n")

    file.write("}\n")

print("Data written to 'collision_loop.h'")
