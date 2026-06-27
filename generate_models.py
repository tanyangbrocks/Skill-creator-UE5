"""
generate_models.py — Blender 5.1 headless 3D item model generator

Run:
    & "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" --background --python "C:\SkillCreatorUE5\generate_models.py"

Output: C:\SkillCreatorUE5\asset\3d\generated\SM_*.fbx
Next:   run import_assets.py in UE5 Editor Python Console to import all files.

Sizes: 1 Blender unit = 1 metre → 100 UE5 cm (via apply_unit_scale=True in FBX export).
Already-exists items are skipped; delete the file to regenerate.
"""
import bpy, bmesh, os, math

OUTPUT_DIR = r"C:\SkillCreatorUE5\asset\3d\generated"
os.makedirs(OUTPUT_DIR, exist_ok=True)

SKIP = {'WeaponWoodSword'}   # real asset already wired (SM_DemonSword)


# ── utilities ────────────────────────────────────────────────────────────────

def clear():
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()


def act():
    return bpy.context.view_layer.objects.active


def tf(obj=None):
    o = obj or act()
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return o


def bev(obj, w=0.003, seg=2):
    bpy.context.view_layer.objects.active = obj
    m = obj.modifiers.new('B', 'BEVEL')
    m.width = w; m.segments = seg
    bpy.ops.object.modifier_apply(modifier='B')


def export(name):
    path = os.path.join(OUTPUT_DIR, f"SM_{name}.fbx")
    bpy.ops.object.select_all(action='SELECT')
    try:
        bpy.ops.export_scene.fbx(
            filepath=path, use_selection=True,
            apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
            axis_forward='-Z', axis_up='Y',
            use_mesh_modifiers=True, mesh_smooth_type='NORMALS_ONLY',
            add_leaf_bones=False, bake_anim=False,
        )
    except Exception as e:
        # Blender 5.x fallback: OBJ
        path = os.path.join(OUTPUT_DIR, f"SM_{name}.obj")
        bpy.ops.wm.obj_export(
            filepath=path, export_selected_objects=True,
            apply_modifiers=True, forward_axis='NEGATIVE_Z', up_axis='Y',
        )
    print(f"  ✓  SM_{name}")


def box(sx, sy, sz, x=0, y=0, z=0, n='Box'):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    o = act(); o.name = n; o.scale = (sx, sy, sz)
    return tf(o)


def cyl(r, h, v=8, x=0, y=0, z=0, n='Cyl'):
    bpy.ops.mesh.primitive_cylinder_add(vertices=v, radius=r, depth=h,
                                         location=(x, y, z))
    o = act(); o.name = n
    return tf(o)


def sph(r, s=8, rn=6, x=0, y=0, z=0, n='Sph'):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=s, ring_count=rn,
                                          radius=r, location=(x, y, z))
    o = act(); o.name = n
    return tf(o)


def cone(r1, r2, h, v=6, x=0, y=0, z=0, n='Cone'):
    bpy.ops.mesh.primitive_cone_add(vertices=v, radius1=r1, radius2=r2,
                                     depth=h, location=(x, y, z))
    o = act(); o.name = n
    return tf(o)


def join_name(name):
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.join()
    o = act(); o.name = name
    return o


def tapered_blade(bw, bl, bt):
    """Hexahedron blade: wide at base, narrows to tip. Y = length axis."""
    mesh = bpy.data.meshes.new('BM')
    obj  = bpy.data.objects.new('Blade', mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bm = bmesh.new()
    hw = bw*0.5; ht = bt*0.5; tw = hw*0.10; tt = ht*0.35
    b = [bm.verts.new(v) for v in [
        (-hw,0,-ht),( hw,0,-ht),( hw,0, ht),(-hw,0, ht),   # base
        (-tw,bl,-tt),( tw,bl,-tt),( tw,bl, tt),(-tw,bl, tt), # tip
    ]]
    for face in [[3,2,1,0],[4,5,6,7],[0,1,5,4],[2,3,7,6],[3,0,4,7],[1,2,6,5]]:
        bm.faces.new([b[i] for i in face])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh); bm.free()
    return tf(obj)


# ── generators ───────────────────────────────────────────────────────────────

