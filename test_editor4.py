# coding: utf-8
import subprocess

res = subprocess.run(["git", "show", "HEAD:Source/Framework/ImGuiEditor/Editor/Editor.cpp"], capture_output=True)
raw = res.stdout
try: text = raw.decode("utf-8-sig")
except: text = raw.decode("cp932", errors="replace")

lines = text.split("\n")
depth = 0
for i, line in enumerate(lines):
    depth += line.count('{')
    depth -= line.count('}')
    if "void Editor::" in line:
        print(f"Line {i+1} Function Start: depth = {depth}")
    if depth == 0 and "}" in line:
        print(f"Line {i+1} depth hit 0: {line}")
