# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\Manager\Collision\CollisionManager.h"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

text = text.replace('"../Manager/Collision/CollisionShape.h"', '"CollisionShape.h"')

with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
print("Fixed CollisionManager.h")
