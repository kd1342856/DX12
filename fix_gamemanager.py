# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\GameManager.h"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

text = text.replace('#include"../ECS/ECS.h"', '#include "ECS/ECS.h"')
text = text.replace('#include "../ECS/ECS.h"', '#include "ECS/ECS.h"')

with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
print("Fixed GameManager.h")
