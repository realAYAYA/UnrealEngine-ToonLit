// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AutomationTest.h"

struct FNoDiscardAsserter
{
protected:
	FAutomationTestBase& TestRunner;

public:
	explicit FNoDiscardAsserter(FAutomationTestBase& TestRunner)
		: TestRunner(TestRunner) {}

	void ExpectError(FString Error, int32 Count = 1);
	void ExpectErrorRegex(FString Error, int32 Count = 1);

	void Fail(FString error);

	UE_NODISCARD bool IsTrue(bool Condition);
	UE_NODISCARD bool IsTrue(bool Condition, const char* FailureMessage);
	UE_NODISCARD bool IsTrue(bool Condition, const TCHAR* FailureMessage);
	UE_NODISCARD bool IsTrue(bool Condition, const FString& FailureMessage);

	UE_NODISCARD bool IsFalse(bool Condition);
	UE_NODISCARD bool IsFalse(bool Condition, const char* FailureMessage);
	UE_NODISCARD bool IsFalse(bool Condition, const TCHAR* FailureMessage);
	UE_NODISCARD bool IsFalse(bool Condition, const FString& FailureMessage);

	template <typename T>
	UE_NODISCARD bool IsNull(const T& Ptr);

	template <typename T>
	UE_NODISCARD bool IsNull(const T& Ptr, const char* FailureMessage);

	template <typename T>
	UE_NODISCARD bool IsNull(const T& Ptr, const TCHAR* FailureMessage);

	template <typename T>
	UE_NODISCARD bool IsNull(const T& Ptr, const FString& FailureMessage);

	template <typename T>
	UE_NODISCARD bool IsNotNull(const T& Ptr);

	template <typename T>
	UE_NODISCARD bool IsNotNull(const T& Ptr, const char* FailureMessage);

	template <typename T>
	UE_NODISCARD bool IsNotNull(const T& Ptr, const TCHAR* FailureMessage);

	template <typename T>
	UE_NODISCARD bool IsNotNull(const T& Ptr, const FString& FailureMessage);

	template <typename TExpected, typename TActual>
	UE_NODISCARD bool AreEqual(const TExpected& Expected, const TActual& Actual);

	template <typename TExpected, typename TActual>
	UE_NODISCARD bool AreEqual(const TExpected& Expected, const TActual& Actual, const char* FailureMessage);

	template <typename TExpected, typename TActual>
	UE_NODISCARD bool AreEqual(const TExpected& Expected, const TActual& Actual, const TCHAR* FailureMessage);

	template <typename TExpected, typename TActual>
	UE_NODISCARD bool AreEqual(const TExpected& Expected, const TActual& Actual, const FString& FailureMessage);

	UE_NODISCARD bool AreEqualIgnoreCase(const FString& Expected, const FString& Actual);

	UE_NODISCARD bool AreEqualIgnoreCase(const FString& Expected, const FString& Actual, const char* FailureMessage);

	UE_NODISCARD bool AreEqualIgnoreCase(const FString& Expected, const FString& Actual, const TCHAR* FailureMessage);

	UE_NODISCARD bool AreEqualIgnoreCase(const FString& Expected, const FString& Actual, const FString& FailureMessage);

	template <typename TExpected, typename TActual>
	UE_NODISCARD bool AreNotEqual(const TExpected& Expected, const TActual& Actual);

	template <typename TExpected, typename TActual>
	UE_NODISCARD bool AreNotEqual(const TExpected& Expected, const TActual& Actual, const char* FailureMessage);

	template <typename TExpected, typename TActual>
	UE_NODISCARD bool AreNotEqual(const TExpected& Expected, const TActual& Actual, const TCHAR* FailureMessage);

	template <typename TExpected, typename TActual>
	UE_NODISCARD bool AreNotEqual(const TExpected& Expected, const TActual& Actual, const FString& FailureMessage);

	UE_NODISCARD bool AreNotEqualIgnoreCase(const FString& Expected, const FString& Actual);

	UE_NODISCARD bool AreNotEqualIgnoreCase(const FString& Expected, const FString& Actual, const char* FailureMessage);

	UE_NODISCARD bool AreNotEqualIgnoreCase(const FString& Expected, const FString& Actual, const TCHAR* FailureMessage);

	UE_NODISCARD bool AreNotEqualIgnoreCase(const FString& Expected, const FString& Actual, const FString& FailureMessage);

	template <typename TExpected, typename TActual, typename TEpsilon>
	UE_NODISCARD bool IsNear(TExpected Expected, TActual Actual, TEpsilon Epsilon);

	template <typename TExpected, typename TActual, typename TEpsilon>
	UE_NODISCARD bool IsNear(TExpected Expected, TActual Actual, TEpsilon Epsilon, const char* FailureMessage);

	template <typename TExpected, typename TActual, typename TEpsilon>
	UE_NODISCARD bool IsNear(TExpected Expected, TActual Actual, TEpsilon Epsilon, const TCHAR* FailureMessage);

	template <typename TExpected, typename TActual, typename TEpsilon>
	UE_NODISCARD bool IsNear(TExpected Expected, TActual Actual, TEpsilon Epsilon, const FString& FailureMessage);

};

#include "Assert/NoDiscardAsserter.inl"