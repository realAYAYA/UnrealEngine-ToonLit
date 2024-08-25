
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_INTERFACES_RUN_CAPTURE_HPP_INCLUDED
#define CATCH_INTERFACES_RUN_CAPTURE_HPP_INCLUDED

namespace Catch {

    struct TestCaseInfo;

    class IRunCapture {
    public:
        virtual const TestCaseInfo* getCurrentTest() const = 0;
    };

    IRunCapture& getRunCapture();
}

#endif // CATCH_INTERFACES_RUN_CAPTURE_HPP_INCLUDED
