import xml.etree.ElementTree as ET
import os
import uuid

vcxproj_path = r'c:\GitHub\DX12\DX12Framework.vcxproj'
filters_path = r'c:\GitHub\DX12\DX12Framework.vcxproj.filters'

ET.register_namespace('', 'http://schemas.microsoft.com/developer/msbuild/2003')
tree = ET.parse(vcxproj_path)
root = tree.getroot()

ns = {'ms': 'http://schemas.microsoft.com/developer/msbuild/2003'}

files = []
for itemgroup in root.findall('ms:ItemGroup', ns):
    for tag_name in ['ClCompile', 'ClInclude', 'None', 'FxCompile']:
        for elem in itemgroup.findall(f'ms:{tag_name}', ns):
            if 'Include' in elem.attrib:
                files.append((tag_name, elem.attrib['Include']))

filters_set = set()
for _, fpath in files:
    d = os.path.dirname(fpath)
    if d:
        parts = d.split('\\')
        for i in range(1, len(parts) + 1):
            filters_set.add('\\'.join(parts[:i]))

def get_uuid():
    return '{' + str(uuid.uuid4()).upper() + '}'

with open(filters_path, 'w', encoding='utf-8') as out:
    out.write('<?xml version="1.0" encoding="utf-8"?>\n')
    out.write('<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n')
    
    out.write('  <ItemGroup>\n')
    for f in sorted(list(filters_set)):
        out.write(f'    <Filter Include="{f}">\n')
        out.write(f'      <UniqueIdentifier>{get_uuid()}</UniqueIdentifier>\n')
        out.write('    </Filter>\n')
    out.write('  </ItemGroup>\n')
    
    out.write('  <ItemGroup>\n')
    for tag, fpath in sorted(files, key=lambda x: (x[0], x[1])):
        d = os.path.dirname(fpath)
        if d:
            out.write(f'    <{tag} Include="{fpath}">\n')
            out.write(f'      <Filter>{d}</Filter>\n')
            out.write(f'    </{tag}>\n')
        else:
            out.write(f'    <{tag} Include="{fpath}" />\n')
    out.write('  </ItemGroup>\n')
    out.write('</Project>\n')

print('Filters regenerated perfectly!')
