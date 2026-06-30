# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
with open(filepath, "r", encoding="cp932") as f:
    lines = f.readlines()

stack = []
for i in range(150, 517):
    line = lines[i]
    for j, ch in enumerate(line):
        if ch == '{':
            stack.append(i + 1)
        elif ch == '}':
            if stack:
                stack.pop()
            else:
                print(f"Extra }} at line {i+1}")

if stack:
    print(f"Unmatched {{ at lines: {stack}")
