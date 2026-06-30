# coding: utf-8
import os
import re

h_files = {}
for dirpath, _, filenames in os.walk(r"c:\GitHub\DX12\Source"):
    if ".git" in dirpath: continue
    for f in filenames:
        if f.endswith((".h", ".hpp")):
            if f not in h_files:
                h_files[f] = []
            h_files[f].append(os.path.join(dirpath, f).replace("\\", "/"))

# Add json.hpp explicitly
h_files["json.hpp"] = ["c:/GitHub/DX12/Library/nlohmann/json.hpp"]

def resolve_include(current_file, include_path):
    current_dir = os.path.dirname(current_file).replace("\\", "/")
    
    # Check if the path already works
    abs_guess = os.path.normpath(os.path.join(current_dir, include_path)).replace("\\", "/")
    if os.path.exists(abs_guess):
        return include_path # Already valid
        
    basename = os.path.basename(include_path)
    if basename not in h_files:
        return include_path # Don't know where it is
        
    candidates = h_files[basename]
    best_candidate = candidates[0]
    if len(candidates) > 1:
        # Pick candidate that shares the most path components with include_path
        inc_parts = include_path.replace("\\", "/").split("/")
        best_score = -1
        for cand in candidates:
            cand_parts = cand.split("/")
            score = 0
            for p in inc_parts:
                if p in cand_parts:
                    score += 1
            if score > best_score:
                best_score = score
                best_candidate = cand
                
    rel_path = os.path.relpath(best_candidate, current_dir).replace("\\", "/")
    if not rel_path.startswith("."):
        rel_path = "./" + rel_path
    if rel_path.startswith("./../"):
        rel_path = rel_path[2:] # simplify
    return rel_path

for dirpath, _, filenames in os.walk(r"c:\GitHub\DX12\Source"):
    if ".git" in dirpath: continue
    for f in filenames:
        if not f.endswith((".cpp", ".h")): continue
        filepath = os.path.join(dirpath, f).replace("\\", "/")
        
        with open(filepath, "r", encoding="cp932") as file:
            lines = file.readlines()
            
        modified = False
        for i, line in enumerate(lines):
            m = re.search(r'#include\s*"(.*?)"', line)
            if m:
                orig_inc = m.group(1)
                new_inc = resolve_include(filepath, orig_inc)
                if orig_inc != new_inc:
                    lines[i] = line.replace('"' + orig_inc + '"', '"' + new_inc + '"')
                    modified = True
                    
        if modified:
            with open(filepath, "w", encoding="cp932") as file:
                file.writelines(lines)

print("Fixed all includes")
