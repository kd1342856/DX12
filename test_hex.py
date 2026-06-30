# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
with open(filepath, "rb") as f:
    text = f.read()

lines = text.split(b"\n")
print(lines[151])
