# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\ECS\Components\Data\NativeScript.h"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

text = text.replace('#include "../../../../Library/nlohmann/json.hpp"', '#include "../../../../../Library/nlohmann/json.hpp"')

with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
print("Fixed NativeScript.h path")
