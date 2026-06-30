# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

# Replace body of DrawHierarchyAndInspector with empty
import re
# We know the function starts at line 151.
# void Editor::DrawHierarchyAndInspector(Scene* scene) {
# ...
# void Editor::DrawHierarchyNode(std::shared_ptr<GameObject> obj) {

part1 = text[:text.find('void Editor::DrawHierarchyAndInspector(Scene* scene) {')]
part2 = text[text.find('void Editor::DrawHierarchyNode(std::shared_ptr<GameObject> obj) {'):]

new_text = part1 + "void Editor::DrawHierarchyAndInspector(Scene* scene) {\n}\n\n" + part2

with open(filepath, "w", encoding="cp932") as f:
    f.write(new_text)
print("Replaced body of DrawHierarchyAndInspector")
