import xml.etree.ElementTree as ET

FILTERS = r'c:\GitHub\DX12\DX12Framework.vcxproj.filters'

ET.register_namespace("", "http://schemas.microsoft.com/developer/msbuild/2003")
tree = ET.parse(FILTERS)
root = tree.getroot()
ns = {'ns': 'http://schemas.microsoft.com/developer/msbuild/2003'}

for itemgroup in root.findall('ns:ItemGroup', ns):
    for filter_tag in itemgroup.findall('ns:Filter', ns):
        inc = filter_tag.get('Include')
        if inc and inc.startswith('Asset'):
            # Replace Asset with Graphics\Asset or Graphics\Shader
            new_inc = inc.replace('Asset', 'Graphics', 1)
            filter_tag.set('Include', new_inc)
            
    for item in itemgroup:
        if item.tag.endswith('Filter'): continue
        
        filter_node = item.find('ns:Filter', ns)
        if filter_node is not None and filter_node.text and filter_node.text.startswith('Asset'):
            filter_node.text = filter_node.text.replace('Asset', 'Graphics', 1)

tree.write(FILTERS, encoding='utf-8', xml_declaration=True)
print("Updated filters to use Graphics instead of Asset!")
