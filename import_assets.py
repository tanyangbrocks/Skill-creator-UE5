"""
import_assets.py — 一次性資產匯入腳本
在 UE5 Editor 的 Output Log 中執行：
    Python Script: C:\SkillCreatorUE5\import_assets.py

或在 Python Console 貼上：
    exec(open(r'C:\SkillCreatorUE5\import_assets.py').read())

匯入結果：
    /Game/Characters/Player/SK_Player          ← 角色 SkeletalMesh
    /Game/Characters/Player/SK_Player_Skeleton ← 自動產生的骨架
    /Game/Characters/Player/Anim/AS_Walk       ← Slow Run
    /Game/Characters/Player/Anim/AS_Run        ← Running
    /Game/Characters/Player/Anim/AS_FastRun    ← Fast Run
    /Game/Characters/Player/Anim/AS_Punch      ← Punching
    /Game/Weapons/DemonSword/SM_DemonSword     ← Sword_A Static Mesh
"""

import unreal

BASE = r'C:\SkillCreatorUE5\asset\3d'
tools = unreal.AssetToolsHelpers.get_asset_tools()

# ─────────────────────────────────────────────────────────────────────────
# Step 1：角色骨架 Mesh（Ch09_nonPBR.fbx → SK_Player）
# ─────────────────────────────────────────────────────────────────────────
print('[1/3] Importing character skeletal mesh...')

mesh_task = unreal.AssetImportTask()
mesh_task.set_editor_property('filename',         BASE + r'\characters\Ch09_nonPBR.fbx')
mesh_task.set_editor_property('destination_path', '/Game/Characters/Player')
mesh_task.set_editor_property('destination_name', 'SK_Player')
mesh_task.set_editor_property('replace_existing', True)
mesh_task.set_editor_property('automated',        True)
mesh_task.set_editor_property('save',             True)

mesh_opts = unreal.FbxImportUI()
mesh_opts.set_editor_property('import_mesh',          True)
mesh_opts.set_editor_property('import_animations',    False)
mesh_opts.set_editor_property('import_as_skeletal',   True)
mesh_opts.set_editor_property('create_physics_asset', False)
mesh_opts.set_editor_property('import_materials',     True)
mesh_opts.set_editor_property('import_textures',      True)
mesh_task.set_editor_property('options', mesh_opts)

tools.import_asset_tasks([mesh_task])
print('  ✓ SK_Player done')

# ─────────────────────────────────────────────────────────────────────────
# Step 2：動作序列（需先取得 Step 1 產生的骨架）
# ─────────────────────────────────────────────────────────────────────────
print('[2/3] Importing animation sequences...')

# 骨架預設命名規則：SK_Player → SK_Player_Skeleton
# 若匯入後此路徑不存在，請在 Content Browser 找到實際骨架名稱後修改此行
SKELETON_PATH = '/Game/Characters/Player/SK_Player_Skeleton'
skeleton = unreal.load_asset(SKELETON_PATH)

if not skeleton:
    print('[ERROR] Skeleton not found at: ' + SKELETON_PATH)
    print('        請在 Content Browser 找到正確骨架路徑後修改 SKELETON_PATH 再重新執行。')
else:
    anim_defs = [
        (BASE + r'\action\Slow Run.fbx', 'AS_Walk'),
        (BASE + r'\action\Running.fbx',  'AS_Run'),
        (BASE + r'\action\Fast Run.fbx', 'AS_FastRun'),
        (BASE + r'\action\Punching.fbx', 'AS_Punch'),
    ]
    anim_tasks = []
    for src_path, dest_name in anim_defs:
        t = unreal.AssetImportTask()
        t.set_editor_property('filename',         src_path)
        t.set_editor_property('destination_path', '/Game/Characters/Player/Anim')
        t.set_editor_property('destination_name', dest_name)
        t.set_editor_property('replace_existing', True)
        t.set_editor_property('automated',        True)
        t.set_editor_property('save',             True)

        o = unreal.FbxImportUI()
        # Legacy FBX Importer（Interchange 已停用）直接支援此組合：
        # import_as_skeletal=True + import_mesh=False + import_animations=True + skeleton
        o.set_editor_property('import_mesh',        False)
        o.set_editor_property('import_as_skeletal', True)
        o.set_editor_property('import_animations',  True)
        o.set_editor_property('skeleton',           skeleton)

        anim_data = unreal.FbxAnimSequenceImportData()
        anim_data.set_editor_property('remove_redundant_keys', True)
        o.set_editor_property('anim_sequence_import_data', anim_data)

        t.set_editor_property('options', o)
        anim_tasks.append(t)

    tools.import_asset_tasks(anim_tasks)
    print('  ✓ AS_Walk / AS_Run / AS_FastRun / AS_Punch done')

