#include "common.hpp"

#include <tsl/hopscotch_map.h>

template<typename Key, typename Value>
using Map = tsl::hopscotch_map<Key, Value>;

int main(int argc, char * argv[])
{
    return countWords<Map>(argc, argv);
}
