#include "common.hpp"

#include <tsl/ordered_map.h>

int main(int argc, char * argv[])
{
    return countWords<tsl::ordered_map>(argc, argv);
}
