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
    print(f"Line {i+1} depth: {depth}")
