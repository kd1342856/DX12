#include <iostream>
#include <fstream>
#include ""../../../Framework/Manager/Scene.h""
#include ""../../../Framework/Manager/GameManager.h""

int main() {
    GameManager::Instance().Init();
    Scene s;
    s.Init();
    
    nlohmann::json j = nlohmann::json::parse(R""(
    {
        ""GameObjects"": [
            {
                ""Children"": [],
                ""Components"": [
                    {
                        ""IsStatic"": false,
                        ""Shapes"": [
                            {
                                ""ShapeId"": 0,
                                ""Offset"": [1.0, 2.0, 3.0],
                                ""Tags"": 42,
                                ""Width"": 5.0,
                                ""Height"": 6.0,
                                ""Depth"": 7.0
                            }
                        ],
                        ""Type"": ""ColliderData""
                    }
                ],
                ""IsActive"": true,
                ""Name"": ""TestObj""
            }
        ]
    }
    )"");

    s.Deserialize(j);

    nlohmann::json outJ;
    s.Serialize(outJ);
    std::cout << outJ.dump(4) << std::endl;

    return 0;
}