def gen_block(n):
    clear()
    o = box(0.10, 0.10, 0.10, n=n)    # 10cm cube (visible dropped tile)
    bev(o, w=0.006)
    export(n)


def gen_ore(n):
    clear()
    o = sph(0.05, s=8, rn=6, n=n)
    o.scale = (1.1, 1.0, 0.85); tf(o)
    export(n)


def gen_fragment(n):
    clear()
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=0.025)
    o = act(); o.name = n
    o.scale = (1.3, 0.8, 0.9); tf(o)
    export(n)


def gen_sword(n, blade_len=0.55, blade_w=0.04, guard_w=0.13, handle_len=0.13):
    clear()
    tapered_blade(blade_w, blade_len, 0.007)
    box(guard_w*0.5, 0.014, 0.014, y=0, n='Guard')
    cyl(0.013, handle_len, v=6, y=-handle_len*0.5, n='Handle')
    sph(0.019, s=6, rn=4, y=-handle_len, n='Pommel')
    o = join_name(n)
    o.location.y = -(blade_len - handle_len)*0.5; tf(o)
    export(n)


def gen_axe(n, hl=0.45):
    clear()
    cyl(0.013, hl, v=8, n='Handle')
    box(0.085, 0.012, 0.075, x=0.05,  y=hl*0.5-0.07, n='Head')
    box(0.025, 0.009, 0.045, x=0.107, y=hl*0.5-0.04, n='Tip')
    cyl(0.022, 0.038, v=8, y=hl*0.5, n='Collar')
    join_name(n); export(n)


def gen_pick(n, hl=0.45):
    clear()
    cyl(0.012, hl, v=8, n='Handle')
    for sign in (1, -1):
        p = cyl(0.008, 0.13, v=6, x=sign*0.035, y=hl*0.5+0.04, n=f'P{sign}')
        p.rotation_euler.z = math.radians(sign*30); tf(p)
    cyl(0.022, 0.04, v=8, y=hl*0.5, n='Collar')
    join_name(n); export(n)


def gen_shovel(n, hl=0.55):
    clear()
    cyl(0.011, hl, v=8, n='Handle')
    box(0.08, 0.006, 0.13, y=hl*0.5+0.065, n='Blade')
    join_name(n); export(n)


def gen_helmet(n):
    clear()
    o = sph(0.07, s=10, rn=6, n=n)
    o.scale = (1.0, 1.0, 0.55); tf(o)
    export(n)


def gen_armor(n):
    clear()
    o = cyl(0.10, 0.12, v=12, n=n)
    o.scale = (1.0, 0.40, 1.0); tf(o)
    export(n)


def gen_pants(n):
    clear()
    cyl(0.035, 0.18, v=8, x=-0.04, n='L')
    cyl(0.035, 0.18, v=8, x= 0.04, n='R')
    box(0.10, 0.07, 0.06, z=0.12, n='Waist')
    join_name(n); export(n)


def gen_boots(n):
    clear()
    o = box(0.05, 0.10, 0.06, n=n)
    bev(o, w=0.012); export(n)


def gen_amulet(n):
    clear()
    cyl(0.030, 0.005, v=16, n='Disk')
    bpy.ops.mesh.primitive_torus_add(major_radius=0.008, minor_radius=0.003,
                                      location=(0, 0, 0.020))
    act().name = 'Ring'
    join_name(n); export(n)


def gen_plank(n):
    clear()
    o = box(0.12, 0.04, 0.008, n=n)
    bev(o, w=0.003); export(n)


def gen_stick(n):
    clear()
    cyl(0.007, 0.18, v=6, n=n); export(n)


def gen_chest(n):
    clear()
    box(0.12, 0.09, 0.070,  z=0.000, n='Base')
    box(0.12, 0.09, 0.025,  z=0.048, n='Lid')
    box(0.121, 0.004, 0.006, z=0.072, n='Lock')
    join_name(n); export(n)


def gen_workbench(n):
    clear()
    box(0.15, 0.11, 0.012, z=0.095, n='Top')
    for xi, yi in [(-0.065,-0.045),(0.065,-0.045),(-0.065,0.045),(0.065,0.045)]:
        box(0.010, 0.010, 0.090, x=xi, y=yi, z=0.045, n='Leg')
    join_name(n); export(n)


