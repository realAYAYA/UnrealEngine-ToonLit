#pragma once
#include "unittestframework.h"

class TestP4BridgeClient :
    public UnitTestSuite
{
public:
    TestP4BridgeClient(void);
    ~TestP4BridgeClient(void);

    DECLARE_TEST_SUITE(TestP4BridgeClient)

    bool Setup();

    bool TearDown(char* testName);

    static bool HandleErrorTest();
    static bool OutputInfoTest();
    static bool OutputTextTest();
    static bool OutputBinaryTest();
    static bool OutputStatTest();

    static bool HandleErrorCallbackTest();
    static bool OutputInfoCallbackTest();
    static bool OutputTextCallbackTest();
    static bool OutputBinaryCallbackTest();
    static bool OutputStatCallbackTest();

    static bool PromptCallbackTest();
};

