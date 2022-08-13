#include "common.hpp"

#include <flat_hash_map.hpp>

int main(int argc, char * argv[])
{
    return countWords<ska::flat_hash_map>(argc, argv);
}
