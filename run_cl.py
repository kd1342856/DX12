# coding: utf-8
import subprocess

cl_path = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\HostX86\x64\cl.exe"
args = [
    cl_path, "/E", "/IC:\GitHub\DX12\packages\directxtex_desktop_2019.2025.10.28.1\build\native\..\..\include",
    "/IC:\GitHub\DX12\packages\directxtk12_desktop_2019.2025.10.28.1\build\native\..\..\include",
    "/IC:\GitHub\DX12\Library\ImGui", "/IC:\GitHub\DX12\Source", "/IC:\GitHub\DX12\Source\Framework",
    "/IC:\GitHub\DX12\Library\nlohmann", "/IC:\GitHub\DX12\Library\assimp\include",
    "/IC:\GitHub\DX12\Library\recastnavigation\include",
    "/FI", "Pch.h", "/D", "_DEBUG", r"C:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
]

res = subprocess.run(args, capture_output=True)
with open(r"c:\GitHub\DX12\Editor_preprocessed.cpp", "wb") as f:
    f.write(res.stdout)
print("Saved preprocessed output. Error:", res.stderr.decode('cp932', errors='replace'))
