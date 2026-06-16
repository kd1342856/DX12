#include <iostream>
#include ""SimpleMath.h""

int main() {
    DirectX::SimpleMath::Vector3 v(1, 2, 3);
    DirectX::SimpleMath::Matrix m = DirectX::SimpleMath::Matrix::Identity;
    m.m[3][3] = 5.0f;
    DirectX::SimpleMath::Vector4 result;
    DirectX::SimpleMath::Vector3::Transform(v, m, result);
    std::cout << result.x << " " << result.y << " " << result.z << " " << result.w << std::endl;
    return 0;
}
