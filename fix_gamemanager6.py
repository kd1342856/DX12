# coding: utf-8
for fpath in [r"c:\GitHub\DX12\Source\Framework\GameManager.h", r"c:\GitHub\DX12\Source\Framework\GameManager.cpp"]:
    with open(fpath, "r", encoding="cp932") as f:
        text = f.read()

    text = text.replace('"../Utility/', '"Utility/')
    text = text.replace('"../DirectX/', '"Graphics/') # Just in case
    
    with open(fpath, "w", encoding="cp932") as f:
        f.write(text)
print("Fixed GameManager Utility include")
