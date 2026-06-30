# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\GameManager.h"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

text = text.replace('"System/JobSystem/JobSystem.h"', '"JobSystem/JobSystem.h"')

with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
print("Fixed GameManager JobSystem include again")
