#include "Pch.h"
#include "ClassAssembly.h"

ClassAssembly& ClassAssembly::Instance()
{
    static ClassAssembly instance;
    return instance;
}
