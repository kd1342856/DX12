# coding: utf-8
import os

def check_file(path):
    # Try reading as purely utf-8
    try:
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
            # If we succeed, check if it's purely ascii
            try:
                text.encode("ascii")
                # Pure ascii is fine, it's the same in utf-8 and cp932
            except UnicodeEncodeError:
                # It has non-ascii characters and is valid utf-8!
                # This means it's a UTF-8 file, which will be mojibake in MSVC (cp932)
                print(f"UTF-8 encoded file (causes mojibake): {path}")
                return True
    except UnicodeDecodeError:
        # Not valid UTF-8, probably valid cp932 already
        pass
    
    # Also let's check for the ? characters from before
    try:
        with open(path, "r", encoding="cp932") as f:
            text = f.read()
            if '\uFFFD' in text or '??' in text or '/* ?' in text:
                print(f"Contains replacement chars / ??: {path}")
    except:
        pass

for root, _, files in os.walk(r"c:\GitHub\DX12\Source"):
    for file in files:
        if file.endswith('.cpp') or file.endswith('.h'):
            check_file(os.path.join(root, file))
