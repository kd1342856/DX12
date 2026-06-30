# coding: utf-8
import os

h_files = {}
dups = set()
for dirpath, _, filenames in os.walk(r"c:\GitHub\DX12\Source"):
    if ".git" in dirpath: continue
    for f in filenames:
        if f.endswith(".h"):
            if f in h_files:
                dups.add(f)
            h_files[f] = os.path.join(dirpath, f)

print("Duplicates:", dups)
