# coding: utf-8
filepath = r"c:\GitHub\DX12\Source\Framework\Editor\Editor.cpp"
with open(filepath, "r", encoding="cp932") as f:
    text = f.read()

# Replace the block with the extra bracket
old_block = """                            } else {
                                ImGui::Text("No animations in model.");
                            }
                        }
                    } else {
                        ImGui::InputInt("Animation Index", &animData.currentAnim.AnimationIndex);
                    }"""

new_block = """                            } else {
                                ImGui::Text("No animations in model.");
                            }
                    } else {
                        ImGui::InputInt("Animation Index", &animData.currentAnim.AnimationIndex);
                    }"""

text = text.replace(old_block, new_block)

with open(filepath, "w", encoding="cp932") as f:
    f.write(text)
print("Fixed Editor.cpp bracket")
