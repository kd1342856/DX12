import os
import re

directories = ['C:/GitHub/DX12/Source']

def process_files():
    for root, _, files in os.walk(directories[0]):
        for file in files:
            if file.endswith('.h'):
                h_path = os.path.join(root, file)
                with open(h_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()

                # Find static CLASSNAME& Instance() { static CLASSNAME instance; return instance; }
                pattern = r'static\s+([A-Za-z0-9_]+)\s*&\s*Instance\s*\(\)\s*\{\s*static\s+\1\s+instance\s*;\s*return\s+instance\s*;\s*\}'
                
                def repl(match):
                    cls_name = match.group(1)
                    # Add to .cpp
                    cpp_path = os.path.splitext(h_path)[0] + '.cpp'
                    if os.path.exists(cpp_path):
                        with open(cpp_path, 'a', encoding='utf-8', errors='ignore') as f_cpp:
                            f_cpp.write(f'\n{cls_name}& {cls_name}::Instance()\n{{\n    static {cls_name} instance;\n    return instance;\n}}\n')
                    return f'static {cls_name}& Instance();'

                new_content, count = re.subn(pattern, repl, content)
                if count > 0:
                    with open(h_path, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                    print(f"Fixed {count} instances in {file}")

process_files()
