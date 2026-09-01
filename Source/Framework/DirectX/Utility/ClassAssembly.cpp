#include "ClassAssembly.h"

ClassAssembly& ClassAssembly::Instance()
{
    static ClassAssembly instance;
    return instance;
}
