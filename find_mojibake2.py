# coding: utf-8
import os

def find_corrupted_files(root_dir):
    corrupted = []
    for dirpath, _, filenames in os.walk(root_dir):
        if '.git' in dirpath: continue
        for f in filenames:
            if not f.endswith(('.cpp', '.h')): continue
            filepath = os.path.join(dirpath, f)
            try:
                with open(filepath, 'r', encoding='utf-8') as file:
                    content = file.read()
                    if '\ufffd' in content or 'E??' in content or 'E?' in content:
                        corrupted.append(filepath)
            except UnicodeDecodeError:
                pass
    return corrupted

files = find_corrupted_files(r"c:\GitHub\DX12\Source")
print("Corrupted files:")
for f in files:
    print(f)
