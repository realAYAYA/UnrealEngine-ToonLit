
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_TEST_REGISTRY_HPP_INCLUDED
#define CATCH_TEST_REGISTRY_HPP_INCLUDED

#include <catch2/internal/catch_source_line_info.hpp>
#include <catch2/internal/catch_noncopyable.hpp>
#include <catch2/interfaces/catch_interfaces_testcase.hpp>
#include <catch2/interfaces/catch_interfaces_group.hpp>
#include <catch2/internal/catch_stringref.hpp>
#include <catch2/internal/catch_unique_ptr.hpp>
#include <catch2/internal/catch_unique_name.hpp>
#include <catch2/internal/catch_preprocessor_remove_parens.hpp>
#include <catch2/internal/catch_constants.hpp>

// GCC 5 and older do not properly handle disabling unused-variable warning
// with a _Pragma. This means that we have to leak the suppression to the
// user code as well :-(
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 5
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif



namespace Catch {

template<typename C>
class TestInvokerAsMethod : public ITestInvoker {
    void (C::*m_testAsMethod)();
public:
    TestInvokerAsMethod( void (C::*testAsMethod)() ) noexcept : m_testAsMethod( testAsMethod ) {}

    void invoke() const override {
        C obj;
        (obj.*m_testAsMethod)();
    }
};

IGroupLifecycleEventInvoker* makeGroupEventInvoker( void(*groupEventAsFunction)(), GroupLifecycleStage stage );

Detail::unique_ptr<ITestInvoker> makeTestInvoker( void(*testAsFunction)() );

template<typename C>
Detail::unique_ptr<ITestInvoker> makeTestInvoker( void (C::*testAsMethod)() ) {
    return Detail::make_unique<TestInvokerAsMethod<C>>( testAsMethod );
}

struct NameAndTags {
    constexpr NameAndTags( StringRef name_ = StringRef(),
                           StringRef tags_ = StringRef() ) noexcept:
        name( name_ ), tags( tags_ ) {}
    StringRef name;
    StringRef tags;
};

struct AutoReg : Detail::NonCopyable {
    AutoReg( Detail::unique_ptr<ITestInvoker> invoker, SourceLineInfo const& lineInfo, StringRef classOrMethod, NameAndTags const& nameAndTags ) noexcept;
    AutoReg( Detail::unique_ptr<ITestInvoker> invoker, SourceLineInfo const& lineInfo, StringRef group, StringRef classOrMethod, NameAndTags const& nameAndTags ) noexcept;
    AutoReg( std::string group, GroupLifecycleStage stage, IGroupLifecycleEventInvoker* invoker) noexcept;
};

} // end namespace Catch

#if defined(CATCH_CONFIG_DISABLE)
    #define INTERNAL_CATCH_TESTCASE_NO_REGISTRATION( TestName, ... ) \
        static inline void TestName()
    #define INTERNAL_CATCH_TESTCASE_METHOD_NO_REGISTRATION( TestName, ClassName, ... ) \
        namespace{                        \
            struct TestName : INTERNAL_CATCH_REMOVE_PARENS(ClassName) { \
                void test();              \
            };                            \
        }                                 \
        void TestName::test()
