# coding: utf-8
# ファイルをUTF-16 LEまたはBOMなしUTF-8 → cp932に変換する
import os

def fix_encoding(path):
    raw = open(path, "rb").read()
    
    # BOMチェック
    if raw[:2] == b'\xff\xfe':
        # UTF-16 LE
        text = raw.decode("utf-16-le")
        out = text.encode("cp932", errors="replace")
        open(path, "wb").write(out)
        print(f"Converted UTF-16LE -> CP932: {path}")
        return
    if raw[:3] == b'\xef\xbb\xbf':
        # UTF-8 with BOM
        text = raw[3:].decode("utf-8", errors="replace")
        out = text.encode("cp932", errors="replace")
        open(path, "wb").write(out)
        print(f"Converted UTF-8 BOM -> CP932: {path}")
        return
    # UTF-8 without BOM check
    try:
        text = raw.decode("utf-8")
        # Check if it has non-ascii
        try:
            text.encode("ascii")
        except UnicodeEncodeError:
            out = text.encode("cp932", errors="replace")
            open(path, "wb").write(out)
            print(f"Converted UTF-8 -> CP932: {path}")
            return
    except UnicodeDecodeError:
        pass
    # Already CP932 or ascii - skip
    # print(f"Skipped (already CP932): {path}")

targets = [
    r"c:\GitHub\DX12\Source\Framework\Object\GameObject.h",
    r"c:\GitHub\DX12\Source\Framework\Manager\Scene\Scene.cpp",
    r"c:\GitHub\DX12\Source\Application\Application.cpp",
]

for t in targets:
    fix_encoding(t)
print("Done!")
