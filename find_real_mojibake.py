# coding: utf-8
import os

def check_file(path):
    try:
        with open(path, "r", encoding="cp932") as f:
            text = f.read()
            # „Å is the classic UTF-8 mojibake for Hiragana/Kanji
            #  is the replacement character
            if '„Å' in text or '' in text or 'Êñ' in text or '' in text or 'Êõ' in text or 'ÅE' in text:
                print(f"Mojibake found in: {path}")
                return True
    except:
        pass
    return False

for root, _, files in os.walk(r"c:\GitHub\DX12\Source"):
    for file in files:
        if file.endswith('.cpp') or file.endswith('.h'):
            check_file(os.path.join(root, file))
