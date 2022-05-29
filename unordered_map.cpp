#include "common.hpp"

#include <unordered_map>

int main(int argc, char * argv[])
{
    return count_words<std::unordered_map>(argc, argv);
}
