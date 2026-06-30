# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Application\Application.cpp"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

import re
# Remove ClassAssembly::Instance() dummy
text = re.sub(r'ClassAssembly&\s*ClassAssembly::Instance\(\)\s*\{[^\}]+\}', '', text)
text = re.sub(r'Logger&\s*Logger::Instance\(\)\s*\{[^\}]+\}', '', text)

with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
print("Fixed Application.cpp dummy defs")
