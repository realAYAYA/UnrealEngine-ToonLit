// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"



namespace FNameInvalidCharsTest
{

	bool RunTest(const FString InvalidChars,
		const FString& StringViewToTest,
		const FString& ExpectedErrorMessage,
		bool bExpectedIsValid,
		FAutomationTestBase& TestBase)
	{
		FText OutMessage;
		const bool bActualIsValid = FName::IsValidXName(FStringView(StringViewToTest), InvalidChars, &OutMessage);
		const bool bErrorMessageResult = TestBase.TestEqual("Generated error message", OutMessage.ToString(), ExpectedErrorMessage);
		const bool bValidityResult = TestBase.TestEqual("validity return value from FName::IsValidXName", bActualIsValid, bExpectedIsValid);

		return bErrorMessageResult && bValidityResult;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(DoubleBadCharactersTest, "System.Core.NameTypes.Test repeated invalid characters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
bool DoubleBadCharactersTest::RunTest(const FString& Parameters)
{
	return FNameInvalidCharsTest::RunTest(	
		" \n\"\r\''\t\x000B",
		TEXT(" \t\r\n'\"'  \r\n\'\"' \x000B"),
		TEXT(
			"Name may not contain whitespace characters (space, line feed [\\n], carriage return [\\r], tab) or the following characters: \" \' U+000B"
			),
		false,
		*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(SingleInvalidWhitespaceCharacterTest, "System.Core.NameTypes.Test single whitespace character", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
bool SingleInvalidWhitespaceCharacterTest::RunTest(const FString& Parameters)
{
	return FNameInvalidCharsTest::RunTest(
		" \n\"\r\''",
		TEXT("test\nstring"),
		TEXT(
			"Name may not contain whitespace characters (line feed [\\n])"
		),
		false,
		*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(SingleInvalidNonWhitespaceCharacterTest, "System.Core.NameTypes.Test single non-whitespace character", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
bool SingleInvalidNonWhitespaceCharacterTest::RunTest(const FString& Parameters)
{
	return FNameInvalidCharsTest::RunTest(
		" \n\"\r\''",
		TEXT("test\nstring"),
		TEXT("Name may not contain whitespace characters (line feed [\\n])"),
		false,
		*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(InvalidNonWhitespaceCharactersTest, "System.Core.NameTypes.Test invalid whitespace characters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
bool InvalidNonWhitespaceCharactersTest::RunTest(const FString& Parameters)
{
	return FNameInvalidCharsTest::RunTest(
		"\";:QWop \n",
		TEXT("test\";:QWERts \ntringyuiopQWERtstringyuiopqwertstringyuiop"),
		TEXT(
		"Name may not contain whitespace characters (space, line feed [\\n]) or the following characters: \" ; : Q W o p"),
		false,
		*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(ValidFNameTest, "System.Core.NameTypes.Test valid FName", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
bool ValidFNameTest::RunTest(const FString& Parameters)
{
	return FNameInvalidCharsTest::RunTest(
		"\"\n\r",
		TEXT("ValidName"),
		TEXT(""),
		true,
		*this);
}