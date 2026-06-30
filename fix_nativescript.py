# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\ECS\Components\Data\NativeScript.h"
with open(filepath, "r", encoding="cp932") as f:
    lines = f.readlines()

if not any("json.hpp" in line for line in lines):
    lines.insert(2, '#include "../../../../Library/nlohmann/json.hpp"\n')

with open(filepath, "w", encoding="cp932") as f:
    f.writelines(lines)
print("Fixed NativeScript.h")
