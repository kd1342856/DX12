# coding: utf-8
import subprocess

filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
res = subprocess.run(["git", "show", "HEAD:Source/Framework/ImGuiEditor/Editor/Editor.cpp"], capture_output=True)
text = res.stdout.decode("cp932", errors="replace")

with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
    
subprocess.run(["py", r"c:\GitHub\DX12\fix_all_includes.py"], capture_output=True)

vsWhere = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
res = subprocess.run([vsWhere, "-latest", "-requires", "Microsoft.Component.MSBuild", "-find", "MSBuild\\**\\Bin\\MSBuild.exe"], capture_output=True)
msbuild_path = res.stdout.decode().strip()

args = [msbuild_path, "DX12Framework.sln", "/p:Configuration=Debug", "/p:Platform=x64", "/t:ClCompile"]
res = subprocess.run(args, capture_output=True, cwd=r"C:\GitHub\DX12")
output = res.stdout.decode('cp932', errors='replace')

if "C2601" in output or "C1075" in output:
    print("Error is there!")
else:
    print("Error is NOT there!")
