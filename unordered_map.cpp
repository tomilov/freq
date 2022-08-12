#include "common.hpp"

#include <unordered_map>

int main(int argc, char * argv[])
{
    return countWords<std::unordered_map>(argc, argv);
}
