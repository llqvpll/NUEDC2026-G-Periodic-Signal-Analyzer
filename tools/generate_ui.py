# -*- coding: utf-8 -*-
"""
DGUS II 迪文屏 UI 背景图生成器 - G题周期信号测量分析装置
基于用户提供的奶龙天使油画背景，生成3个页面 (800x480, 24-bit BMP)
Page 0: 参数显示页
Page 1: 波形显示页
Page 2: 频谱分析页

注意: 数值显示区域只画空框和标签, 不写死数字, 由 DGUS "数据变量显示" 控件填充
"""
from PIL import Image, ImageDraw, ImageFont
import os

W, H = 800, 480

# 与原油画背景相配的金棕+象牙白配色
C_PANEL    = (45, 35, 25, 210)   # 深棕面板 (带透明, 会混合到背景)
C_PANEL2   = (55, 42, 28, 180)   # 浅一点的深棕
C_BORDER   = (212, 185, 140)     # 金色边框
C_TEXT     = (255, 248, 230)     # 象牙白文字
C_TEXT_DIM = (200, 180, 150)     # 暗金文字
C_ACCENT   = (255, 215, 120)     # 亮金强调
C_GRID     = (120, 100, 80, 80)  # 网格线
C_GRID_MAJ = (180, 155, 115, 120)
C_SLOT     = (30, 24, 18, 160)   # 数值空槽底色
C_SLOT_LINE= (160, 135, 100)     # 空槽边框

BG_PATH = r"C:\Users\Administrator\.workbuddy\clipboard-images\clipboard-2026-07-31T04-21-23-036Z-7331116a.png"

def load_font(size, bold=False):
    paths = [
        "C:/Windows/Fonts/msyhbd.ttc" if bold else "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/arial.ttf",
    ]
    for p in paths:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except:
                continue
    return ImageFont.load_default()

def load_mono(size, bold=False):
    paths = [
        "C:/Windows/Fonts/consolab.ttf" if bold else "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/cour.ttf",
        "C:/Windows/Fonts/arial.ttf",
    ]
    for p in paths:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except:
                continue
    return ImageFont.load_default()

def make_background():
    """加载原图并裁切成 800x480, 保留中心区域"""
    bg = Image.open(BG_PATH).convert("RGB")
    bw, bh = bg.size
    # 目标 800x480 = 5:3, 原图 1:1, 取横向居中、纵向偏上裁剪
    target_ratio = W / H  # 1.6667
    crop_w = bw
    crop_h = int(crop_w / target_ratio)
    if crop_h > bh:
        crop_h = bh
        crop_w = int(crop_h * target_ratio)
    left = (bw - crop_w) // 2
    top = int((bh - crop_h) * 0.25)  # 偏上一点, 让两个奶龙都在画面里
    bg = bg.crop((left, top, left + crop_w, top + crop_h))
    bg = bg.resize((W, H), Image.LANCZOS)
    return bg

def draw_rounded_rect_alpha(img, xy, radius, fill_rgba, outline=None, width=1):
    """在现有图像上画带透明的圆角矩形"""
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    draw.rounded_rectangle(xy, radius=radius, fill=fill_rgba, outline=outline, width=width)
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))

def draw_rounded_rect(draw, xy, radius, fill=None, outline=None, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)

def draw_grid(img, x, y, w, h, cols=10, rows=6):
    draw = ImageDraw.Draw(img)
    # 网格区域先加深半透明背景
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rectangle([x, y, x+w, y+h], fill=(25, 18, 12, 180))
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))
    draw = ImageDraw.Draw(img)
    draw.rectangle([x, y, x+w, y+h], outline=C_BORDER, width=2)
    for i in range(1, cols):
        gx = x + w * i // cols
        color = C_GRID_MAJ[:3] if i == cols // 2 else C_GRID[:3]
        draw.line([gx, y, gx, y+h], fill=color, width=1)
    for i in range(1, rows):
        gy = y + h * i // rows
        color = C_GRID_MAJ[:3] if i == rows // 2 else C_GRID[:3]
        draw.line([x, gy, x+w, gy], fill=color, width=1)
    cx, cy = x + w // 2, y + h // 2
    draw.line([cx-8, cy, cx+8, cy], fill=C_ACCENT, width=1)
    draw.line([cx, cy-8, cx, cy+8], fill=C_ACCENT, width=1)

def draw_title_bar(img, title):
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(overlay)
    d.rectangle([0, 0, W, 46], fill=(35, 26, 18, 220))
    d.line([0, 46, W, 46], fill=C_ACCENT, width=2)
    d.rectangle([0, 0, 5, 46], fill=C_ACCENT)
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))
    draw = ImageDraw.Draw(img)
    f = load_font(20, bold=True)
    draw.text((18, 11), title, fill=C_TEXT, font=f)
    f2 = load_font(12)
    draw.text((W - 140, 16), "STM32F429 @168MHz", fill=C_TEXT_DIM, font=f2)

