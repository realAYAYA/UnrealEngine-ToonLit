#pragma once

//#include "unittestframework.h"

class TestP4BridgeServerUtf8 :
    public UnitTestSuite
{
public:
    TestP4BridgeServerUtf8(void);
    ~TestP4BridgeServerUtf8(void);

    DECLARE_TEST_SUITE(TestP4BridgeServerUtf8)

    bool Setup();

    bool TearDown(char* testName);

    static bool ServerConnectionTest();
    static bool TestUntaggedCommand();
    static bool TestNonUnicodeClientToUnicodeServer();
    static bool TestTaggedCommand();
    static bool TestUnicodeUserName();
    static bool TestTextOutCommand();
    static bool TestBinaryOutCommand();
    static bool TestErrorOutCommand();
};

