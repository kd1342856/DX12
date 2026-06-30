# coding: utf-8
import subprocess

filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
res = subprocess.run(["git", "show", "HEAD:Source/Framework/ImGuiEditor/Editor/Editor.cpp"], capture_output=True)
text = res.stdout.decode("cp932", errors="replace")

lines = text.split("\n")
print(f"Line 151: {repr(lines[150])}")
print(f"Line 152: {repr(lines[151])}")
print(f"Line 153: {repr(lines[152])}")
