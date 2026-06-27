"""
import_icons.py — 批次匯入 ICO_*.png 圖示到 UE5 /Game/Icons/
在 UE5 Editor Output Log 執行：
    py "C:\SkillCreatorUE5\import_icons.py"

前置條件：先執行 python generate_icons.py 產生 asset/icons/ICO_*.png
結果：/Game/Icons/ICO_{EItemId_name}  (Texture2D)
"""

import unreal, os

ICON_DIR = r'C:\SkillCreatorUE5\asset\icons'
DEST     = '/Game/Icons'
tools    = unreal.AssetToolsHelpers.get_asset_tools()

if not os.path.isdir(ICON_DIR):
    print(f'[ERROR] Icon folder not found: {ICON_DIR}')
    print('        Run:  python generate_icons.py  first.')
else:
    tasks = []
    for fname in sorted(os.listdir(ICON_DIR)):
        if not ((fname.startswith('ICO_') or fname.startswith('STA_')) and fname.endswith('.png')):
            continue
        stem = fname[:-4]   # 'ICO_BlockDirt.png' -> 'ICO_BlockDirt'

        t = unreal.AssetImportTask()
        t.set_editor_property('filename',         os.path.join(ICON_DIR, fname))
        t.set_editor_property('destination_path', DEST)
        t.set_editor_property('destination_name', stem)
        t.set_editor_property('replace_existing', True)
        t.set_editor_property('automated',        True)
        t.set_editor_property('save',             True)

        # UI icon：不壓縮（TC_EditorIcon=RGBA8）、不串流、不生成 mip
        tex_opts = unreal.TextureFactory()
        tex_opts.set_editor_property('compression_settings',
            unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex_opts.set_editor_property('lod_group',
            unreal.TextureGroup.TEXTUREGROUP_UI)
        tex_opts.set_editor_property('mip_gen_settings',
            unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        t.set_editor_property('factory', tex_opts)

        tasks.append(t)

    if tasks:
        tools.import_asset_tasks(tasks)
        print(f'[OK] {len(tasks)} icons imported to {DEST}/')
        print('Usage in C++:')
        print('  UTexture2D* T = LoadObject<UTexture2D>(nullptr,')
        print('    TEXT("/Game/Icons/ICO_WeaponWoodSword.ICO_WeaponWoodSword"));')
    else:
        print('[WARN] No ICO_*.png found. Run: python generate_icons.py')
