# coding: utf-8
import subprocess

res = subprocess.run(["git", "show", "HEAD:Source/Framework/ImGuiEditor/Editor/Editor.cpp"], capture_output=True)
raw = res.stdout
try: text = raw.decode("utf-8-sig")
except: text = raw.decode("cp932", errors="replace")

lines = text.split("\n")
depth = 0
for i, line in enumerate(lines):
    if 150 <= i <= 518:
        # Check depth in just this function
        depth += line.count('{')
        depth -= line.count('}')
        if depth < 0:
            print(f"Depth went negative at line {i+1}: {line}")

print("Final depth in function:", depth)
