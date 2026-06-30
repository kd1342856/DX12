# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
with open(filepath, "r", encoding="cp932") as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if '"{"' in line or "'{'" in line or '"}"' in line or "'}'" in line or '//' in line and ('{' in line or '}' in line):
        print(f"Line {i+1}: {line.strip()}")
