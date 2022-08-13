#include "common.hpp"

#include <emilib/hash_map.hpp>

int main(int argc, char * argv[])
{
    return countWords<emilib::HashMap>(argc, argv);
}
