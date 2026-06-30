# coding: utf-8
import subprocess

res = subprocess.run(["git", "show", "HEAD:Source/Framework/ImGuiEditor/Editor/Editor.cpp"], capture_output=True)
text = res.stdout

lines = text.split(b"\n")
print("Line 150 (0-indexed):", lines[150])
print("Line 151 (0-indexed):", lines[151])
print("Line 152 (0-indexed):", lines[152])
