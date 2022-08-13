#include "common.hpp"

#include <tsl/sparse_map.h>

template<typename Key, typename Value>
using Map = tsl::sparse_map<Key, Value>;

int main(int argc, char * argv[])
{
    return countWords<Map>(argc, argv);
}
