#include "basic.hpp"

#include <absl/container/flat_hash_map.h>

int main(int argc, char * argv[])
{
    return basic<absl::flat_hash_map>(argc, argv);
}
