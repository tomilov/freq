#include "common.hpp"

#if __has_include(<sparsehash/dense_hash_map>)
#include <sparsehash/dense_hash_map>
#elif __has_include(<google/dense_hash_map>)
#include <google/dense_hash_map>
#else
#error !
#endif

int main(int argc, char * argv[])
{
    return countWords<google::dense_hash_map, /* kIsOrdered */ false,
                      /* kSetEmptyKey */ true>(argc, argv);
}
