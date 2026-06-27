"""
generate_icons.py — 為每個 EItemId 生成 64×64 像素風格圖示（PNG）

Run:
    python generate_icons.py

Output: C:\SkillCreatorUE5\asset\icons\ICO_*.png
Next:   在 UE5 Editor 直接 Import 這些 PNG 作為 Texture2D，
        或 import_icons.py 批次匯入到 /Game/Icons/ICO_{Name}

圖示設計原則：
  - 64×64，黑色背景，像素風格
  - 色系對應材質類型（土=棕、石=灰、木=橙棕、金屬=銀、魔法=紫）
  - 形狀對應物品類別（劍=長菱形、斧=扇形、礦石=不規則球、碎片=小碎塊…）
  - 已存在檔案自動跳過；刪除後重跑才重新生成
"""

from PIL import Image, ImageDraw, ImageFilter
import os, math

OUT_DIR = r"C:\SkillCreatorUE5\asset\icons"
os.makedirs(OUT_DIR, exist_ok=True)

SIZE   = 64
BG     = (18, 18, 24)       # 深藍黑背景
ALPHA  = True                # RGBA（支援透明邊框）

# ── 調色盤 ──────────────────────────────────────────────────────────────────
C = {
    'dirt':    (139, 100,  55),
    'stone':   (140, 140, 140),
    'wood':    (180, 120,  60),
    'sand':    (220, 200, 130),
    'ash':     (180, 175, 170),
    'coal':    ( 60,  60,  70),
    'copper':  (200, 120,  50),
    'iron':    (170, 175, 185),
    'gold':    (220, 185,  50),
    'crystal': (160, 100, 220),
    'green':   ( 80, 170,  80),
    'red':     (210,  60,  60),
    'silver':  (190, 195, 205),
    'brown':   (140,  90,  45),
    'leaf':    ( 70, 150,  50),
    'white':   (230, 230, 220),
    'magic':   (130,  80, 210),
    'orange':  (220, 130,  40),
    'yellow':  (220, 200,  60),
    'teal':    ( 60, 180, 170),
    'hi':      (255, 255, 255),   # 高光
    'shadow':  ( 30,  30,  40),   # 陰影
}

def new_img():
    img = Image.new('RGBA', (SIZE, SIZE), (*BG, 255))
    return img, ImageDraw.Draw(img)

def save(img, name):
    path = os.path.join(OUT_DIR, f"ICO_{name}.png")
    if os.path.exists(path):
        print(f"  -  {name}  (exists)")
        return
    img.save(path)
    print(f"  OK {name}")


# ── 基礎繪製工具 ─────────────────────────────────────────────────────────────

def px(d, points, color, width=1):
    """連接多邊形頂點（不填充）"""
    d.polygon(points, outline=color)

def fill_poly(d, points, color, outline=None):
    d.polygon(points, fill=color, outline=outline or color)

def circle(d, cx, cy, r, color, outline=None):
    d.ellipse([cx-r, cy-r, cx+r, cy+r], fill=color, outline=outline)

def rect(d, x1, y1, x2, y2, color, outline=None):
    d.rectangle([x1, y1, x2, y2], fill=color, outline=outline)

