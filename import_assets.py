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
        o.set_editor_property('import_mesh',       False)
        o.set_editor_property('import_animations', True)
        o.set_editor_property('import_as_skeletal',True)
        o.set_editor_property('skeleton',          skeleton)
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

print()
print('=== 匯入完成 ===')
print('下一步：')
print('  1. 在 Content Browser 確認三個資料夾都有內容')
print('  2. 若骨架名稱不是 SK_Player_Skeleton，修改腳本中的 SKELETON_PATH 再跑一次 Step 2')
print('  3. 關 Editor → Rebuild → 重開 Editor → 執行 Standalone')
print('  4. 角色應出現 Ch09 外觀，木劍掉落物顯示惡魔劍模型')
