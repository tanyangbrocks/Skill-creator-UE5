"""
generate_placeholders.py — 批次生成體積佔位 Mesh
在 UE5 Editor Output Log 執行：
    Python Script: C:\SkillCreatorUE5\generate_placeholders.py

效果：
    /Game/Items/SM_BlockDirt        ← 泥土   (Cube)
    /Game/Items/SM_ToolBasicPick    ← 木鎬   (Cylinder)
    /Game/Items/SM_FragmentStone    ← 石塊碎片 (Sphere)
    ... 依下方規則自動生成

規則：
    - 已存在的資產跳過（保護手動改過的精緻版本）
    - SKIP_ITEMS 清單裡的項目跳過（有真實 FBX 資產，由 FItemData::MeshPath 指定）
    - 替換：把真實 FBX 匯入到同路徑加 replace_existing=True 即覆蓋，C++ 一行不用改

新增 EItemId 後：
    1. 在下方 ALL_ITEMS 清單補上
    2. 若形狀符合前綴規則則自動對應，否則在 SHAPE_OVERRIDE 補例外
    3. 重新執行此腳本（已存在者跳過，只建新的）
"""

import unreal

# ─── EItemId 完整清單（同步 Source/SkillCreatorCore/Public/ItemId.h）────────────
ALL_ITEMS = [
    # 方塊（可放置地圖 tile）
    'BlockDirt', 'BlockStone', 'BlockWood', 'BlockSand', 'BlockAsh',
    # 工具
    'ToolBasicPick', 'ToolBasicAxe', 'ToolIronPick', 'ToolWoodShovel', 'ToolStonePick',
    # 裝備
    'EquipBasicSword', 'EquipLeatherArmor', 'EquipAmulet',
    'EquipLeatherHelmet', 'EquipLeatherPants', 'EquipLeatherBoots',
    # 礦石原材料
    'OreCoal', 'OreCopperRaw', 'OreIronRaw', 'OreGoldRaw', 'OreMagicCrystal',
    # 材質碎片
    'FragmentDirt', 'FragmentStone', 'FragmentSand', 'FragmentWood', 'FragmentAsh',
    'FragmentCoal', 'FragmentCopper', 'FragmentIron', 'FragmentMagicCrystal',
    # 武器
    'WeaponWoodSword', 'WeaponStoneSword',
    # 素材（加工原料）
    'MaterialPlank', 'MaterialStick',
    # 可放置物（不可塑形，spawn Actor）
    'PlaceableWoodChest', 'PlaceableWoodWorkbench',
    # 道具（消耗品）
    'ConsumableBerry',
    # 樹木產物（W-A）
    'OakLog', 'FallenLeaf', 'OakSapling', 'OakFruit',
    # 地表植物（W-G）
    'Weed',
]

# ─── 已有真實資產的項目，跳過（由 FItemData::MeshPath 指定真實路徑）──────────
SKIP_ITEMS = {
    'WeaponWoodSword',   # → /Game/Weapons/DemonSword/SM_DemonSword（惡魔劍）
}

# ─── 形狀對應（依名稱前綴自動判斷）────────────────────────────────────────────
# Cube=正方體, Cylinder=圓柱, Sphere=球體
SHAPE_BY_PREFIX = [
    ('Block',      'Cube'),
    ('Tool',       'Cylinder'),
    ('Weapon',     'Cylinder'),
    ('Equip',      'Cube'),
    ('Ore',        'Sphere'),
    ('Fragment',   'Sphere'),
    ('Material',   'Cube'),
    ('Placeable',  'Cube'),
    ('Consumable', 'Sphere'),
    ('OakLog',     'Cylinder'),
    ('OakSapling', 'Cone'),
    ('OakFruit',   'Sphere'),
    ('Oak',        'Cylinder'),
    ('Fallen',     'Cube'),
    ('Weed',       'Sphere'),
]
DEFAULT_SHAPE = 'Cube'

# ─── 個別例外（前綴規則對不到時用這裡手動覆蓋）────────────────────────────
SHAPE_OVERRIDE = {
    'MaterialStick':  'Cylinder',  # 木棍是圓柱，不是方塊
    'EquipAmulet':    'Sphere',    # 護符是球形吊墜
}

SHAPE_SOURCE = {
    'Cube':     '/Engine/BasicShapes/Cube',
    'Sphere':   '/Engine/BasicShapes/Sphere',
    'Cylinder': '/Engine/BasicShapes/Cylinder',
    'Cone':     '/Engine/BasicShapes/Cone',
}

DEST_FOLDER = '/Game/Items'

# ─────────────────────────────────────────────────────────────────────────────

def get_shape(name: str) -> str:
    if name in SHAPE_OVERRIDE:
        return SHAPE_OVERRIDE[name]
    for prefix, shape in SHAPE_BY_PREFIX:
        if name.startswith(prefix):
            return shape
    return DEFAULT_SHAPE

def main():
    lib   = unreal.EditorAssetLibrary
    total = len(ALL_ITEMS)

    skipped_real  = []   # 有真實資產
    skipped_exist = []   # 已存在（保留）
    created       = []
    failed        = []

    with unreal.ScopedSlowTask(total, 'Generating placeholder meshes...') as task:
        task.make_dialog(True)

        for name in ALL_ITEMS:
            task.enter_progress_frame(1, f'SM_{name}')
            if task.should_cancel():
                break

            # 有真實資產，跳過
            if name in SKIP_ITEMS:
                skipped_real.append(name)
                continue

            dest_path = f'{DEST_FOLDER}/SM_{name}'

            # 已存在，跳過（保護手動改過的版本）
            if lib.does_asset_exist(dest_path):
                skipped_exist.append(name)
                continue

            shape    = get_shape(name)
            src_path = SHAPE_SOURCE.get(shape, SHAPE_SOURCE['Cube'])

            # 複製 Engine BasicShape 到目標路徑
            success = lib.duplicate_asset(src_path, dest_path)
            if success:
                lib.save_asset(dest_path, only_if_is_dirty=False)
                created.append(f'{name} ({shape})')
                print(f'  ✓  SM_{name:<35} ← {shape}')
            else:
                failed.append(name)
                print(f'  ✗  SM_{name}  duplicate FAILED')

    # ── 結果摘要 ────────────────────────────────────────────────────────────
    print()
    print('=' * 60)
    print(f'新建：{len(created)} 個')
    if skipped_exist:
        print(f'已存在（保留）：{len(skipped_exist)} 個')
    if skipped_real:
        print(f'跳過（有真實資產）：{skipped_real}')
    if failed:
        print(f'失敗：{failed}')
    print('=' * 60)
    print()
    print('下一步：')
    print('  1. 在 MagicaVoxel 畫好模型後匯出 OBJ/FBX')
    print(f'     → import_assets.py 加一行匯入到 {DEST_FOLDER}/SM_{{ItemName}}')
    print('     → replace_existing=True 自動覆蓋佔位版本')
    print('  2. 有真實 FBX 資產的武器/道具：在 ItemRegistry.cpp 設定 D.MeshPath')
    print('     → APhysicalItemActor::Init() 自動優先使用')

main()
