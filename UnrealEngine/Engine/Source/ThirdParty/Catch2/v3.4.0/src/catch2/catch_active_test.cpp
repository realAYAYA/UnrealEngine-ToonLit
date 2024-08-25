
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <catch2/interfaces/catch_interfaces_run_capture.hpp>
#include <catch2/internal/catch_context.hpp>
#include <catch2/internal/catch_enforce.hpp>

#include <catch2/catch_active_test.hpp>
#include <catch2/catch_test_case_info.hpp>

namespace Catch {

	const std::string getActiveTestName() {
        if ( auto* CurrentTest = getRunCapture().getCurrentTest() )
            return CurrentTest->name;
        else
            CATCH_INTERNAL_ERROR( "No active test instance" );
    }
	
	const std::string getActiveTestTags() {
        if ( auto* CurrentTest = getRunCapture().getCurrentTest() )
            return CurrentTest->tagsAsString();
        else
            CATCH_INTERNAL_ERROR( "No active test instance" );
	}
} // end namespace Catch