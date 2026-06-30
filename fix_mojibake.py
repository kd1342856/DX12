import subprocess
import difflib

def get_original(path):
    # Git paths use forward slashes
    git_path = path.replace("\\", "/").replace("c:/GitHub/DX12/", "")
    # Check old paths if it was moved
    old_paths = {
        "Source/Application/Script/Character/Player/Player.cpp": "Source/Application/Object/Script/Player/Player.cpp",
        "Source/Framework/Editor/Editor.cpp": "Source/Framework/ImGuiEditor/Editor/Editor.cpp",
        "Source/Framework/Manager/Collision/CollisionManager.cpp": "Source/Framework/Manager/CollisionManager.cpp",
        "Source/Framework/Manager/Scene/SceneManager.cpp": "Source/Framework/Manager/SceneManager.cpp"
    }
    
    target_git_path = old_paths.get(git_path, git_path)
    
    result = subprocess.run(["git", "show", f"HEAD:{target_git_path}"], capture_output=True)
    if result.returncode != 0:
        print(f"Failed to get {target_git_path} from git")
        return ""
    
    raw = result.stdout
    try:
        text = raw.decode("utf-8-sig")
    except UnicodeDecodeError:
        try:
            text = raw.decode("cp932")
        except UnicodeDecodeError:
            text = raw.decode("utf-8", errors="replace")
    return text.replace("\r\n", "\n")

def process_file(filepath):
    print(f"Processing {filepath}")
    orig = get_original(filepath)
    if not orig: return
    
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        curr = f.read().replace("\r\n", "\n")
        
    orig_lines = orig.splitlines()
    curr_lines = curr.splitlines()
    
    merged = []
    
    # We only want to keep the INCLUDE changes from curr, and code changes from curr that are purely English.
    # Actually, we can just use difflib
    matcher = difflib.SequenceMatcher(None, orig_lines, curr_lines)
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            merged.extend(orig_lines[i1:i2])
        elif tag == "replace":
            # For each replaced line, if the new line has mojibake characters, keep the original line!
            # Otherwise, keep the new line!
            for k in range(j2 - j1):
                new_line = curr_lines[j1 + k] if j1 + k < len(curr_lines) else ""
                old_line = orig_lines[i1 + k] if i1 + k < len(orig_lines) else ""
                
                # Check for mojibake in new_line
                if '\ufffd' in new_line or 'E??' in new_line or 'E?' in new_line or 'ƒ`E' in new_line:
                    merged.append(old_line)
                else:
                    # Is it just an include change?
                    if old_line.strip().startswith("#include") and new_line.strip().startswith("#include"):
                        merged.append(new_line)
                    else:
                        # If there are code changes, we prefer the new line unless it has mojibake
                        merged.append(new_line)
                        
            # If the lengths don't match, just append the remaining new lines (assuming no mojibake)
            for k in range(j2 - j1, j2 - j1):
                merged.append(curr_lines[j1 + k])
                
        elif tag == "insert":
            for line in curr_lines[j1:j2]:
                if '\ufffd' not in line and 'E??' not in line:
                    merged.append(line)
        elif tag == "delete":
            # We allow deletion of lines (e.g. dummy functions removed)
            pass

    final_text = "\n".join(merged) + "\n"
    
    # Write as cp932 as per user rule!
    with open(filepath, "w", encoding="cp932", errors="replace") as f:
        f.write(final_text)

files = [
    r"c:\GitHub\DX12\Source\Application\Application.cpp",
    r"c:\GitHub\DX12\Source\Application\Application.h",
    r"c:\GitHub\DX12\Source\Application\Scene\GameScene\GameScene.cpp",
    r"c:\GitHub\DX12\Source\Application\Scene\TitleScene\TitleScene.cpp",
    r"c:\GitHub\DX12\Source\Application\Script\Character\Player\Player.cpp",
    r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp",
    r"c:\GitHub\DX12\Source\Framework\Manager\Collision\CollisionManager.cpp",
    r"c:\GitHub\DX12\Source\Framework\Manager\Scene\SceneManager.cpp"
]

for f in files:
    process_file(f)

print("Done")
