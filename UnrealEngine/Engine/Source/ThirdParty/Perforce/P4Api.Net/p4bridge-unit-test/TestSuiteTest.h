#pragma once
#include "unittestframework.h"
class TestSuiteTest :
    public UnitTestSuite
{
public:
    TestSuiteTest(void);
    ~TestSuiteTest(void);
    DECLARE_TEST_SUITE(TestSuiteTest)

    bool Setup();

    bool TearDown();

    static bool Test1();
    static bool Test2();
    static bool Test3();
};

