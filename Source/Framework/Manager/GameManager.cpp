#include "GameManager.h"

// GameManager ‚Ì static ƒƒ“ƒo•Ï”‚Ì’è‹`
bool GameManager::s_alive = true;
GameManager& GameManager::Instance()
{
    static GameManager instance;
    return instance;
}
