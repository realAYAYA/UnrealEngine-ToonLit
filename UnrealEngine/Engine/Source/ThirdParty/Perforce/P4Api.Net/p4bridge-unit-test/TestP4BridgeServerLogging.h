#pragma once

#include "unittestframework.h"

class TestP4BridgeServerLogging :
    public UnitTestSuite
{
public:
    TestP4BridgeServerLogging(void);
    ~TestP4BridgeServerLogging(void);

    DECLARE_TEST_SUITE(TestP4BridgeServerLogging)

    bool Setup();

    bool TearDown(char* testName);

    static int _stdcall LogCallback(int level, const char* file, int line, const char* message);

    static bool LogMessageTest();
    static bool BadLogFnPtrTest();
};

