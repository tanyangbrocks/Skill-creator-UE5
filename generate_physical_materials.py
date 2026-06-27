"""
generate_physical_materials.py
PHYS-0：為所有 EMaterialType 建立對應的 UPhysicalMaterial .uasset
在 UE5 Editor Python Console 執行：
    py "C:\\SkillCreatorUE5\\generate_physical_materials.py"

Restitution 對齊 MaterialRegistry.cpp 中 FMaterialData.Restitution (P-16)：
  - Dirt_Dry   0.02、Grass 0.05、Ore_MagicCrystal 0.30（已明確設定）
  - 其餘材質使用合理物理默認值（石頭 0.30、金屬 0.40 等）
  - Liquid/Gas 材質 Restitution=0（由 buoyancy/tile-logic 處理，PM 僅佔位）
"""

import unreal

DEST_PATH = "/Game/PhysicalMaterials"

# (Restitution, Friction)
# 命名規則：PM_{EMaterialType 枚舉值名稱}（去掉前綴 Ore_）
MATERIALS = {
    # 名稱                        (Restitution, Friction)  說明
    "PM_Stone_Cobble":            (0.30,        0.70),   # 圓石：有一定彈性
    "PM_Dirt_Dry":                (0.02,        0.65),   # 乾泥：對齊 registry P-16
    "PM_Grass":                   (0.05,        0.70),   # 草地：對齊 registry P-16
    "PM_Sand":                    (0.05,        0.50),   # 沙：鬆散，略有緩衝
    "PM_Water":                   (0.00,        0.10),   # 液體：PM 佔位，浮力由 Tick 處理
    "PM_Lava":                    (0.00,        0.90),   # 液體：高黏性
    "PM_Wood":                    (0.20,        0.60),   # 木頭：有些彈性
    "PM_Leaves":                  (0.10,        0.65),   # 樹葉：軟緩衝
    "PM_Ore_Iron":                (0.40,        0.50),   # 鐵礦：金屬彈性
    "PM_Ore_Gold":                (0.35,        0.45),   # 金礦：略軟於鐵
    "PM_Fire":                    (0.00,        0.00),   # 氣體：無意義，佔位
    "PM_Steam":                   (0.00,        0.00),   # 氣體：佔位
    "PM_Ash":                     (0.05,        0.40),   # 灰燼：粉末，無彈性
    "PM_Ore_Coal":                (0.15,        0.55),   # 煤礦：略有彈性
    "PM_Ore_Copper":              (0.40,        0.50),   # 銅礦：金屬彈性
    "PM_Ore_MagicCrystal":        (0.30,        0.35),   # 魔法水晶：對齊 registry P-16
    "PM_Fixture":                 (0.20,        0.60),   # 固定物：通用
    "PM_FallenLeaf":              (0.10,        0.60),   # 落葉：軟緩衝
    "PM_Default":                 (0.20,        0.60),   # fallback：未映射材質
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal         = unreal.EditorAssetLibrary
factory     = unreal.PhysicalMaterialFactoryNew()

created = 0
skipped = 0
failed  = 0

print("=" * 50)
print("PHYS-0: Generate UPhysicalMaterial assets")
print(f"  Target: {DEST_PATH}")
print("=" * 50)

for name, (restitution, friction) in MATERIALS.items():
    full_path = f"{DEST_PATH}/{name}"

    if eal.does_asset_exist(full_path):
        print(f"  SKIP  {name}  (already exists)")
        skipped += 1
        continue

    pm = asset_tools.create_asset(
        asset_name    = name,
        package_path  = DEST_PATH,
        asset_class   = unreal.PhysicalMaterial,
        factory       = factory,
    )

    if pm is None:
        print(f"  FAIL  {name}")
        failed += 1
        continue

    pm.set_editor_property("restitution", restitution)
    pm.set_editor_property("friction",    friction)
    eal.save_asset(full_path, only_if_is_dirty=False)
    print(f"  OK    {name:<32}  R={restitution:.2f}  F={friction:.2f}")
    created += 1

print("=" * 50)
print(f"Result: {created} created, {skipped} skipped, {failed} failed")
print("Next: run PHYS-1 (add PhysicalMaterialPath to FMaterialData, Rebuild)")
print("=" * 50)
