#include <windows.h>
#include <iostream>

int main() {
    std::cout << (GetAsyncKeyState('W') & 0x8000) << std::endl;
    return 0;
}