# ─────────────────────────────────────────────────────────────────────────
# Step 3：惡魔劍 Static Mesh（Sword_A.fbx → SM_DemonSword）
# ─────────────────────────────────────────────────────────────────────────
print('[3/3] Importing demon sword static mesh...')

sword_task = unreal.AssetImportTask()
sword_task.set_editor_property('filename',
    BASE + r'\real_object\demon_sword\Demon Sword\Sword_A.fbx')
sword_task.set_editor_property('destination_path', '/Game/Weapons/DemonSword')
sword_task.set_editor_property('destination_name', 'SM_DemonSword')
sword_task.set_editor_property('replace_existing', True)
sword_task.set_editor_property('automated',        True)
sword_task.set_editor_property('save',             True)

sword_opts = unreal.FbxImportUI()
sword_opts.set_editor_property('import_mesh',        True)
sword_opts.set_editor_property('import_as_skeletal', False)
sword_opts.set_editor_property('import_animations',  False)
sword_opts.set_editor_property('import_materials',   True)
sword_opts.set_editor_property('import_textures',    True)
sword_task.set_editor_property('options', sword_opts)

tools.import_asset_tasks([sword_task])
print('  ✓ SM_DemonSword done')

# ─────────────────────────────────────────────────────────────────────────
# Step 4：匯入 generate_models.py 產生的 item 模型（/Game/Items/SM_*）
# ─────────────────────────────────────────────────────────────────────────
print('[4/4] Importing generated item models from asset/3d/generated/ ...')

import os as _os

gen_dir = BASE + r'\generated'
if not _os.path.isdir(gen_dir):
    print('  (generated/ folder not found — run generate_models.py first)')
else:
    # 同一個 stem 優先取 .fbx，沒有才取 .obj
    by_stem = {}
    for fname in _os.listdir(gen_dir):
        if not fname.startswith('SM_'):
            continue
        ext  = fname.rsplit('.', 1)[-1].lower()
        if ext not in ('fbx', 'obj'):
            continue
        stem = fname[3:].rsplit('.', 1)[0]      # 'SM_BlockDirt.fbx' → 'BlockDirt'
        if stem not in by_stem or ext == 'fbx':  # FBX 優先
            by_stem[stem] = fname

    gen_tasks = []
    for stem, fname in sorted(by_stem.items()):
        ext = fname.rsplit('.', 1)[-1].lower()
        t = unreal.AssetImportTask()
        t.set_editor_property('filename',         _os.path.join(gen_dir, fname))
        t.set_editor_property('destination_path', '/Game/Items')
        t.set_editor_property('destination_name', f'SM_{stem}')
        t.set_editor_property('replace_existing', True)
        t.set_editor_property('automated',        True)
        t.set_editor_property('save',             True)

        if ext == 'fbx':
            opts = unreal.FbxImportUI()
            opts.set_editor_property('import_mesh',        True)
            opts.set_editor_property('import_as_skeletal', False)
            opts.set_editor_property('import_animations',  False)
            opts.set_editor_property('import_materials',   False)
            opts.set_editor_property('import_textures',    False)
            t.set_editor_property('options', opts)

        gen_tasks.append(t)

    if gen_tasks:
        tools.import_asset_tasks(gen_tasks)
        print(f'  ✓ {len(gen_tasks)} models imported to /Game/Items/')
    else:
        print('  (no SM_*.fbx/obj files found — run generate_models.py first)')

print()
print('=== 匯入完成 ===')
print('下一步：')
print('  1. 在 Content Browser 確認各資料夾都有內容')
print('  2. 若骨架名稱不是 SK_Player_Skeleton，修改 SKELETON_PATH 後重跑 Step 2')
print('  3. 關 Editor → Rebuild → 重開 Editor → 執行 Standalone')
print('  4. 角色應出現 Ch09 外觀；木劍掉落物顯示惡魔劍；其他物品顯示生成模型')
