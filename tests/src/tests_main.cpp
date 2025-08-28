#include "logger.hpp"
#include <gtest/gtest.h>

int main(int argc, char** argv) 
{
    ::testing::InitGoogleTest(&argc, argv);
    Logger::init();
    return RUN_ALL_TESTS();
}