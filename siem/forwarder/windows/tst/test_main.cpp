/**
 * @file test_main.cpp
 * @brief Main entry point for all unit tests
 *
 * Initializes Google Test and runs all test suites
 */

#include <gtest/gtest.h>
#include <iostream>

/**
 * @brief Main function for test runner
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 if all tests pass, 1 if any test fails
 */
int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "Windows Event Log Forwarder - Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Run all tests
    int result = RUN_ALL_TESTS();

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    if (result == 0) {
        std::cout << "All tests passed!" << std::endl;
    } else {
        std::cout << "Some tests failed!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return result;
}
