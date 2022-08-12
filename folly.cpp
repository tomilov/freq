#include "common.hpp"

#include <folly/container/F14Map.h>

int main(int argc, char * argv[])
{
    return countWords<folly::F14ValueMap>(argc, argv);
}
