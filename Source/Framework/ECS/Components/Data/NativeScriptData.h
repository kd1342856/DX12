#pragma once
#include <memory>
#include <string>
#include "NativeScript.h"

struct NativeScriptData {
    std::shared_ptr<NativeScript> Instance;
    std::string ScriptName; // Used for serialization and editor
};