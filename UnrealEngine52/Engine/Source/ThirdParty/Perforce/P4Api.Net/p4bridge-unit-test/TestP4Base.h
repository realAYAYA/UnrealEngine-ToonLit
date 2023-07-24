#pragma once
#include "UnitTestFrameWork.h"

class TestP4Base :
    public UnitTestSuite
{
public:
    TestP4Base(void);
    ~TestP4Base(void);

    DECLARE_TEST_SUITE(TestP4Base)

    bool Setup();

    bool TearDown(char* testName);

    static bool p4BaseSmokeTest();
};

