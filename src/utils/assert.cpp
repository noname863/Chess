#include <utils/assert.hpp>

void fassert(bool condition, const char * message)
{
    if (!condition)
    {
        puts(message);
        abort();
    }
}
