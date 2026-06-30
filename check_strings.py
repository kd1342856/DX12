# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

import re
strings = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', text)
for s in strings:
    if '{' in s or '}' in s:
        print(f"String with bracket: {s}")
