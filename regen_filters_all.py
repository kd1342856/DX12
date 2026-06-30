import os
import xml.etree.ElementTree as ET

PROJ = r'c:\GitHub\DX12\DX12Framework.vcxproj'
FILTERS = r'c:\GitHub\DX12\DX12Framework.vcxproj.filters'

ET.register_namespace("", "http://schemas.microsoft.com/developer/msbuild/2003")
tree = ET.parse(PROJ)
root = tree.getroot()
ns = {'ns': 'http://schemas.microsoft.com/developer/msbuild/2003'}

# Extract all file includes
all_files = []
items = {}

for itemgroup in root.findall('ns:ItemGroup', ns):
    for child in itemgroup:
        inc = child.get('Include')
        if inc:
            all_files.append(inc)
            if child.tag not in items:
                items[child.tag] = []
            items[child.tag].append(inc)

# Process folders
folders = set()
for f in all_files:
    d = os.path.dirname(f)
    if d.startswith('Source\\'):
        d = d[7:]
    elif d == 'Source':
        d = ''
    if not d: continue
    parts = d.split('\\')
    for i in range(1, len(parts) + 1):
        folders.add('\\'.join(parts[:i]))

filters_out = []
filters_out.append('<?xml version="1.0" encoding="utf-8"?>\n')
filters_out.append('<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n')

filters_out.append('  <ItemGroup>\n')
for folder in sorted(folders):
    filters_out.append(f'    <Filter Include="{folder}">\n')
    filters_out.append(f'      <UniqueIdentifier>{{{os.urandom(16).hex()[:8]}-0000-0000-0000-000000000000}}</UniqueIdentifier>\n')
    filters_out.append(f'    </Filter>\n')
filters_out.append('  </ItemGroup>\n')

for tag, files in items.items():
    tag_name = tag.split('}')[-1]
    # We ignore ProjectConfiguration in filters
    if tag_name == 'ProjectConfiguration':
        continue
    filters_out.append('  <ItemGroup>\n')
    for f in files:
        d = os.path.dirname(f)
        if d.startswith('Source\\'):
            d = d[7:]
        elif d == 'Source':
            d = ''
            
        if d:
            filters_out.append(f'    <{tag_name} Include="{f}">\n')
            filters_out.append(f'      <Filter>{d}</Filter>\n')
            filters_out.append(f'    </{tag_name}>\n')
        else:
            filters_out.append(f'    <{tag_name} Include="{f}" />\n')
    filters_out.append('  </ItemGroup>\n')

filters_out.append('</Project>\n')

with open(FILTERS, 'w', encoding='utf-8') as f:
    f.writelines(filters_out)

print("Regenerated all filters!")
