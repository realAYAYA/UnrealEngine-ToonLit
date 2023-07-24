#include "StdAfx.h"
#include "TestSuiteTest.h"

CREATE_TEST_SUITE(TestSuiteTest)

TestSuiteTest::TestSuiteTest(void)
{
    UnitTestSuite::RegisterTest(&Test1, "Test1");
    UnitTestSuite::RegisterTest(&Test2, "Test2");
    UnitTestSuite::RegisterTest(&Test3, "Test3");
}

TestSuiteTest::~TestSuiteTest(void)
{
}

bool TestSuiteTest::Setup()
{
    printf("\tSetting up for the a TestSuiteTest\r\n");
    return true;
}

bool TestSuiteTest::TearDown()
{
    printf("\tTearing down after the a TestSuiteTest\r\n");
    return true;
}

bool TestSuiteTest::Test1(void)
{
    printf("\tRunning Test 1!\r\n");

    ASSERT_EQUAL(1,1);

    return true;
}

bool TestSuiteTest::Test2(void)
{
    printf("\t Test 2 Always fails!\r\n");

    ASSERT_NOT_EQUAL(1,1);

    return true;
}

bool TestSuiteTest::Test3(void)
{
    printf("\tShouldn't run Test 3!\r\n");

    return true;
}

