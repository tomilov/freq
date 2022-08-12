#include "common.hpp"

#include <tsl/robin_map.h>

template<typename Key, typename Value>
using RobinMap =
    tsl::robin_map<Key, Value, std::hash<Key>, std::equal_to<Key>,
                   std::allocator<std::pair<Key, Value>>, true /* StoreHash */>;

int main(int argc, char * argv[])
{
    return countWords<RobinMap>(argc, argv);
}
