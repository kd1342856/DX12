# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Pch.h"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

if "ClassAssembly.h" not in text:
    text = text.replace('#include"Framework/Utility/Random.h"', '#include"Framework/Utility/Random.h"\n#include "Framework/Utility/ClassAssembly.h"')
    with open(filepath, "w", encoding="cp932") as f:
        f.write(text)
print("Added ClassAssembly to Pch.h")
