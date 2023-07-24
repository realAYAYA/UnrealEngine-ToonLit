
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <iostream>
#include <catch2/catch_test_macros.hpp>


////////////////////////////////////////////////////////////////////////////////////////
/* Group events - Default Group */
GROUP_BEFORE_ALL( Catch::DefaultGroup ) { std::cout << "Before All - Default Group\n"; }

GROUP_AFTER_ALL( Catch::DefaultGroup ) { std::cout << "After All - Default Group\n"; }

GROUP_BEFORE_GLOBAL( Catch::DefaultGroup )  { std::cout << "Before Global - Default Group\n"; }

GROUP_AFTER_GLOBAL( Catch::DefaultGroup )  { std::cout << "After Global - Default Group\n"; }

////////////////////////////////////////////////////////////////////////////////////////
/* Group events - Oranges */
GROUP_BEFORE_ALL( "Oranges" ) { std::cout << "Before All - Oranges\n"; }

GROUP_BEFORE_EACH( "Oranges" ) { std::cout << "Before Each - Oranges\n"; }

GROUP_AFTER_EACH( "Oranges" ) { std::cout << "After Each - Oranges\n"; }

GROUP_AFTER_ALL( "Oranges" ) { std::cout << "After All - Oranges\n"; }

GROUP_BEFORE_GLOBAL( "Oranges" )  { std::cout << "Before Global - Oranges\n"; }

GROUP_AFTER_GLOBAL( "Oranges" )  { std::cout << "After Global - Oranges\n"; }

////////////////////////////////////////////////////////////////////////////////////////
/* Group events - Apples */
GROUP_BEFORE_ALL("Apples") { std::cout << "Before All - Apples\n"; }

GROUP_BEFORE_EACH( "Apples" ) { std::cout << "Before Each - Apples\n"; }

GROUP_AFTER_EACH( "Apples" ) { std::cout << "After Each - Apples\n"; }

GROUP_AFTER_ALL( "Apples" ) { std::cout << "After All - Apples\n"; }

GROUP_BEFORE_GLOBAL( "Apples" )  { std::cout << "Before Global - Apples\n"; }

GROUP_AFTER_GLOBAL( "Apples" )  { std::cout << "After Global - Apples\n"; }

////////////////////////////////////////////////////////////////////////////////////////
/* Group test cases, various groups - Should each execute within its group */
GROUP_TEST_CASE( "Apples" )
{
    std::cout << "1 Apples group\n";
    SUCCEED( "anonymous group test case - Apples" );
}

GROUP_TEST_CASE( "Oranges" )
{
    std::cout <<  "1 Oranges group\n";
    SUCCEED( "anonymous group test case - Oranges" );
}

GROUP_TEST_CASE( "Apples", "Test inside Apples group" )
{
    std::cout <<  "2 Apples group\n";
    SUCCEED( "no assertions" );
}

GROUP_TEST_CASE( "Oranges", "Test inside Oranges group" )
{
    std::cout <<  "2 Oranges group\n";
    SUCCEED( "no assertions" );
}

GROUP_TEST_CASE( "Apples", "Test witht tags inside Apples group", "[apples][group]" )
{
    std::cout <<  "3 Apples group\n";
    SECTION( "Section with one argument" )
    {
        SUCCEED( "no assertions" );
    }
}

GROUP_TEST_CASE( "Oranges", "Test witht tags inside Oranges group", "[oranges][group]" )
{
    std::cout <<  "3 Oranges group\n";
    SECTION( "Section with one argument" )
	{
		SUCCEED( "no assertions" );
	}
}
