#include "common.hpp"

#include <folly/container/F14Map.h>

int main(int argc, char * argv[])
{
    return count_words<folly::F14ValueMap>(argc, argv);
}