#endif

    ///////////////////////////////////////////////////////////////////////////////
   #define INTERNAL_CATCH_TESTCASE2( Group, TestName, ... ) \
        static void TestName(); \
        CATCH_INTERNAL_START_WARNINGS_SUPPRESSION \
        CATCH_INTERNAL_SUPPRESS_GLOBALS_WARNINGS \
        namespace{ Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( Catch::makeTestInvoker( &TestName ), CATCH_INTERNAL_LINEINFO, Catch::StringRef(Group), Catch::StringRef(), Catch::NameAndTags{ __VA_ARGS__ } ); } /* NOLINT */ \
        CATCH_INTERNAL_STOP_WARNINGS_SUPPRESSION \
        static void TestName()
    #define INTERNAL_CATCH_TESTCASE_GROUP( Group, ... ) \
        INTERNAL_CATCH_TESTCASE2( Group, INTERNAL_CATCH_UNIQUE_NAME( CATCH2_INTERNAL_TEST_ ), __VA_ARGS__ )
    #define INTERNAL_CATCH_TESTCASE( ... ) \
        INTERNAL_CATCH_TESTCASE2( Catch::DefaultGroup, INTERNAL_CATCH_UNIQUE_NAME( CATCH2_INTERNAL_TEST_ ), __VA_ARGS__ )
    #define INTERNAL_CATCH_TEST_GROUP_EVENT2( Group, Stage, EventName ) \
        static void EventName(); \
        CATCH_INTERNAL_START_WARNINGS_SUPPRESSION \
        CATCH_INTERNAL_SUPPRESS_GLOBALS_WARNINGS \
        namespace{ Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( Group, Stage, Catch::makeGroupEventInvoker( &EventName, Stage ) ); } /* NOLINT */ \
        CATCH_INTERNAL_STOP_WARNINGS_SUPPRESSION \
        static void EventName()
    #define INTERNAL_CATCH_TEST_GROUP_EVENT( Group, Stage ) \
        INTERNAL_CATCH_TEST_GROUP_EVENT2( Group, Stage, INTERNAL_CATCH_UNIQUE_NAME( CATCH2_GROUP_EVENT_ ) )

    ///////////////////////////////////////////////////////////////////////////////
    #define INTERNAL_CATCH_METHOD_AS_TEST_CASE_GROUP( Group, QualifiedMethod, ... ) \
        CATCH_INTERNAL_START_WARNINGS_SUPPRESSION \
        CATCH_INTERNAL_SUPPRESS_GLOBALS_WARNINGS \
        namespace{ Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( Catch::makeTestInvoker( &QualifiedMethod ), CATCH_INTERNAL_LINEINFO, Group, "&" #QualifiedMethod, Catch::NameAndTags{ __VA_ARGS__ } ); } /* NOLINT */ \
        CATCH_INTERNAL_STOP_WARNINGS_SUPPRESSION

	#define INTERNAL_CATCH_METHOD_AS_TEST_CASE( QualifiedMethod, ... ) INTERNAL_CATCH_METHOD_AS_TEST_CASE_GROUP( Catch::DefaultGroup, QualifiedMethod, __VA_ARGS__ )

    ///////////////////////////////////////////////////////////////////////////////
    #define INTERNAL_CATCH_TEST_CASE_METHOD2( Group, TestName, ClassName, ... )\
        CATCH_INTERNAL_START_WARNINGS_SUPPRESSION \
        CATCH_INTERNAL_SUPPRESS_GLOBALS_WARNINGS \
        namespace{ \
            struct TestName : INTERNAL_CATCH_REMOVE_PARENS(ClassName) { \
                void test(); \
            }; \
            Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar ) ( Catch::makeTestInvoker( &TestName::test ), CATCH_INTERNAL_LINEINFO, Group, #ClassName, Catch::NameAndTags{ __VA_ARGS__ } ); /* NOLINT */ \
        } \
        CATCH_INTERNAL_STOP_WARNINGS_SUPPRESSION \
        void TestName::test()

	#define INTERNAL_CATCH_TEST_CASE_METHOD_GROUP( Group, ClassName, ... ) \
			INTERNAL_CATCH_TEST_CASE_METHOD2( Group, INTERNAL_CATCH_UNIQUE_NAME( CATCH2_INTERNAL_TEST_ ), ClassName, __VA_ARGS__ )

	#define INTERNAL_CATCH_TEST_CASE_METHOD( ClassName, ... ) INTERNAL_CATCH_TEST_CASE_METHOD_GROUP( Catch::DefaultGroup, ClassName, __VA_ARGS__ )


    ///////////////////////////////////////////////////////////////////////////////
    #define INTERNAL_CATCH_REGISTER_TESTCASE_GROUP( Group, Function, ... ) \
        do { \
            CATCH_INTERNAL_START_WARNINGS_SUPPRESSION \
            CATCH_INTERNAL_SUPPRESS_GLOBALS_WARNINGS \
            Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( Catch::makeTestInvoker( Function ), CATCH_INTERNAL_LINEINFO, Group, Catch::StringRef(), Catch::NameAndTags{ __VA_ARGS__ } ); /* NOLINT */ \
            CATCH_INTERNAL_STOP_WARNINGS_SUPPRESSION \
        } while(false)

	#define INTERNAL_CATCH_REGISTER_TESTCASE( Function, ... ) INTERNAL_CATCH_REGISTER_TESTCASE_GROUP( Catch::DefaultGroup, Function, __VA_ARGS__ )


#endif // CATCH_TEST_REGISTRY_HPP_INCLUDED
