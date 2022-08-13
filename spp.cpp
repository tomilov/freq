#include "common.hpp"

#include <sparsepp/spp.h>

int main(int argc, char * argv[])
{
    return countWords<spp::sparse_hash_map>(argc, argv);
}
