#include "basic.hpp"

#include <folly/container/F14Map.h>

int main(int argc, char * argv[])
{
    return basic<folly::F14ValueMap>(argc, argv);
}
