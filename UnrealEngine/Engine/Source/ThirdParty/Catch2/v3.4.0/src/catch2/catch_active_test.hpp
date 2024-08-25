
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_ACTIVE_TEST_HPP_INCLUDED
#define CATCH_ACTIVE_TEST_HPP_INCLUDED

#include <string>

namespace Catch {
   const std::string getActiveTestName();
   const std::string getActiveTestTags();
} // end namespace Catch

#endif // CATCH_ACTIVE_TEST_HPP_INCLUDED