def draw_button_bar(img, buttons):
    """底部按钮栏, buttons=[(text, hot), ...]"""
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(overlay)
    bar_y = 408
    d.rectangle([0, bar_y, W, H], fill=(35, 26, 18, 220))
    d.line([0, bar_y, W, bar_y], fill=C_BORDER, width=1)
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))
    draw = ImageDraw.Draw(img)
    n = len(buttons)
    margin = 8
    bw = (W - margin * (n + 1)) // n
    bh = 48
    by = bar_y + 12
    for i, (text, hot) in enumerate(buttons):
        bx = margin + i * (bw + margin)
        fill = (90, 68, 42) if hot else (50, 38, 26)
        border = C_ACCENT if hot else C_BORDER
        draw.rounded_rectangle([bx, by, bx+bw, by+bh], radius=6, fill=fill, outline=border, width=2)
        f = load_font(15, bold=True)
        bbox = f.getbbox(text)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        draw.text((bx + (bw - tw)//2, by + (bh - th)//2 - 2), text, fill=C_TEXT, font=f)

def draw_data_slot(img, x, y, w, h, label, unit, big=False):
    """数据槽: 深棕半透明背景 + 标签 + 单位, 数值区域画空框不写数字"""
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(overlay)
    d.rounded_rectangle([x, y, x+w, y+h], radius=8, fill=C_PANEL, outline=C_BORDER, width=1)
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))
    draw = ImageDraw.Draw(img)
    f_label = load_font(14)
    draw.text((x + 14, y + 8), label, fill=C_TEXT_DIM, font=f_label)
    if unit:
        f_unit = load_font(13)
        draw.text((x + w - 70, y + 10), unit, fill=C_TEXT_DIM, font=f_unit)
    # 数值空槽 (只画框, 不写数字)
    slot_h = 36 if big else 28
    slot_y = y + h - slot_h - 10
    draw.rounded_rectangle([x + 12, slot_y, x + w - 12, slot_y + slot_h], radius=4,
                           fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
    # 画3条淡色占位线, 表示这里是数据区, 但不像真实数字
    line_color = (80, 65, 50)
    ly = slot_y + slot_h // 2
    if big:
        draw.line([x + 22, ly, x + 180, ly], fill=line_color, width=2)
    else:
        draw.line([x + 22, ly, x + 130, ly], fill=line_color, width=2)

# ========== Page 0: 参数显示页 ==========
def create_page0():
    img = make_background()
    draw_title_bar(img, "周期信号测量分析装置  G题")

    # 基频大槽
    draw_data_slot(img, 20, 62, 760, 92, "基频  f1", "kHz", big=True)
    d = ImageDraw.Draw(img)
    d.line([20, 160, 780, 160], fill=C_BORDER, width=1)

    # Upp + Urms
    draw_data_slot(img, 20, 174, 370, 90, "峰峰值  Upp", "mV")
    draw_data_slot(img, 410, 174, 370, 90, "真有效值  Urms", "mV")

    # 模式 + 采样率信息栏
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rounded_rectangle([20, 278, 780, 332], radius=8, fill=C_PANEL, outline=C_BORDER, width=1)
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))
    d = ImageDraw.Draw(img)
    f = load_font(15)
    d.text((34, 288), "测量模式:", fill=C_TEXT_DIM, font=f)
    # 模式空槽 (动态显示 Item1/Item2/Item3, 不写死)
    d.rounded_rectangle([130, 286, 350, 324], radius=4, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
    d.text((500, 288), "采样率:", fill=C_TEXT_DIM, font=f)
    # 采样率数值空槽
    d.rounded_rectangle([570, 286, 690, 324], radius=4, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
    d.text((696, 294), "MHz", fill=C_TEXT_DIM, font=load_font(13))

    # 谐波预览栏
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rounded_rectangle([20, 344, 780, 392], radius=8, fill=C_PANEL, outline=C_BORDER, width=1)
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))
    d = ImageDraw.Draw(img)
    f2 = load_font(13)
    d.text((34, 354), "谐波:", fill=C_TEXT_DIM, font=f2)
    for i in range(5):
        hx = 90 + i * 130
        d.text((hx, 354), f"H{i+1}", fill=C_TEXT_DIM, font=f2)
        d.rounded_rectangle([hx + 30, 354, hx + 110, 382], radius=3, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)

    draw_button_bar(img, [
        ("Item1", True), ("Item2", False), ("Item3", False),
        ("波形", False), ("频谱", False), ("校准", False),
    ])
    return img

