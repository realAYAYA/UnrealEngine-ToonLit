#include "StdAfx.h"
#include "UnitTestFrameWork.h"
#include "TestP4BridgeServerLogging.h"

#include "../p4bridge/P4BridgeClient.h"
#include "../p4bridge/P4BridgeServer.h"

#include <strtable.h>
#include <strarray.h>

CREATE_TEST_SUITE(TestP4BridgeServerLogging)

TestP4BridgeServerLogging::TestP4BridgeServerLogging(void)
{
    UnitTestSuite::RegisterTest(LogMessageTest, "LogMessageTest");
    UnitTestSuite::RegisterTest(BadLogFnPtrTest, "BadLogFnPtrTest");
}

TestP4BridgeServerLogging::~TestP4BridgeServerLogging(void)
{
}

bool TestP4BridgeServerLogging::Setup()
{
    return true;
}

bool TestP4BridgeServerLogging::TearDown(char* testName)
{
	p4base::PrintMemoryState(testName);
    return true;
}

int _stdcall TestP4BridgeServerLogging::LogCallback(int level, const char* file, int line, const char* message)
{
	const char* msg = message;

    if ((level == 3) && (strncmp( msg, "Info", 4 ) == 0))
        return 1;

    if ((level == 0) && (strncmp( msg, "Fatal:1", 7 ) == 0))
        return 1;

    if ((level == 42) && (strncmp( msg, "Debug:42", 8 ) == 0))
        return 1;

    return 0;
}

bool TestP4BridgeServerLogging::LogMessageTest()
{
    P4BridgeServer::SetLogCallFn(LogCallback);

    ASSERT_INT_TRUE(LOG_INFO("Info"));

    ASSERT_INT_TRUE(LOG_FATAL1("Fatal:%s", "1"));

    ASSERT_INT_TRUE(LOG_DEBUG2(42, "Debug:%c%c", '4', '2'));

    return true;
}

bool TestP4BridgeServerLogging::BadLogFnPtrTest()
{
    P4BridgeServer::SetLogCallFn(0);

    ASSERT_FALSE(LOG_INFO("Info"));

    P4BridgeServer::SetLogCallFn((LogCallbackFn*) 0xFFFFFFFF);

    ASSERT_FALSE(LOG_INFO("Info"));

    P4BridgeServer::SetLogCallFn((LogCallbackFn*) 0x123456789ACDEF0);

    ASSERT_FALSE(LOG_INFO("Info"));

    return true;
}