def add_highlight(d, cx, cy, r=4):
    """左上角小高光點"""
    d.ellipse([cx-r, cy-r, cx+r//2, cy+r//2],
              fill=(*C['hi'], 120))

def add_shadow(d, points):
    """在多邊形右下偏移畫陰影"""
    shadow_pts = [(x+2, y+2) for x, y in points]
    d.polygon(shadow_pts, fill=(*C['shadow'], 80))


# ── 物品圖示生成器 ────────────────────────────────────────────────────────────

def icon_block(name, color):
    img, d = new_img()
    c = color
    dark = tuple(max(0, v-50) for v in c[:3])
    rect(d, 12, 12, 52, 52, c)
    rect(d, 12, 12, 52, 14, C['hi'])      # 頂部高光線
    rect(d, 12, 12, 14, 52, C['hi'])      # 左側高光線
    rect(d, 50, 12, 52, 52, dark)         # 右側暗線
    rect(d, 12, 50, 52, 52, dark)         # 底部暗線
    save(img, name)


def icon_ore(name, color):
    img, d = new_img()
    c = color
    dark = tuple(max(0, v-60) for v in c[:3])
    light = tuple(min(255, v+60) for v in c[:3])
    # 不規則球體（多邊形模擬）
    pts = [(32,10),(45,14),(52,24),(54,36),(48,48),(36,54),(22,52),(14,40),(12,28),(20,16)]
    add_shadow(d, pts)
    fill_poly(d, pts, c)
    # 礦脈紋路
    d.line([(28,22),(38,30),(32,42)], fill=dark, width=2)
    d.line([(22,36),(30,30)], fill=dark, width=1)
    add_highlight(d, 22, 20, 5)
    save(img, name)


def icon_fragment(name, color):
    img, d = new_img()
    c = color
    dark = tuple(max(0, v-70) for v in c[:3])
    # 三個小碎塊
    for (pts, col) in [
        ([(20,15),(32,12),(36,22),(24,26)], c),
        ([(12,30),(22,28),(24,42),(10,40)], dark),
        ([(30,32),(46,28),(50,44),(34,46)], c),
    ]:
        fill_poly(d, pts, col, outline=dark)
    save(img, name)


def icon_sword(name, color, length=44, width=6):
    img, d = new_img()
    c = color
    cx = 32
    # 刀身（漸縮菱形）
    blade = [(cx, 8), (cx+width//2, 30), (cx, 50), (cx-width//2, 30)]
    add_shadow(d, blade)
    fill_poly(d, blade, c)
    # 護手
    rect(d, cx-12, 28, cx+12, 32, C['brown'])
    # 握柄
    rect(d, cx-3, 32, cx+3, 48, C['brown'])
    # 護手頂寶石
    circle(d, cx, 26, 3, C['crystal'])
    # 刀身高光
    d.line([(cx, 10),(cx-1, 28)], fill=(*C['hi'], 160), width=1)
    save(img, name)


def icon_axe(name):
    img, d = new_img()
    # 柄
    d.line([(20, 54),(44, 16)], fill=C['brown'], width=5)
    # 斧頭（扇形）
    pts = [(36,12),(52,16),(54,30),(42,34),(36,28)]
    add_shadow(d, pts)
    fill_poly(d, pts, C['iron'])
    fill_poly(d, [(52,16),(54,30),(56,22)], C['silver'])   # 刃面高光
    save(img, name)


def icon_pick(name, color=None):
    img, d = new_img()
    c = color or C['iron']
    # 柄
    d.line([(32,54),(32,30)], fill=C['brown'], width=4)
    # 左爪
    pts_l = [(32,28),(16,16),(12,22),(28,32)]
    # 右爪
    pts_r = [(32,28),(48,16),(52,22),(36,32)]
    add_shadow(d, pts_l); fill_poly(d, pts_l, c)
    add_shadow(d, pts_r); fill_poly(d, pts_r, c)
    # 中央鐵環
    circle(d, 32, 30, 5, C['stone'])
    save(img, name)


def icon_shovel(name):
    img, d = new_img()
    # 長柄
    d.line([(32, 10),(32, 44)], fill=C['brown'], width=4)
    # 鏟頭
    rect(d, 22, 42, 42, 56, C['iron'], outline=C['stone'])
    # 高光
    rect(d, 22, 42, 24, 56, C['hi'])
    save(img, name)


def icon_equip_helmet(name):
    img, d = new_img()
    # 半球頭盔
    pts = [(16,38),(14,28),(20,18),(32,14),(44,18),(50,28),(48,38)]
    fill_poly(d, pts, C['brown'])
    # 護面縫
    rect(d, 24,32, 40,40, C['shadow'])
    # 高光
    d.arc([18,16,46,44], start=200, end=320, fill=C['hi'], width=2)
    save(img, name)


def icon_equip_armor(name):
    img, d = new_img()
    # 胸甲輪廓
    pts = [(18,16),(46,16),(52,26),(52,48),(46,54),(18,54),(12,48),(12,26)]
    fill_poly(d, pts, C['brown'])
    # 中央鑲嵌
    fill_poly(d, [(28,22),(36,22),(38,38),(26,38)], C['shadow'])
    circle(d, 32, 30, 4, C['copper'])
    # 高光
    d.line([(18,16),(18,48)], fill=(*C['hi'], 100), width=2)
    save(img, name)


def icon_equip_pants(name):
    img, d = new_img()
    # 腰帶
    rect(d, 16, 12, 48, 22, C['brown'])
    # 左腿
    pts_l = [(16,22),(28,22),(30,54),(14,54)]
    fill_poly(d, pts_l, C['brown'], outline=C['shadow'])
    # 右腿
    pts_r = [(36,22),(48,22),(50,54),(34,54)]
    fill_poly(d, pts_r, C['brown'], outline=C['shadow'])
    save(img, name)


def icon_equip_boots(name):
    img, d = new_img()
    # 靴身
    pts = [(16,20),(48,20),(50,44),(48,54),(16,54),(12,44)]
    fill_poly(d, pts, C['brown'])
    # 鞋尖
    fill_poly(d, [(44,48),(56,44),(54,54),(44,54)], C['brown'])
    # 綁帶
    for y in [28,36]:
        rect(d, 16, y, 48, y+3, C['shadow'])
    save(img, name)


def icon_amulet(name):
    img, d = new_img()
    # 鍊子
    d.arc([28, 8, 36, 20], start=0, end=180, fill=C['silver'], width=2)
    # 吊墜圓盤
    circle(d, 32, 38, 16, C['magic'])
    circle(d, 32, 38, 12, C['shadow'])
    circle(d, 32, 38,  6, C['crystal'])
    # 高光
    circle(d, 27, 33, 3, (*C['hi'], 160))
    save(img, name)


def icon_plank(name):
    img, d = new_img()
    for i, y in enumerate([12, 24, 36, 48]):
        col = C['wood'] if i % 2 == 0 else tuple(max(0, v-25) for v in C['wood'])
        rect(d, 10, y, 54, y+10, col, outline=C['shadow'])
    save(img, name)


def icon_stick(name):
    img, d = new_img()
    d.line([(18, 10),(46, 54)], fill=C['brown'], width=5)
    d.line([(19, 10),(47, 54)], fill=(*C['hi'], 80), width=2)
    save(img, name)


def icon_chest(name):
    img, d = new_img()
    # 箱身
    rect(d, 10, 30, 54, 54, C['wood'])
    # 箱蓋
    rect(d, 10, 16, 54, 32, C['brown'])
    # 蓋縫
    rect(d, 10, 30, 54, 33, C['shadow'])
    # 鎖
    rect(d, 28, 34, 36, 42, C['yellow'])
    circle(d, 32, 38, 2, C['shadow'])
    # 高光
    rect(d, 10, 16, 12, 32, (*C['hi'], 80))
    save(img, name)


def icon_workbench(name):
    img, d = new_img()
    # 桌面
    rect(d, 8, 14, 56, 26, C['wood'])
    rect(d, 8, 14, 56, 16, C['hi'])         # 頂面高光
    # 正面
    rect(d, 8, 26, 56, 46, C['brown'])
    # 工具紋路（交叉鋸紋）
    d.line([(14,28),(50,44)], fill=C['shadow'], width=1)
    d.line([(14,44),(50,28)], fill=C['shadow'], width=1)
    # 腿
    rect(d, 10, 46, 18, 56, C['wood'])
    rect(d, 46, 46, 54, 56, C['wood'])
    save(img, name)


def icon_berry(name, color=None):
    img, d = new_img()
    c = color or C['red']
    # 果實
    circle(d, 32, 36, 18, c)
    # 高光
    circle(d, 25, 29, 5, (*C['hi'], 140))
    # 葉子
    pts = [(30,20),(38,12),(44,18),(36,24)]
    fill_poly(d, pts, C['leaf'])
    save(img, name)


def icon_oaklog(name):
    img, d = new_img()
    # 圓木截面
    circle(d, 32, 32, 22, C['wood'])
    # 年輪
    for r in [16, 10, 5]:
        circle(d, 32, 32, r, None, outline=tuple(max(0,v-40) for v in C['wood']))
    # 樹皮高光
    d.arc([10,10,54,54], start=200, end=300, fill=C['hi'], width=2)
    save(img, name)


def icon_sapling(name):
    img, d = new_img()
    # 幹
    d.line([(32, 54),(32, 34)], fill=C['brown'], width=4)
    # 小樹冠
    for (cx, cy, r) in [(32,28,10),(24,32,7),(40,32,7),(32,20,8)]:
        circle(d, cx, cy, r, C['leaf'])
    save(img, name)


def icon_fallenleaf(name):
    img, d = new_img()
    # 葉片橢圓
    pts = [(16,32),(32,14),(48,32),(32,50)]
    fill_poly(d, pts, C['leaf'])
    # 葉脈
    d.line([(32,16),(32,50)], fill=C['shadow'], width=1)
    for angle in [-30, 30, -50, 50]:
        r = math.radians(angle)
        ex = int(32 + 14*math.sin(r))
        ey = int(33 - 12*abs(math.cos(r)) * math.copysign(1, angle))
        d.line([(32,33),(ex,ey)], fill=C['shadow'], width=1)
    save(img, name)


def icon_oakfruit(name):
    img, d = new_img()
    # 果身
    circle(d, 32, 36, 14, C['brown'])
    # 果帽
    pts = [(22,24),(42,24),(44,32),(20,32)]
    fill_poly(d, pts, C['wood'])
    # 莖
    d.line([(32,24),(34,14)], fill=C['brown'], width=3)
    # 高光
    circle(d, 27, 31, 4, (*C['hi'], 120))
    save(img, name)


def icon_weed(name):
    img, d = new_img()
    for (x1,y1,x2,y2,w) in [
        (32,54,22,16,3), (32,54,38,20,3), (32,54,44,32,2)
    ]:
        d.line([(x1,y1),(x2,y2)], fill=C['leaf'], width=w)
    # 尖端
    for (cx,cy) in [(22,16),(38,20),(44,32)]:
        pts = [(cx-2,cy+4),(cx,cy-4),(cx+2,cy+4)]
        fill_poly(d, pts, C['green'])
    save(img, name)


def icon_magic_ore(name):
    img, d = new_img()
    # 多面體（魔晶石）
    pts = [(32,8),(50,20),(54,38),(44,54),(20,54),(10,38),(14,20)]
    fill_poly(d, pts, C['magic'])
    # 內光
    inner = [(32,18),(44,26),(46,38),(38,48),(26,48),(18,38),(20,26)]
    fill_poly(d, inner, C['crystal'])
    center = [(32,24),(38,30),(38,40),(26,40),(26,30)]
    fill_poly(d, center, C['magic'])
    # 高光
    circle(d, 26, 22, 4, (*C['hi'], 180))
    save(img, name)


def icon_equip_sword_base(name, color):
    """基礎劍形（較短，適合初始裝備）"""
    icon_sword(name, color, length=38, width=5)


# ── 主生成表 ─────────────────────────────────────────────────────────────────

def generate_all():
    print(f"\n{'='*50}")
    print(f"SkillCreator icon generator — Output: {OUT_DIR}")
    print(f"{'='*50}\n")

    # 方塊
    icon_block('BlockDirt',  C['dirt'])
    icon_block('BlockStone', C['stone'])
    icon_block('BlockWood',  C['wood'])
    icon_block('BlockSand',  C['sand'])
    icon_block('BlockAsh',   C['ash'])

    # 工具
    icon_pick  ('ToolBasicPick')
    icon_axe   ('ToolBasicAxe')
    icon_pick  ('ToolIronPick')
    icon_shovel('ToolWoodShovel')
    icon_pick  ('ToolStonePick')

    # 武器
    icon_sword('EquipBasicSword',  C['iron'],   length=40)
    icon_sword('WeaponWoodSword',  C['wood'],   length=38)
    icon_sword('WeaponStoneSword', C['stone'],  length=42)

    # 裝備
    icon_equip_helmet('EquipLeatherHelmet')
    icon_equip_armor ('EquipLeatherArmor')
    icon_equip_pants ('EquipLeatherPants')
    icon_equip_boots ('EquipLeatherBoots')
    icon_amulet      ('EquipAmulet')

    # 礦石
    icon_ore('OreCoal',         C['coal'])
    icon_ore('OreCopperRaw',    C['copper'])
    icon_ore('OreIronRaw',      C['iron'])
    icon_ore('OreGoldRaw',      C['gold'])
    icon_magic_ore('OreMagicCrystal')

    # 碎片
    icon_fragment('FragmentDirt',         C['dirt'])
    icon_fragment('FragmentStone',        C['stone'])
    icon_fragment('FragmentSand',         C['sand'])
    icon_fragment('FragmentWood',         C['wood'])
    icon_fragment('FragmentAsh',          C['ash'])
    icon_fragment('FragmentCoal',         C['coal'])
    icon_fragment('FragmentCopper',       C['copper'])
    icon_fragment('FragmentIron',         C['iron'])
    icon_fragment('FragmentMagicCrystal', C['crystal'])

    # 素材
    icon_plank('MaterialPlank')
    icon_stick('MaterialStick')

    # 可放置物
    icon_chest    ('PlaceableWoodChest')
    icon_workbench('PlaceableWoodWorkbench')

    # 道具
    icon_berry  ('ConsumableBerry', C['red'])
    icon_oakfruit('OakFruit')

    # 樹木產物
    icon_oaklog    ('OakLog')
    icon_fallenleaf('FallenLeaf')
    icon_sapling   ('OakSapling')

    # 地表植物
    icon_weed('Weed')

    print(f"\n{'='*50}")
    print("Done! Next: run import_icons.py in UE5 Editor Python Console")
    print(f"{'='*50}")


if __name__ == '__main__':
    generate_all()
