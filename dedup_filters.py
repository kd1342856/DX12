import xml.etree.ElementTree as ET

FILTERS = r'c:\GitHub\DX12\DX12Framework.vcxproj.filters'

ET.register_namespace("", "http://schemas.microsoft.com/developer/msbuild/2003")
tree = ET.parse(FILTERS)
root = tree.getroot()
ns = {'ns': 'http://schemas.microsoft.com/developer/msbuild/2003'}

# Find the first ItemGroup containing filters
filter_itemgroup = None
seen_filters = set()
to_remove = []

for itemgroup in root.findall('ns:ItemGroup', ns):
    for filter_tag in itemgroup.findall('ns:Filter', ns):
        if filter_itemgroup is None:
            filter_itemgroup = itemgroup
            
        inc = filter_tag.get('Include')
        if inc in seen_filters:
            to_remove.append((itemgroup, filter_tag))
        else:
            seen_filters.add(inc)

for parent, child in to_remove:
    parent.remove(child)

tree.write(FILTERS, encoding='utf-8', xml_declaration=True)
print("Deduplicated filters!")
