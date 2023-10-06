#pragma once

#include "unittestframework.h"
#include "../p4bridge/P4BridgeServer.h"

class TestP4BridgeServer :
    public UnitTestSuite
{
public:
    TestP4BridgeServer(void);
    ~TestP4BridgeServer(void);

    DECLARE_TEST_SUITE(TestP4BridgeServer)

    bool Setup();

    bool TearDown(char* testName);

    static bool ServerConnectionTest();
    static bool TestUnicodeClientToNonUnicodeServer();
    static bool TestUnicodeUserName();
    static bool TestUntaggedCommand();
    static bool TestTaggedCommand();
    static bool TestTextOutCommand();
    static bool TestBinaryOutCommand();
    static bool TestErrorOutCommand();
    static bool TestGetSet();
	static bool TestEnviro();
	static bool TestConnectSetClient();
	static bool TestGetConfig();
    static bool TestIsIgnored();
    static bool TestConnectionManager();
	static bool TestSetVars();
    static bool TestDefaultProgramNameAndVersion();
	static bool TestParallelSync();
	static bool TestParallelSyncCallback();
	static bool TestGetTicketFile();
	static bool TestSetProtocol();
	static bool TestSetTicketFile();

	static int _stdcall LogCallback(int level, const char *file, int line, const char *msg);
};

