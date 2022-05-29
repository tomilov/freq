#include "common.hpp"

#if __has_include(<sparsehash/sparse_hash_map>)
#include <sparsehash/sparse_hash_map>
#elif __has_include(<google/sparse_hash_map>)
#include <google/sparse_hash_map>
#else
#error !
#endif

int main(int argc, char * argv[])
{
    return count_words<google::sparse_hash_map>(argc, argv);
}
