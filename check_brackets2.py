# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
with open(filepath, "r", encoding="cp932") as f:
    lines = f.readlines()

depth = 0
for i, line in enumerate(lines):
    depth += line.count('{')
    depth -= line.count('}')
    if "void Editor::" in line:
        print(f"Line {i+1}: {line.strip()} | Depth before this line: {depth - line.count('{') + line.count('}')}")
