# coding: utf-8
for fpath in [r"c:\GitHub\DX12\Source\Framework\GameManager.h", r"c:\GitHub\DX12\Source\Framework\GameManager.cpp"]:
    with open(fpath, "r", encoding="cp932") as f:
        text = f.read()

    text = text.replace('"../Collision/', '"Manager/Collision/')
    text = text.replace('"../Scene/', '"Manager/Scene/')
    text = text.replace('"../Audio/', '"Manager/Audio/')
    text = text.replace('"../Animation/', '"Manager/Animation/')
    text = text.replace('"../Resource/', '"Manager/Resource/')
    
    with open(fpath, "w", encoding="cp932") as f:
        f.write(text)
print("Fixed GameManager Manager includes")
