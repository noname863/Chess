#pragma once
#include <string>

void fassert(bool condition, const char * message);

template <typename T>
void criticalAssertEqual(T received, T desired, std::string message)
{
    message += "\nerror code is " + std::to_string(static_cast<int>(received)) + "\n";
    fassert(received == desired, message.c_str());
}