def gen_berry(n):
    clear()
    sph(0.025, s=8, rn=6, n=n); export(n)


def gen_oaklog(n):
    clear()
    cyl(0.055, 0.15, v=10, n=n); export(n)


def gen_sapling(n):
    clear()
    cyl(0.006, 0.08, v=6, n='Trunk')
    sph(0.025, s=6, rn=4, z=0.07, n='Crown')
    join_name(n); export(n)


def gen_fallenleaf(n):
    clear()
    o = cyl(0.04, 0.002, v=8, n=n)
    o.scale = (1.0, 0.7, 1.0); tf(o); export(n)


def gen_oakfruit(n):
    clear()
    sph(0.022, s=8, rn=6, n='Body')
    cone(0.024, 0.005, 0.01, v=8, z=0.02, n='Cap')
    join_name(n); export(n)


def gen_weed(n):
    clear()
    for i, (xo, ang) in enumerate([(0,0),(0.02,20),(-0.018,-15)]):
        c = cone(0.005, 0.001, 0.08, v=4, x=xo, n=f'B{i}')
        c.rotation_euler.z = math.radians(ang); tf(c)
    join_name(n); export(n)


# ── dispatch ─────────────────────────────────────────────────────────────────

ITEMS = {
    # Blocks
    'BlockDirt': gen_block, 'BlockStone': gen_block,
    'BlockWood': gen_block, 'BlockSand':  gen_block, 'BlockAsh': gen_block,
    # Tools
    'ToolBasicPick': gen_pick,   'ToolBasicAxe':   gen_axe,
    'ToolIronPick':  gen_pick,   'ToolWoodShovel': gen_shovel,
    'ToolStonePick': gen_pick,
    # Weapons
    'EquipBasicSword':  lambda n: gen_sword(n, 0.65),
    'WeaponStoneSword': lambda n: gen_sword(n, 0.70, blade_w=0.045),
    # Equip
    'EquipLeatherArmor':  gen_armor,   'EquipLeatherHelmet': gen_helmet,
    'EquipLeatherPants':  gen_pants,   'EquipLeatherBoots':  gen_boots,
    'EquipAmulet':        gen_amulet,
    # Ores
    'OreCoal': gen_ore, 'OreCopperRaw': gen_ore, 'OreIronRaw': gen_ore,
    'OreGoldRaw': gen_ore, 'OreMagicCrystal': gen_ore,
    # Fragments
    'FragmentDirt': gen_fragment, 'FragmentStone': gen_fragment,
    'FragmentSand': gen_fragment, 'FragmentWood':  gen_fragment,
    'FragmentAsh':  gen_fragment, 'FragmentCoal':  gen_fragment,
    'FragmentCopper': gen_fragment, 'FragmentIron': gen_fragment,
    'FragmentMagicCrystal': gen_fragment,
    # Materials
    'MaterialPlank': gen_plank, 'MaterialStick': gen_stick,
    # Placeables
    'PlaceableWoodChest': gen_chest, 'PlaceableWoodWorkbench': gen_workbench,
    # Consumables
    'ConsumableBerry': gen_berry, 'OakFruit': gen_oakfruit,
    # Tree products
    'OakLog': gen_oaklog, 'FallenLeaf': gen_fallenleaf, 'OakSapling': gen_sapling,
    # Plants
    'Weed': gen_weed,
}

if __name__ == '__main__':
    print(f"\n{'='*55}")
    print(f"SkillCreator item generator — {len(ITEMS)} items")
    print(f"Output: {OUTPUT_DIR}")
    print(f"{'='*55}\n")

    done, failed = [], []
    for name, fn in ITEMS.items():
        if name in SKIP:
            print(f"  ─  {name}  (real asset — skip)"); continue
        out = os.path.join(OUTPUT_DIR, f"SM_{name}.fbx")
        if os.path.exists(out):
            print(f"  ○  {name}  (exists)"); done.append(name); continue
        try:
            fn(name); done.append(name)
        except Exception as e:
            import traceback
            print(f"  ✗  {name}: {e}")
            traceback.print_exc()
            failed.append(name)

    print(f"\n{'='*55}")
    print(f"Done: {len(done)}  |  Failed: {failed or 'none'}")
    print("Next: run import_assets.py in UE5 Editor Python Console")
    print(f"{'='*55}")
