#include "StdAfx.h"
#include "UnitTestFrameWork.h"
#include <string.h>

#include "TestUtils.h"

#include "..\p4bridge\utils.h"

CREATE_TEST_SUITE(TestUtils)

TestUtils::TestUtils(void)
{
    UnitTestSuite::RegisterTest(&TestAllocString, "TestCopyStr");
}

TestUtils::~TestUtils(void)
{
}

bool TestUtils::Setup()
{
    return true;
}

bool TestUtils::TearDown(char* testName)
{
    return true;
}

bool TestUtils::TestAllocString(void)
{
    const char * pCopy = Utils::AllocString("12345");

    ASSERT_EQUAL(5,strlen(pCopy));

    ASSERT_EQUAL(0,strcmp(pCopy, "12345"));

	Utils::ReleaseString((void*) pCopy);

    return true;
}