# ========== Page 1: 波形显示页 ==========
def create_page1():
    img = make_background()
    draw_title_bar(img, "波形显示")
    draw_grid(img, 20, 58, 760, 320, cols=10, rows=8)

    d = ImageDraw.Draw(img)
    f = load_font(11)
    d.text((4, 60), "+", fill=C_TEXT_DIM, font=f)
    d.text((6, 212), "0", fill=C_TEXT_DIM, font=f)
    d.text((6, 360), "-", fill=C_TEXT_DIM, font=f)
    d.text((24, 382), "0", fill=C_TEXT_DIM, font=f)
    d.text((760, 382), "t", fill=C_TEXT_DIM, font=f)

    # 信息栏 (4个数值槽)
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rounded_rectangle([20, 384, 780, 400], radius=4, fill=C_PANEL, outline=C_BORDER, width=1)
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))
    d = ImageDraw.Draw(img)
    f2 = load_font(13)
    # f1槽
    d.text((30, 386), "f1:", fill=C_TEXT_DIM, font=f2)
    d.rounded_rectangle([55, 386, 155, 398], radius=2, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
    d.text((160, 386), "kHz", fill=C_TEXT_DIM, font=f2)
    # Upp槽
    d.text((220, 386), "Upp:", fill=C_TEXT_DIM, font=f2)
    d.rounded_rectangle((255, 386, 340, 398), radius=2, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
    d.text((345, 386), "mV", fill=C_TEXT_DIM, font=f2)
    # Urms槽
    d.text((420, 386), "Urms:", fill=C_TEXT_DIM, font=f2)
    d.rounded_rectangle((460, 386, 545, 398), radius=2, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
    d.text((550, 386), "mV", fill=C_TEXT_DIM, font=f2)
    # 时基
    d.text((640, 386), "1/div: ----us", fill=C_TEXT_DIM, font=f2)

    draw_button_bar(img, [
        ("1周期", True), ("3周期", False),
        ("参数", False), ("频谱", False),
    ])
    return img

# ========== Page 2: 频谱分析页 ==========
def create_page2():
    img = make_background()
    draw_title_bar(img, "频谱分析")
    draw_grid(img, 20, 58, 760, 210, cols=12, rows=5)

    d = ImageDraw.Draw(img)
    f = load_font(11)
    d.text((4, 60), "dB", fill=C_TEXT_DIM, font=f)
    d.text((24, 270), "0 Hz", fill=C_TEXT_DIM, font=f)
    d.text((720, 270), "fs/2", fill=C_TEXT_DIM, font=f)

    # 谐波表
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rounded_rectangle([20, 280, 780, 398], radius=8, fill=C_PANEL, outline=C_BORDER, width=1)
    img.paste(Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB"), (0, 0))
    d = ImageDraw.Draw(img)
    d.line([20, 306, 780, 306], fill=C_BORDER, width=1)
    f_h = load_font(13, bold=True)
    d.text((40, 287), "谐波", fill=C_TEXT_DIM, font=f_h)
    d.text((110, 287), "频率 kHz", fill=C_TEXT_DIM, font=f_h)
    d.text((280, 287), "幅值 mV", fill=C_TEXT_DIM, font=f_h)
    d.text((430, 287), "谐波个数 N:", fill=C_TEXT_DIM, font=f_h)
    d.rounded_rectangle([520, 286, 545, 302], radius=2, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
    d.text((560, 287), "谐波", fill=C_TEXT_DIM, font=f_h)
    d.text((630, 287), "频率 kHz", fill=C_TEXT_DIM, font=f_h)
    d.text((710, 287), "幅值 mV", fill=C_TEXT_DIM, font=f_h)

    f_r = load_font(13)
    for i in range(4):
        ry = 312 + i * 22
        # 左列 H1-H4
        d.text((44, ry), f"H{i+1}", fill=C_ACCENT, font=f_r)
        d.rounded_rectangle([110, ry, 200, ry + 16], radius=2, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
        d.rounded_rectangle([280, ry, 340, ry + 16], radius=2, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
        # 右列 H5-H8
        d.text((564, ry), f"H{i+5}", fill=C_ACCENT, font=f_r)
        d.rounded_rectangle([630, ry, 720, ry + 16], radius=2, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)
        d.rounded_rectangle([710, ry, 770, ry + 16], radius=2, fill=C_SLOT[:3], outline=C_SLOT_LINE, width=1)

    draw_button_bar(img, [
        ("频谱图", True), ("分量幅值", False),
        ("参数", False), ("波形", False),
    ])
    return img

def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))
    pages = [
        (create_page0, "page0_params.bmp", "参数显示页"),
        (create_page1, "page1_wave.bmp",   "波形显示页"),
        (create_page2, "page2_spectrum.bmp","频谱分析页"),
    ]
    for func, fname, desc in pages:
        img = func()
        path_bmp = os.path.join(out_dir, fname)
        path_png = os.path.join(out_dir, fname.replace(".bmp", ".png"))
        img.save(path_bmp, "BMP")
        img.save(path_png, "PNG")
        print(f"[OK] {desc} -> {fname}  ({img.size[0]}x{img.size[1]})")
    # 同时复制到 DGUS 工程 image/ 目录
    proj_img = r"C:\Users\Administrator\OneDrive\Desktop\AD DA+AD9959+ADS1256\image"
    if os.path.exists(proj_img):
        import shutil
        shutil.copy(os.path.join(out_dir, "page0_params.bmp"), os.path.join(proj_img, "0.bmp"))
        shutil.copy(os.path.join(out_dir, "page1_wave.bmp"),   os.path.join(proj_img, "1.bmp"))
        shutil.copy(os.path.join(out_dir, "page2_spectrum.bmp"), os.path.join(proj_img, "2.bmp"))
        print(f"[OK] copied 0.bmp/1.bmp/2.bmp -> {proj_img}")
    print(f"\n3 pages generated in: {out_dir}")

if __name__ == "__main__":
    main()
