#pragma once
#include "unittestframework.h"
class TestUtils :
    public UnitTestSuite
{
public:
    TestUtils(void);
    ~TestUtils(void);

    DECLARE_TEST_SUITE(TestUtils)

    bool Setup();

    bool TearDown(char* testName);

    static bool TestAllocString();
};

