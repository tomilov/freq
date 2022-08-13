#include "common.hpp"

#include <boost/unordered_map.hpp>

int main(int argc, char * argv[])
{
    return countWords<boost::unordered_map>(argc, argv);
}
