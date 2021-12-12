#include "basic.hpp"

#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tag_and_trait.hpp>
#include <ext/pb_ds/trie_policy.hpp>

template<typename Key, typename Value>
using Trie =
    __gnu_pbds::trie<Key, Value,
                     __gnu_pbds::trie_string_access_traits<Key, 'a', 'z'>,
                     __gnu_pbds::pat_trie_tag, __gnu_pbds::null_node_update>;

int main(int argc, char * argv[])
{
    return basic<Trie, /* kIsOrdered */ true>(argc, argv);
}
