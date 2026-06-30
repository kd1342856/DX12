# coding: utf-8
import subprocess

filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
res = subprocess.run(["git", "show", "HEAD:Source/Framework/ImGuiEditor/Editor/Editor.cpp"], capture_output=True)
text = res.stdout.decode("cp932", errors="replace")

lines = text.split("\n")

def check_compile(start, end):
    test_lines = list(lines)
    for i in range(start, end+1):
        test_lines[i] = "// " + test_lines[i]
        
    with open(filepath, "w", encoding="cp932") as f:
        f.write("\n".join(test_lines))
        
    subprocess.run(["py", r"c:\GitHub\DX12\fix_all_includes.py"], capture_output=True)
    
    vsWhere = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    res = subprocess.run([vsWhere, "-latest", "-requires", "Microsoft.Component.MSBuild", "-find", "MSBuild\\**\\Bin\\MSBuild.exe"], capture_output=True)
    msbuild_path = res.stdout.decode().strip()
    
    args = [msbuild_path, "DX12Framework.sln", "/p:Configuration=Debug", "/p:Platform=x64", "/t:ClCompile"]
    res = subprocess.run(args, capture_output=True, cwd=r"C:\GitHub\DX12")
    output = res.stdout.decode('cp932', errors='replace')
    if "C2601" in output or "C1075" in output:
        return False
    return True

low = 152
high = 516

while low <= high:
    mid = (low + high) // 2
    print(f"Checking {low} to {mid}...")
    if check_compile(low, mid):
        print("Error is in commented block!")
        high = mid - 1
    else:
        print("Error is NOT in commented block!")
        low = mid + 1

print(f"Error is at line {low}")
