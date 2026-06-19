#include "Pch.h"
#include "Logger.h"

Logger& Logger::Instance()
{
    static Logger instance;
    return instance;
}
