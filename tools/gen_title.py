#!/usr/bin/env python3
# gen_title.py — タイトルYJK画(assets/title.yjk, SCREEN12 256x212 raw)に「改」(毛筆)と「KAI」を合成する。
#   零の咆哮 の隣に大きく毛筆体で「改」、ローマ字 ZERO NO HOUKOU の隣に「KAI」(1スペース空け)。
#   変更した4pxグループのみYJK再エンコード=元画のクオリティを保持(1943改 風の追加)。
import glob, sys, os
from PIL import Image, ImageFont, ImageDraw

W, H = 256, 212
SRC = os.path.join(os.path.dirname(__file__), '..', 'assets', 'title.yjk')

def weibei():
    for p in glob.glob('/System/Library/AssetsV2/**/WeibeiSC-Bold.otf', recursive=True):
        return p
    sys.exit('WeibeiSC-Bold.otf not found')

def clamp(v, lo, hi): return lo if v < lo else hi if v > hi else v

def decode(d):
    im = Image.new('RGB', (W, H))
    px = im.load()
    for y in range(H):
        for gx in range(64):
            b = d[y*256+gx*4: y*256+gx*4+4]
            K = (b[0]&7)|((b[1]&7)<<3); J = (b[2]&7)|((b[3]&7)<<3)
            if K>=32: K-=64
            if J>=32: J-=64
            for i in range(4):
                Y = b[i]>>3
                px[gx*4+i, y] = (clamp(Y+J,0,31)*8, clamp(Y+K,0,31)*8, clamp((5*Y-2*J-K)>>2,0,31)*8)
    return im

def encode_group(rgb4):
    Ys=[]; Js=[]; Ks=[]
    for (R,G,B) in rgb4:
        R>>=3; G>>=3; B>>=3
        Y = clamp((4*B + 2*R + G)//8, 0, 31)
        Ys.append(Y); Js.append(R-Y); Ks.append(G-Y)
    J = clamp(round(sum(Js)/4), -32, 31); K = clamp(round(sum(Ks)/4), -32, 31)
    return bytes([
        (Ys[0]<<3)|(K & 7), (Ys[1]<<3)|((K>>3)&7),
        (Ys[2]<<3)|(J & 7), (Ys[3]<<3)|((J>>3)&7),
    ])

def draw_outlined(base, xy, text, font, fill, outline, ow=2):
    d = ImageDraw.Draw(base)
    x, y = xy
    for dx in range(-ow, ow+1):
        for dy in range(-ow, ow+1):
            if dx or dy: d.text((x+dx, y+dy), text, font=font, fill=outline)
    d.text((x, y), text, font=font, fill=fill)

def main():
    d = bytearray(open(SRC, 'rb').read())
    orig = decode(d)
    new = orig.copy()
    # 「改」= 毛筆(魏碑) 大。零の咆哮(哮 右端~x200)の隣、白+暗赤縁で1943改風。
    fkanji = ImageFont.truetype(weibei(), 58)
    draw_outlined(new, (200, 34), '改', fkanji, (255,255,255), (120,0,0), ow=3)
    # 「KAI」= ローマ字 ZERO NO HOUKOU(~x150で終わり)の隣に1スペース空け。斜体ボールドで雰囲気を合わせる。
    flat = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf', 13)
    draw_outlined(new, (158, 84), 'KAI', flat, (232,240,255), (0,0,40), ow=1)
    # 変更グループのみ再エンコード
    op = orig.load(); npx = new.load(); changed = 0
    for y in range(H):
        for gx in range(64):
            x0 = gx*4
            grp = [npx[x0+i, y] for i in range(4)]
            if any(npx[x0+i, y] != op[x0+i, y] for i in range(4)):
                enc = encode_group(grp)
                base = y*256+gx*4
                d[base:base+4] = enc
                changed += 1
    open(SRC, 'wb').write(d)
    decode(bytes(d)).resize((W*2, H*2), Image.NEAREST).save(
        os.path.join(os.path.dirname(__file__), '..', 'build', 'title_new.png'))
    print(f'title.yjk updated: {changed} groups re-encoded')

if __name__ == '__main__':
    main()
