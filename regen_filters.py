import os
import xml.etree.ElementTree as ET

PROJ = r'c:\GitHub\DX12\DX12Framework.vcxproj'
FILTERS = r'c:\GitHub\DX12\DX12Framework.vcxproj.filters'
SRC_DIR = r'c:\GitHub\DX12\Source'
LIB_DIR = r'c:\GitHub\DX12\Library'

def get_files(base_dir, exts):
    res = []
    for root, dirs, files in os.walk(base_dir):
        for f in files:
            if any(f.endswith(ext) for ext in exts):
                rel = os.path.relpath(os.path.join(root, f), r'c:\GitHub\DX12')
                res.append(rel)
    return res

src_cpp = get_files(SRC_DIR, ['.cpp'])
src_h = get_files(SRC_DIR, ['.h'])
lib_cpp = get_files(LIB_DIR, ['.cpp'])
lib_h = get_files(LIB_DIR, ['.h'])

# Generate .filters with "Source\" prefix removed from Filter names
all_files = src_cpp + src_h + lib_cpp + lib_h
folders = set()
for f in all_files:
    d = os.path.dirname(f)
    if d.startswith('Source\\'):
        d = d[7:]
    if d.startswith('Source'):
        d = d[6:]
    if not d: continue
    
    parts = d.split(os.sep)
    for i in range(1, len(parts) + 1):
        folders.add(os.sep.join(parts[:i]))

filters_out = []
filters_out.append('<?xml version="1.0" encoding="utf-8"?>\n')
filters_out.append('<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n')
filters_out.append('  <ItemGroup>\n')
for folder in sorted(folders):
    if not folder: continue
    filters_out.append(f'    <Filter Include="{folder}">\n')
    filters_out.append(f'      <UniqueIdentifier>{{{os.urandom(16).hex()[:8]}-0000-0000-0000-000000000000}}</UniqueIdentifier>\n')
    filters_out.append(f'    </Filter>\n')
filters_out.append('  </ItemGroup>\n')

filters_out.append('  <ItemGroup>\n')
for f in src_cpp + lib_cpp:
    filters_out.append(f'    <ClCompile Include="{f}">\n')
    d = os.path.dirname(f)
    if d.startswith('Source\\'):
        d = d[7:]
    elif d == 'Source':
        d = ''
    if d:
        filters_out.append(f'      <Filter>{d}</Filter>\n')
    filters_out.append(f'    </ClCompile>\n')
filters_out.append('  </ItemGroup>\n')

filters_out.append('  <ItemGroup>\n')
for f in src_h + lib_h:
    filters_out.append(f'    <ClInclude Include="{f}">\n')
    d = os.path.dirname(f)
    if d.startswith('Source\\'):
        d = d[7:]
    elif d == 'Source':
        d = ''
    if d:
        filters_out.append(f'      <Filter>{d}</Filter>\n')
    filters_out.append(f'    </ClInclude>\n')
filters_out.append('  </ItemGroup>\n')
filters_out.append('</Project>\n')

with open(FILTERS, 'w', encoding='utf-8') as f:
    f.writelines(filters_out)

print("Regenerated cleanly via XML ElementTree!")
