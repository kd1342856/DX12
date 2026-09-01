#pragma once
class NativeScript;

struct NativeScriptData {
    std::shared_ptr<NativeScript> Instance;
    std::string ScriptName; // Used for serialization and editor
};