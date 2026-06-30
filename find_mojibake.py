# coding: utf-8
import os
import glob

def check_file(path):
    try:
        with open(path, "r", encoding="cp932") as f:
            text = f.read()
            if '??' in text and not path.endswith('.py'):
                # Many comments might have been replaced with ??
                lines = text.split('\n')
                for i, line in enumerate(lines):
                    if '// ?' in line or '/* ?' in line or '??' in line:
                        print(f"{path}:{i+1} : {line.strip()}")
                        return True
    except:
        pass
    return False

for root, _, files in os.walk(r"c:\GitHub\DX12\Source"):
    for file in files:
        if file.endswith('.cpp') or file.endswith('.h'):
            check_file(os.path.join(root, file))
