# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Graphics\Shader\ShadowShader\ShadowShader.h"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

text = text.replace('#include "../../../Framework/GDF/GDF.h"', '#include "../../../Graphics/GDF/GDF.h"')

with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
print("Fixed ShadowShader.h")
