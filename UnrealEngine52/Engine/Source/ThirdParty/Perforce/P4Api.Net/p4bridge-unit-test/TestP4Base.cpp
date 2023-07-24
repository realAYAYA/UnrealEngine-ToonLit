#include "StdAfx.h"
#include "UnitTestFrameWork.h"
#include "TestP4Base.h"
#include "../p4bridge/p4Base.h"

CREATE_TEST_SUITE(TestP4Base)

TestP4Base::TestP4Base(void)
{
    UnitTestSuite::RegisterTest(&p4BaseSmokeTest, "p4BaseSmokeTest");
}

TestP4Base::~TestP4Base(void)
{
}

bool TestP4Base::Setup()
{
    return true;
}

bool TestP4Base::TearDown(char* testName)
{
	p4base::PrintMemoryState(testName);
    return true;
}

class class0 : public p4base
{
public:
    class0() : p4base(0) {};

    int Type(void) { return  0 ; }
};

class class1 : public p4base
{
public:
    class1() : p4base(1) {};

    int Type(void) { return  1 ; }
};

bool TestP4Base::p4BaseSmokeTest()
{
    // try to validate a NULL pointer, should fail
    class0 * obj0 = NULL;

    int r0 = p4base::ValidateHandle( obj0, 0 );

    ASSERT_EQUAL(r0, 0)

    // try to validate an invalid pointer, should fail
    obj0 = (class0 *) 0x001;

    r0 = p4base::ValidateHandle( obj0, 0 );

    ASSERT_EQUAL(r0, 0)

    // validate a good pointer
    obj0 = new class0();

    r0 = p4base::ValidateHandle( obj0, 0 );

    ASSERT_EQUAL(r0, 1)

    // validate a good pointer of a different class

    class1 * obj1 = new class1();

    r0 = p4base::ValidateHandle( obj1, 1 );

    ASSERT_EQUAL(r0, 1)

    // try to validate a pointer for a deleted object, should fail
    delete obj0;

    r0 = p4base::ValidateHandle( obj0, 0 );

    ASSERT_EQUAL(r0, 0)

    // build up a list of 6 class0 objects
    class0 * obj0a = new class0();
    class0 * obj0b = new class0();
    class0 * obj0c = new class0();
    class0 * obj0d = new class0();
    class0 * obj0e = new class0();
    class0 * obj0f = new class0();

    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0c, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0d, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0e, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0f, 0 ), 1)

    // Delete one in the middle
    delete obj0c;

    // test the handles
    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0c, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0d, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0e, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0f, 0 ), 1)

    // Delete the last one
    delete obj0f;

    // test the handles
    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0b, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0c, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0d, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0e, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0f, 0 ), 0) // should fail

    // Delete the new last one
    delete obj0e;

    // test the handles
    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0b, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0c, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0d, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0e, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0f, 0 ), 0) // should fail

    // Delete the first one
    delete obj0a;

    // test the handles
    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0b, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0c, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0d, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0e, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0f, 0 ), 0) // should fail

    // Delete the new first one
    delete obj0b;

    // test the handles
    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0b, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0c, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0d, 0 ), 1)
    ASSERT_EQUAL(p4base::ValidateHandle( obj0e, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0f, 0 ), 0) // should fail

    // Delete the only one left
    delete obj0d;

    // test the handles
    ASSERT_EQUAL(p4base::ValidateHandle( obj0a, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0b, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0c, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0d, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0e, 0 ), 0) // should fail
    ASSERT_EQUAL(p4base::ValidateHandle( obj0f, 0 ), 0) // should fail

    // good ol' object 1 should still be valid
    r0 = p4base::ValidateHandle( obj1, 1 );

    ASSERT_EQUAL(r0, 1)

	delete obj1;

    return 1; // we passed all tests!
}
