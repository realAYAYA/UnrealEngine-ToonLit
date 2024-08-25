// Copyright Epic Games, Inc. All Rights Reserved.


#include "Engine/Level.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/PackageName.h"
#include "GameMapsSettings.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "Serialization/MemoryReader.h"
#include "Tests/AutomationTestSettings.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

#include "Tests/AutomationCommon.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "GameFramework/DefaultPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UWorld* GetSimpleEngineAutomationTestGameWorld(const int32 TestFlags)
	{
		// Accessing the game world is only valid for game-only 
		check((TestFlags & EAutomationTestFlags::ApplicationContextMask) == EAutomationTestFlags::ClientContext);
		check(GEngine->GetWorldContexts().Num() == 1);
		check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

		return GEngine->GetWorldContexts()[0].World();
	}

	/**
	* Populates the test names and commands for complex tests that are ran on all available maps
	*
	* @param OutBeautifiedNames - The list of map names
	* @param OutTestCommands - The list of commands for each test (The file names in this case)
	*/
	void PopulateTestsForAllAvailableMaps(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
	{
		TArray<FString> FileList;
#if WITH_EDITOR
		FEditorFileUtils::FindAllPackageFiles(FileList);
#else
		// Look directly on disk. Very slow!
		FPackageName::FindPackagesInDirectory(FileList, *FPaths::ProjectContentDir());
#endif

		// Iterate over all files, adding the ones with the map extension..
		for (int32 FileIndex = 0; FileIndex < FileList.Num(); FileIndex++)
		{
			const FString& Filename = FileList[FileIndex];

			// Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
			if (FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension())
			{
				if (FAutomationTestFramework::Get().ShouldTestContent(Filename))
				{
					OutBeautifiedNames.Add(FPaths::GetBaseFilename(Filename));
					OutTestCommands.Add(Filename);
				}
			}
		}
	}
}

#if PLATFORM_DESKTOP

/**
 * SetRes Verification - Verify changing resolution works
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSetResTest, "System.Windows.Set Resolution", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

/** 
 * Change resolutions, wait, and change back
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FSetResTest::RunTest(const FString& Parameters)
{

	//Gets the default map that the game uses.
	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
	const FString& MapName = GameMapsSettings->GetGameDefaultMap();

	//Opens the actual default map in game.
	GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));

	//Gets the current resolution.
	int32 ResX = GSystemResolution.ResX;
	int32 ResY = GSystemResolution.ResY;
	FString RestoreResolutionString = FString::Printf(TEXT("setres %dx%d"), ResX, ResY);

	//Change the resolution and then restore it.
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 640x480")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(RestoreResolutionString));
	return true;
}

#endif

/**
 * Stats verification - Toggle various "stats" commands
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsVerificationMapTest, "System.Maps.Stats Verification", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

/** 
 * Execute the loading of one map to verify screen captures and performance captures work
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FStatsVerificationMapTest::RunTest(const FString& Parameters)
{
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	check(AutomationTestSettings);

	if ( AutomationTestSettings->AutomationTestmap.IsValid() )
	{
		FString MapName = AutomationTestSettings->AutomationTestmap.GetLongPackageName();

		GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));
	}
	else
	{
		UE_LOG(LogEngineAutomationTests, Log, TEXT("Automation test map doesn't exist or is not set: %s.  \nUsing the currently loaded map."), *AutomationTestSettings->AutomationTestmap.GetLongPackageName());
	}

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat game")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat game")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat scenerendering")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat scenerendering")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat memory")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat memory")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat slate")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat slate")));

	return true;
}

/**
 * Latent command to take a screenshot of the viewport
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTakeViewportScreenshotCommand, FString, ScreenshotFileName);

bool FTakeViewportScreenshotCommand::Update()
{
	const bool bShowUI = false;
	const bool bAddFilenameSuffix = false;
	FScreenshotRequest::RequestScreenshot( ScreenshotFileName, bShowUI, bAddFilenameSuffix );
	return true;
}

/**
 * SaveGameTest
 * Test makes sure a save game (without UI) saves and loads correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveGameTest, "System.Engine.Game.Noninteractive Save", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

/** 
 * Saves and loads a savegame file
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FSaveGameTest::RunTest(const FString& Parameters)
{
	// automation save name
	const TCHAR* SaveName = TEXT("AutomationSaveTest");
	uint32 SavedData = 99;

	// the blob we are going to write out
	TArray<uint8> Blob;
	FMemoryWriter WriteAr(Blob);
	WriteAr << SavedData;

	// get the platform's save system
	ISaveGameSystem* Save = IPlatformFeaturesModule::Get().GetSaveGameSystem();

	// write it out
	if (Save->SaveGame(false, SaveName, 0, Blob) == false)
	{
		return false;
	}

	// make sure it was written
	if (Save->DoesSaveGameExist(SaveName, 0) == false)
	{
		return false;
	}

	// read it back in
	Blob.Empty();
	if (Save->LoadGame(false, SaveName, 0, Blob) == false)
	{
		return false;
	}

	// make sure it's the same data
	FMemoryReader ReadAr(Blob);
	uint32 LoadedData;
	ReadAr << LoadedData;

	// try to delete it (not all platforms can)
	if (Save->DeleteGame(false, SaveName, 0))
	{
		// make sure it's no longer there
		if (Save->DoesSaveGameExist(SaveName, 0) == true)
		{
			return false;
		}
	}

	return LoadedData == SavedData;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationLogAddMessage, "TestFramework.Log.Add Log Message", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAutomationLogAddMessage::RunTest(const FString& Parameters)
{
	//** TEST **//
	AddInfo(TEXT("Test log message."));

	//** VERIFY **//
	TestEqual<FString>(TEXT("Test log message was not added to the ExecutionInfo.Log array."), ExecutionInfo.GetEntries().Last().Event.Message, TEXT("Test log message."));
	
	//** TEARDOWN **//
	// We have to empty this log array so that it doesn't show in the automation results window as it may cause confusion.
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Info);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationLogAddWarning, "TestFramework.Log.Add Warning Message", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAutomationLogAddWarning::RunTest(const FString& Parameters)
{
	//** TEST **//
	AddWarning(TEXT("Test warning message."));

	//** VERIFY **//
	FString CurrentWarningMessage = ExecutionInfo.GetEntries().Last().Event.Message;
	// The warnings array is emptied so that it doesn't cause a false positive warning for this test.
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Warning);

	TestEqual<FString>(TEXT("Test warning message was not added to the ExecutionInfo.Warning array."), CurrentWarningMessage, TEXT("Test warning message."));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationLogAddError, "TestFramework.Log.Add Error Message", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAutomationLogAddError::RunTest(const FString& Parameters)
{
	//** TEST **//
	AddError(TEXT("Test error message"));
	
	//** VERIFY **//
	FString CurrentErrorMessage = ExecutionInfo.GetEntries().Last().Event.Message;
	// The errors array is emptied so that this doesn't cause a false positive failure for this test.
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Error);

	TestEqual<FString>(TEXT("Test error message was not added to the ExecutionInfo.Error array."), CurrentErrorMessage, TEXT("Test error message"));
	
	return true;
}

class FAutomationNearlyEqualTest : public FAutomationTestBase
{
public:
	FAutomationNearlyEqualTest(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

protected:
	template<typename T>
	int32 RunSimpleTest(const FString& What, const T ActualValue, const T ExpectedValue, float Tolerance)
	{
		int32 CasesCheckedTotal = 0;

		TestNearlyEqual(What, ActualValue, ExpectedValue, Tolerance);
		++CasesCheckedTotal;

		return CasesCheckedTotal;
	}

	// Note: this method is used to avoid rising of error C2666 2 overloads have similar conversions
	template<>
	int32 RunSimpleTest<double>(const FString& What, double ActualValue, double ExpectedValue, float Tolerance)
	{
		int32 CasesCheckedTotal = 0;

		TestNearlyEqual(What, ActualValue, ExpectedValue, static_cast<double>(Tolerance));
		++CasesCheckedTotal;

		return CasesCheckedTotal;
	}

	int32 RunFloatMutationTest(const FString& WhatPrefix, float BaseValue, float Difference, float Tolerance)
	{
		check(Difference != 0.f);
		check(Tolerance > 0.f);

		int32 CasesCheckedTotal = 0;

		// Perform tests with mutated values
		TestNearlyEqual(FString::Format(*ActualValueIsIncreasedByFormatString, { *WhatPrefix, Difference }),
			BaseValue + Difference, BaseValue, Tolerance);
		++CasesCheckedTotal;

		TestNearlyEqual(FString::Format(*ExpectedValueIsIncreasedByFormatString, { *WhatPrefix, Difference }),
			BaseValue, BaseValue + Difference, Tolerance);
		++CasesCheckedTotal;

		return CasesCheckedTotal;
	}

	int32 RunDoubleMutationTest(const FString& WhatPrefix, double BaseValue, double Difference, float Tolerance)
	{
		check(Difference != 0.f);
		check(Tolerance > 0.f);

		int32 CasesCheckedTotal = 0;

		// Perform tests with mutated values
		TestNearlyEqual(FString::Format(*ActualValueIsIncreasedByFormatString, { *WhatPrefix, Difference }),
			BaseValue + Difference, BaseValue, static_cast<double>(Tolerance));
		++CasesCheckedTotal;
		TestNearlyEqual(FString::Format(*ExpectedValueIsIncreasedByFormatString, { *WhatPrefix, Difference }),
			BaseValue, BaseValue + Difference, static_cast<double>(Tolerance));
		++CasesCheckedTotal;

		return CasesCheckedTotal;
	}

	using GetWhatCallable = TFunction<FString(const FString& WhatPrefix, uint32 ActualValueMutationBitMask, 
		uint32 ExpectedValueMutationBitMask, double Difference)>;

	template<typename T>
	using GetMutatedValueCallable = TFunction<T(const T& BaseValue, uint32 MutationBitMask, double Difference)>;

	int32 RunFVectorMutationTest(const FString& WhatPrefix, const FVector& BaseValue, double Difference, float Tolerance)
	{
		GetWhatCallable GetWhatCallableImpl = [](const FString& WhatPrefix, uint32 ActualValueMutationBitMask, uint32 ExpectedValueMutationBitMask, double Difference)
		{
			return FString::Printf(
				TEXT(
					"%s: the actual FVector value is not nearly equal to the expected FVector value\n"
					"(mutation mask for actual value is (%c, %c, %c), mutation mask for expected value is (%c, %c, %c), values were increased by %f)"
				),
				*WhatPrefix,
				GetNthBitAsChar(ActualValueMutationBitMask, 2),
				GetNthBitAsChar(ActualValueMutationBitMask, 1),
				GetNthBitAsChar(ActualValueMutationBitMask, 0),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 2),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 1),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 0),
				Difference);
		};

		GetMutatedValueCallable<FVector> GetMutatedValueCallableImpl = [](const FVector& BaseValue, uint32 MutationBitMask, double Difference)
		{
			return FVector(
				BaseValue.X + GetNthBitAsUInt32(MutationBitMask, 2) * Difference,
				BaseValue.Y + GetNthBitAsUInt32(MutationBitMask, 1) * Difference,
				BaseValue.Z + GetNthBitAsUInt32(MutationBitMask, 0) * Difference
			);
		};

		return RunMutationTestImpl<FVector>(WhatPrefix, BaseValue, MaxFVectorMutationBitMask, Difference, Tolerance,
			GetWhatCallableImpl, GetMutatedValueCallableImpl);
	}

	int32 RunFRotatorMutationTest(const FString& WhatPrefix, const FRotator& BaseValue, double Difference, float Tolerance)
	{
		GetWhatCallable GetWhatCallableImpl = [](const FString& WhatPrefix, uint32 ActualValueMutationBitMask, uint32 ExpectedValueMutationBitMask, double Difference)
		{
			return FString::Printf(
				TEXT(
					"%s: the actual FRotator value is not nearly equal to the expected FRotator value\n"
					"(mutation mask for actual value is (%c, %c, %c), mutation mask for expected value is (%c, %c, %c), values were increased by %f)"
				),
				*WhatPrefix,
				GetNthBitAsChar(ActualValueMutationBitMask, 2),
				GetNthBitAsChar(ActualValueMutationBitMask, 1),
				GetNthBitAsChar(ActualValueMutationBitMask, 0),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 2),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 1),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 0),
				Difference);
		};

		GetMutatedValueCallable<FRotator> GetMutatedValueCallableImpl = [](const FRotator& BaseValue, uint32 MutationBitMask, double Difference)
		{
			return FRotator(
				BaseValue.Pitch + GetNthBitAsUInt32(MutationBitMask, 2) * Difference,
				BaseValue.Yaw + GetNthBitAsUInt32(MutationBitMask, 1) * Difference,
				BaseValue.Roll + GetNthBitAsUInt32(MutationBitMask, 0) * Difference
			);
		};

		return RunMutationTestImpl<FRotator>(WhatPrefix, BaseValue, MaxFRotatorMutationBitMask, Difference, Tolerance,
			GetWhatCallableImpl, GetMutatedValueCallableImpl);
	}

	int32 RunFTransformMutationTest(const FString& WhatPrefix, const FTransform& BaseValue, double Difference, float Tolerance)
	{
		GetWhatCallable GetWhatCallableImpl = [](const FString& WhatPrefix, uint32 ActualValueMutationBitMask, uint32 ExpectedValueMutationBitMask, double Difference)
		{
			return FString::Printf(
				TEXT(
					"%s: the actual FTransform value is not nearly equal to the expected FTransform value\n"
					"(mutation mask for actual value is (%c, %c, %c, %c, %c, %c, %c, %c, %c), "
					"mutation mask for expected value is (%c, %c, %c, %c, %c, %c, %c, %c, %c), values were increased by %f)"
				),
				*WhatPrefix,
				GetNthBitAsChar(ActualValueMutationBitMask, 8),
				GetNthBitAsChar(ActualValueMutationBitMask, 7),
				GetNthBitAsChar(ActualValueMutationBitMask, 6),
				GetNthBitAsChar(ActualValueMutationBitMask, 5),
				GetNthBitAsChar(ActualValueMutationBitMask, 4),
				GetNthBitAsChar(ActualValueMutationBitMask, 3),
				GetNthBitAsChar(ActualValueMutationBitMask, 2),
				GetNthBitAsChar(ActualValueMutationBitMask, 1),
				GetNthBitAsChar(ActualValueMutationBitMask, 0),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 8),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 7),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 6),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 5),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 4),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 3),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 2),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 1),
				GetNthBitAsChar(ExpectedValueMutationBitMask, 0),
				Difference);
		};

		GetMutatedValueCallable<FTransform> GetMutatedValueCallableImpl = [](const FTransform& BaseValue, uint32 MutationBitMask, double Difference)
		{
			return FTransform(
				FRotator(
					BaseValue.Rotator().Pitch + GetNthBitAsUInt32(MutationBitMask, 8) * Difference,
					BaseValue.Rotator().Yaw + GetNthBitAsUInt32(MutationBitMask, 7) * Difference,
					BaseValue.Rotator().Roll + GetNthBitAsUInt32(MutationBitMask, 6) * Difference
				),
				FVector(
					BaseValue.GetTranslation().X + GetNthBitAsUInt32(MutationBitMask, 5) * Difference,
					BaseValue.GetTranslation().Y + GetNthBitAsUInt32(MutationBitMask, 4) * Difference,
					BaseValue.GetTranslation().Z + GetNthBitAsUInt32(MutationBitMask, 3) * Difference
				),
				FVector(
					BaseValue.GetScale3D().X + GetNthBitAsUInt32(MutationBitMask, 2) * Difference,
					BaseValue.GetScale3D().Y + GetNthBitAsUInt32(MutationBitMask, 1) * Difference,
					BaseValue.GetScale3D().Z + GetNthBitAsUInt32(MutationBitMask, 0) * Difference
				)
			);
		};

		return RunMutationTestImpl<FTransform>(WhatPrefix, BaseValue, MaxFTransformMutationBitMask, Difference, Tolerance,
			GetWhatCallableImpl, GetMutatedValueCallableImpl);
	}

	static const float NullTolerance;
	static const float PositiveTolerance;
	static const float PositiveDifference;
	static const float PositiveHalfDifference;

	// Max mutation masks for complex classes/structs
	// Each bit represents whether (value 1) or not (value 0) mutation will be applied to the object's constructor paramter
	static const uint32 MaxFVectorMutationBitMask;
	static const uint32 MaxFRotatorMutationBitMask;
	static const uint32 MaxFTransformMutationBitMask;

	static const FString TestFailMessage;

	static const float BaseFloatValue;
	static const float ActualFloatValue;
	static const float ExpectedFloatValue;
	static const float ExpectedFloatValueForNullTolerance;
	static const float FloatDifferenceToGetOutOfTolerance;
	static const float ExpectedFloatValueOutOfTolerance;

	static const double BaseDoubleValue;
	static const double ActualDoubleValue;
	static const double ExpectedDoubleValue;
	static const double ExpectedDoubleValueForNullTolerance;
	static const double DoubleDifferenceToGetOutOfTolerance;
	static const double ExpectedDoubleValueOutOfTolerance;

	static const FVector ActualFVectorValue;
	static const FVector& ExpectedFVectorValue;
	static const FVector& BaseFVectorValue;

	static const FRotator ActualFRotatorValue;
	static const FRotator& ExpectedFRotatorValue;
	static const FRotator& BaseFRotatorValue;

	static const FTransform ActualFTransformValue;
	static const FTransform& ExpectedFTransformValue;
	static const FTransform& BaseFTransformValue;

private:
	template<typename T>
	int32 RunMutationTestImpl(const FString& WhatPrefix, const T& BaseValue, uint32 MaxMutationBitMask, float Difference, float Tolerance,
		GetWhatCallable GetWhatCallableImpl, GetMutatedValueCallable<T> GetMutatedValueCallableImpl)
	{
		check(Difference != 0.f);
		check(Tolerance > 0.f);

		int32 CasesCheckedTotal = 0;

		for (uint32 ActualValueMutationBitMask = 0; ActualValueMutationBitMask <= MaxMutationBitMask; ++ActualValueMutationBitMask)
		{
			for (uint32 ExpectedValueMutationBitMask = 0; ExpectedValueMutationBitMask <= MaxMutationBitMask; ++ExpectedValueMutationBitMask)
			{
				if (ActualValueMutationBitMask == ExpectedValueMutationBitMask)
				{
					// The values' mutation submasks are the same, we should skip this combination
					continue;
				}

				// Perform test with mutated values in accordance to the current MutationBitMask
				const FString WhatMessage(GetWhatCallableImpl(WhatPrefix, ActualValueMutationBitMask, ExpectedValueMutationBitMask, Difference));
				const T ActualValue(GetMutatedValueCallableImpl(BaseValue, ActualValueMutationBitMask, Difference));
				const T ExpectedValue(GetMutatedValueCallableImpl(BaseValue, ExpectedValueMutationBitMask, Difference));

				TestNearlyEqual(WhatMessage, ActualValue, ExpectedValue, Tolerance);
				++CasesCheckedTotal;
			}
		}

		return CasesCheckedTotal;
	}

	static uint32 GetNthBitAsUInt32(uint32 Value, uint32 BitIndex)
	{
		return ((Value & (1 << BitIndex)) == 0 ? 0 : 1);
	}

	static char GetNthBitAsChar(uint32 Value, uint32 BitIndex)
	{
		return (GetNthBitAsUInt32(Value, BitIndex) == 1 ? '1' : '0');
	}

	static const FString ActualValueIsIncreasedByFormatString;
	static const FString ExpectedValueIsIncreasedByFormatString;
	static const FString DifferenceAndOrToleranceAreNotValidFormatString;
};

const float FAutomationNearlyEqualTest::NullTolerance(0.f);
const float FAutomationNearlyEqualTest::PositiveTolerance(1.e-4f);
const float FAutomationNearlyEqualTest::PositiveDifference(1.e-4f);
const float FAutomationNearlyEqualTest::PositiveHalfDifference((1.e-4f) / 2.f);
const FString FAutomationNearlyEqualTest::TestFailMessage(TEXT("Total amount of errors is not equal to the expected amount"));
const uint32 FAutomationNearlyEqualTest::MaxFVectorMutationBitMask(0b111);
const uint32 FAutomationNearlyEqualTest::MaxFRotatorMutationBitMask(0b111);
const uint32 FAutomationNearlyEqualTest::MaxFTransformMutationBitMask(0b111);

const float FAutomationNearlyEqualTest::BaseFloatValue(0.f);
const float FAutomationNearlyEqualTest::ActualFloatValue(BaseFloatValue);
const float FAutomationNearlyEqualTest::ExpectedFloatValue(BaseFloatValue);
const float FAutomationNearlyEqualTest::ExpectedFloatValueForNullTolerance(0.1f);
const float FAutomationNearlyEqualTest::FloatDifferenceToGetOutOfTolerance(PositiveTolerance + 0.1f);
const float FAutomationNearlyEqualTest::ExpectedFloatValueOutOfTolerance(ActualFloatValue + FloatDifferenceToGetOutOfTolerance + 0.1f);

const double FAutomationNearlyEqualTest::BaseDoubleValue(0.0);
const double FAutomationNearlyEqualTest::ActualDoubleValue(BaseDoubleValue);
const double FAutomationNearlyEqualTest::ExpectedDoubleValue(BaseDoubleValue);
const double FAutomationNearlyEqualTest::ExpectedDoubleValueForNullTolerance(0.1);
const double FAutomationNearlyEqualTest::DoubleDifferenceToGetOutOfTolerance(PositiveTolerance + 0.1);
const double FAutomationNearlyEqualTest::ExpectedDoubleValueOutOfTolerance(ActualDoubleValue + DoubleDifferenceToGetOutOfTolerance);

const FVector FAutomationNearlyEqualTest::ActualFVectorValue(0.f, -1.f, 1.f);
const FVector& FAutomationNearlyEqualTest::ExpectedFVectorValue(ActualFVectorValue);
const FVector& FAutomationNearlyEqualTest::BaseFVectorValue(ActualFVectorValue);

const FRotator FAutomationNearlyEqualTest::ActualFRotatorValue(0.001f, -1.002f, 1.003f);
const FRotator& FAutomationNearlyEqualTest::ExpectedFRotatorValue(ActualFRotatorValue);
const FRotator& FAutomationNearlyEqualTest::BaseFRotatorValue(ActualFRotatorValue);

const FTransform FAutomationNearlyEqualTest::ActualFTransformValue(FRotator(0.f, -1.f, 1.f), FVector(0.1f, -1.2f, 1.3f), FVector(0.01f, -1.02f, 1.03f));
const FTransform& FAutomationNearlyEqualTest::ExpectedFTransformValue(ActualFTransformValue);
const FTransform& FAutomationNearlyEqualTest::BaseFTransformValue(ActualFTransformValue);

const FString FAutomationNearlyEqualTest::ActualValueIsIncreasedByFormatString(TEXT("{0} (actual value is increased by {1})"));
const FString FAutomationNearlyEqualTest::ExpectedValueIsIncreasedByFormatString(TEXT("{0} (expected value is increased by {1})"));
const FString FAutomationNearlyEqualTest::DifferenceAndOrToleranceAreNotValidFormatString(TEXT("Difference and/or Tolerance are not valid. Difference: {0}, Tolerance: {1}"));

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualFloatPositive, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualFloatPositive", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualFloatPositive::RunTest(const FString& Parameters)
{
	//** TEST **//
	RunSimpleTest<float>(TEXT("The same float values with null tolerance"),
		ActualFloatValue, ExpectedFloatValue, NullTolerance);
	RunSimpleTest<float>(TEXT("The same float values with positive tolerance"),
		ActualFloatValue, ExpectedFloatValue, PositiveTolerance);
	RunFloatMutationTest(TEXT("Mutation of base float value with the same positive difference and tolerance (edge case)"),
		BaseFloatValue, PositiveDifference, PositiveTolerance);
	RunFloatMutationTest(TEXT("Mutation of base float value with negative difference and positive tolerance that are equal after being placed in Abs"),
		BaseFloatValue, -PositiveDifference, PositiveTolerance);
	RunFloatMutationTest(TEXT("Mutation of base float value with positive half difference and positive tolerance"),
		BaseFloatValue, PositiveHalfDifference, PositiveTolerance);
	RunFloatMutationTest(TEXT("Mutation of base float value with negative half difference and positive tolerance"),
		BaseFloatValue, -PositiveHalfDifference, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	const int32 ExpectedErrorTotal = 0;

	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Error);
	TestEqual(TestFailMessage, ErrorTotal, ExpectedErrorTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualFloatNegative, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualFloatNegative", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualFloatNegative::RunTest(const FString& Parameters)
{
	int32 CasesCheckedTotal = 0;

	//** TEST **//
	CasesCheckedTotal += RunSimpleTest<float>(TEXT("Different float values with null tolerance"),
		ActualFloatValue, ExpectedFloatValueForNullTolerance, NullTolerance);
	CasesCheckedTotal += RunSimpleTest<float>(TEXT("Different float values with positive tolerance"),
		ActualFloatValue, ExpectedFloatValueOutOfTolerance, PositiveTolerance);
	CasesCheckedTotal += RunFloatMutationTest(TEXT("Mutation of base float value with positive difference that is greater than positive tolerance"),
		BaseFloatValue, FloatDifferenceToGetOutOfTolerance, PositiveTolerance);
	CasesCheckedTotal += RunFloatMutationTest(TEXT("Mutation of base float value with negative difference which absolute value is greater than positive tolerance"),
		BaseFloatValue, -FloatDifferenceToGetOutOfTolerance, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Error);
	TestEqual(TestFailMessage, ErrorTotal, CasesCheckedTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualDoublePositive, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualDoublePositive", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualDoublePositive::RunTest(const FString& Parameters)
{
	//** TEST **//
	RunSimpleTest<double>(TEXT("The same double values with null tolerance"),
		ActualDoubleValue, ExpectedDoubleValue, NullTolerance);
	RunSimpleTest<double>(TEXT("The same double values with positive tolerance"),
		ActualDoubleValue, ExpectedDoubleValue, PositiveTolerance);
	RunDoubleMutationTest(TEXT("Mutation of base double value with the same positive difference and tolerance (edge case)"),
		BaseDoubleValue, PositiveDifference, PositiveTolerance);
	RunDoubleMutationTest(TEXT("Mutation of base double value with negative difference and positive tolerance that are equal after being placed in Abs"),
		BaseDoubleValue, -PositiveDifference, PositiveTolerance);
	RunDoubleMutationTest(TEXT("Mutation of base double value with positive half difference and positive tolerance"),
		BaseDoubleValue, PositiveHalfDifference, PositiveTolerance);
	RunDoubleMutationTest(TEXT("Mutation of base double value with negative half difference and positive tolerance"),
		BaseDoubleValue, -PositiveHalfDifference, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	const int32 ExpectedErrorTotal = 0;

	TestEqual(TestFailMessage, ErrorTotal, ExpectedErrorTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualDoubleNegative, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualDoubleNegative", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualDoubleNegative::RunTest(const FString& Parameters)
{
	int32 CasesCheckedTotal = 0;

	//** TEST **//
	CasesCheckedTotal += RunSimpleTest<double>(TEXT("Different double values with null tolerance"),
		ActualDoubleValue, ExpectedDoubleValueForNullTolerance, NullTolerance);
	CasesCheckedTotal += RunSimpleTest<double>(TEXT("Different double values with positive tolerance"),
		ActualDoubleValue, ExpectedDoubleValueOutOfTolerance, PositiveTolerance);
	CasesCheckedTotal += RunDoubleMutationTest(TEXT("Mutation of base double value with positive difference that is greater than positive tolerance"),
		BaseDoubleValue, DoubleDifferenceToGetOutOfTolerance, PositiveTolerance);
	CasesCheckedTotal += RunDoubleMutationTest(TEXT("Mutation of base double value with negative difference which absolute value is greater than positive tolerance"),
		BaseDoubleValue, -DoubleDifferenceToGetOutOfTolerance, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Error);
	TestEqual(TestFailMessage, ErrorTotal, CasesCheckedTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualFVectorPositive, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualFVectorPositive", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualFVectorPositive::RunTest(const FString& Parameters)
{
	//** TEST **//
	RunSimpleTest<FVector>(TEXT("The same FVector values with null tolerance"),
		ActualFVectorValue, ExpectedFVectorValue, NullTolerance);
	RunSimpleTest<FVector>(TEXT("The same FVector values with positive tolerance"),
		ActualFVectorValue, ExpectedFVectorValue, PositiveTolerance);
	RunFVectorMutationTest(TEXT("Mutation of base FVector value with the same positive difference and tolerance (edge case)"),
		BaseFVectorValue, PositiveDifference, PositiveTolerance);
	RunFVectorMutationTest(TEXT("Mutation of base FVector value with negative difference and positive tolerance that are equal after being placed in Abs"),
		BaseFVectorValue, -PositiveDifference, PositiveTolerance);
	RunFVectorMutationTest(TEXT("Mutation of base FVector value with positive half difference and positive tolerance"),
		BaseFVectorValue, PositiveHalfDifference, PositiveTolerance);
	RunFVectorMutationTest(TEXT("Mutation of base FVector value with negative half difference and positive tolerance"),
		BaseFVectorValue, -PositiveHalfDifference, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	const int32 ExpectedErrorTotal = 0;

	TestEqual(TestFailMessage, ErrorTotal, ExpectedErrorTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualFVectorNegative, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualFVectorNegative", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualFVectorNegative::RunTest(const FString& Parameters)
{
	int32 CasesCheckedTotal = 0;

	//** TEST **//
	CasesCheckedTotal += RunFVectorMutationTest(TEXT("Mutation of base FVector value with positive difference that is greater than positive tolerance"),
		BaseFVectorValue, PositiveDifference + 0.1f, PositiveTolerance);
	CasesCheckedTotal += RunFVectorMutationTest(TEXT("Mutation of base FVector value with negative difference which absolute value is greater than positive tolerance"),
		BaseFVectorValue, -PositiveDifference - 0.1f, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Error);
	TestEqual(TestFailMessage, ErrorTotal, CasesCheckedTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualFRotatorPositive, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualFRotatorPositive", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualFRotatorPositive::RunTest(const FString& Parameters)
{
	//** TEST **//
	RunSimpleTest<FRotator>(TEXT("The same FRotator values with null tolerance"),
		ActualFRotatorValue, ExpectedFRotatorValue, NullTolerance);
	RunSimpleTest<FRotator>(TEXT("The same FRotator values with positive tolerance"),
		ActualFRotatorValue, ExpectedFRotatorValue, PositiveTolerance);
	RunFRotatorMutationTest(TEXT("Mutation of base FRotator value with the same positive difference and tolerance (edge case)"),
		BaseFRotatorValue, PositiveDifference, PositiveTolerance);
	RunFRotatorMutationTest(TEXT("Mutation of base FRotator value with negative difference and positive tolerance that are equal after being placed in Abs"),
		BaseFRotatorValue, -PositiveDifference, PositiveTolerance);
	RunFRotatorMutationTest(TEXT("Mutation of base FRotator value with positive half difference and positive tolerance"),
		BaseFRotatorValue, PositiveHalfDifference, PositiveTolerance);
	RunFRotatorMutationTest(TEXT("Mutation of base FRotator value with negative half difference and positive tolerance"),
		BaseFRotatorValue, -PositiveHalfDifference, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	const int32 ExpectedErrorTotal = 0;

	TestEqual(TestFailMessage, ErrorTotal, ExpectedErrorTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualFRotatorNegative, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualFRotatorNegative", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualFRotatorNegative::RunTest(const FString& Parameters)
{
	int32 CasesCheckedTotal = 0;

	//** TEST **//
	CasesCheckedTotal += RunFRotatorMutationTest(TEXT("Mutation of base FRotator value with positive difference that is greater than positive tolerance"),
		BaseFRotatorValue, PositiveDifference + 1, PositiveTolerance);
	CasesCheckedTotal += RunFRotatorMutationTest(TEXT("Mutation of base FRotator value with negative difference which absolute value is greater than positive tolerance"),
		BaseFRotatorValue, -PositiveDifference - 1, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Error);
	TestEqual(TestFailMessage, ErrorTotal, CasesCheckedTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualFTransformPositive, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualFTransformPositive", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualFTransformPositive::RunTest(const FString& Parameters)
{
	//** TEST **//
	RunSimpleTest<FTransform>(TEXT("The same FTransform values with null tolerance"),
		ActualFTransformValue, ExpectedFTransformValue, NullTolerance);
	RunSimpleTest<FTransform>(TEXT("The same FTransform values with positive tolerance"),
		ActualFTransformValue, ExpectedFTransformValue, PositiveTolerance);
	RunFTransformMutationTest(TEXT("Mutation of base FTransform value with the same positive difference and tolerance (edge case)"),
		BaseFTransformValue, PositiveDifference, PositiveTolerance);
	RunFTransformMutationTest(TEXT("Mutation of base FTransform value with negative difference and positive tolerance that are equal after being placed in Abs"),
		BaseFTransformValue, -PositiveDifference, PositiveTolerance);
	RunFTransformMutationTest(TEXT("Mutation of base FTransform value with positive half difference and positive tolerance"),
		BaseFTransformValue, PositiveHalfDifference, PositiveTolerance);
	RunFTransformMutationTest(TEXT("Mutation of base FTransform value with negative half difference and positive tolerance"),
		BaseFTransformValue, -PositiveHalfDifference, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	const int32 ExpectedErrorTotal = 0;

	TestEqual(TestFailMessage, ErrorTotal, ExpectedErrorTotal);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTestNearlyEqualFTransformNegative, FAutomationNearlyEqualTest, "TestFramework.Validation.TestNearlyEqualFTransformNegative", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTestNearlyEqualFTransformNegative::RunTest(const FString& Parameters)
{
	int32 CasesCheckedTotal = 0;

	//** TEST **//
	CasesCheckedTotal += RunFTransformMutationTest(TEXT("Mutation of base FTransform value with positive difference that is greater than positive tolerance"),
		BaseFTransformValue, PositiveDifference + 0.1f, PositiveTolerance);
	CasesCheckedTotal += RunFTransformMutationTest(TEXT("Mutation of base FTransform value with negative difference which absolute value is greater than positive tolerance"),
		BaseFTransformValue, -PositiveDifference - 0.1f, PositiveTolerance);

	//** VERIFY **//
	const int32 ErrorTotal = ExecutionInfo.GetErrorTotal();
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Error);
	TestEqual(TestFailMessage, ErrorTotal, CasesCheckedTotal);

	return true;
}

class FAutomationUTestMacrosExpr : public FAutomationTestBase
{
public:
	FAutomationUTestMacrosExpr(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

protected:

	static const float PositiveToleranceFloat;
	static const float ActualFloatValue;
	static const float ExpectedFloatValue;
	static const float WrongFloatValue;
	static const float ExpectedFloatValueOutOfTolerance;
	static const FString ActualFStringValue;
	static const FString ExpectedFStringValueLowerCase;
	static const FString UnexpectedFStringValueLowerCase;
};

const float FAutomationUTestMacrosExpr::PositiveToleranceFloat(1.e-4f);
const float FAutomationUTestMacrosExpr::ActualFloatValue(0.f);
const float FAutomationUTestMacrosExpr::ExpectedFloatValue(ActualFloatValue);
const float FAutomationUTestMacrosExpr::WrongFloatValue(ActualFloatValue + 1.f);
const float FAutomationUTestMacrosExpr::ExpectedFloatValueOutOfTolerance(ActualFloatValue + PositiveToleranceFloat);
const FString FAutomationUTestMacrosExpr::ActualFStringValue(TEXT("EQUALS"));
const FString FAutomationUTestMacrosExpr::ExpectedFStringValueLowerCase(TEXT("equals"));
const FString FAutomationUTestMacrosExpr::UnexpectedFStringValueLowerCase(TEXT("not-equals"));

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationEqualEXPR, FAutomationUTestMacrosExpr, "TestFramework.Validation.UTestEqual", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationEqualEXPR::RunTest(const FString& Parameters)
{

	UTEST_EQUAL_EXPR(ActualFloatValue, ExpectedFloatValue);
	UTEST_NEARLY_EQUAL_EXPR(ActualFloatValue, ExpectedFloatValueOutOfTolerance, PositiveToleranceFloat);
	UTEST_EQUAL_TOLERANCE_EXPR(ActualFloatValue, ExpectedFloatValueOutOfTolerance, PositiveToleranceFloat);
	UTEST_NOT_EQUAL_EXPR(ActualFloatValue, WrongFloatValue);
	UTEST_EQUAL_INSENSITIVE_EXPR(*ActualFStringValue, *ExpectedFStringValueLowerCase);
	UTEST_NOT_EQUAL_INSENSITIVE_EXPR(*ActualFStringValue, *UnexpectedFStringValueLowerCase);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationSameNotSameEXPR, FAutomationUTestMacrosExpr, "TestFramework.Validation.UTestSameNotSame", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationSameNotSameEXPR::RunTest(const FString& Parameters)
{

	UTEST_SAME_EXPR(ActualFStringValue, ActualFStringValue);
	UTEST_NOT_SAME_EXPR(ActualFStringValue, ExpectedFStringValueLowerCase);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationTrueFalseEXPR, FAutomationUTestMacrosExpr, "TestFramework.Validation.UTestTrueFalse", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationTrueFalseEXPR::RunTest(const FString& Parameters)
{

	UTEST_TRUE_EXPR(ActualFloatValue == ExpectedFloatValue);
	UTEST_FALSE_EXPR(ActualFloatValue > ExpectedFloatValue);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationValidInvalidEXPR, FAutomationUTestMacrosExpr, "TestFramework.Validation.UTestValidInvalid", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationValidInvalidEXPR::RunTest(const FString& Parameters)
{
	struct FHasIsValid
	{
		explicit FHasIsValid(bool bInIsValid)
			: bIsValid(bInIsValid)
		{  }
		
		bool IsValid() const { return bIsValid; }

	private:
		bool bIsValid;
	};
	
	//** TEST **//
	TSharedPtr<FVector> ValidSharedPtr = MakeShared<FVector>();
	TSharedPtr<UObject> InvalidSharedPtr = nullptr;

	FHasIsValid ValidObject(true);
	FHasIsValid InvalidObject(false);

	//** VERIFY **//
	UTEST_VALID_EXPR(ValidSharedPtr);
	UTEST_INVALID_EXPR(InvalidSharedPtr);

	UTEST_VALID_EXPR(ValidObject);
	UTEST_INVALID_EXPR(InvalidObject);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAutomationNullNotNullPtrEXPR, FAutomationUTestMacrosExpr, "TestFramework.Validation.UTestNullNotNull", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FAutomationNullNotNullPtrEXPR::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);

	UTEST_NULL_EXPR(nullptr);
	UTEST_NOT_NULL_EXPR(World);

	World->DestroyWorld(false);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationAttachment, "System.Engine.Attachment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#define DUMP_EXPECTED_TRANSFORMS 0
#define TEST_NEW_ATTACHMENTS 1
#define TEST_DEPRECATED_ATTACHMENTS 0

namespace AttachTestConstants
{
	const FVector ParentLocation(1.0f, -2.0f, 4.0f);
	const FQuat ParentRotation(FRotator(0.0f, 45.0f, 45.0f).Quaternion());
	const FVector ParentScale(1.25f, 1.25f, 1.25f);
	const FVector ChildLocation(2.0f, -8.0f, -4.0f);
	const FQuat ChildRotation(FRotator(0.0f, 45.0f, 20.0f).Quaternion());
	const FVector ChildScale(1.25f, 1.25f, 1.25f);
}

void AttachmentTest_CommonTests(AActor* ParentActor, AActor* ChildActor, FAutomationTestBase* Test)
{
#if TEST_DEPRECATED_ATTACHMENTS
	static const FTransform LegacyExpectedChildTransforms[4][2] =
	{
		{
			FTransform(
				FQuat(-0.49031073f, -0.11344112f, 0.64335662f, 0.57690436f),
				FVector(10.26776695f, -7.73223305f, 7.53553343f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
			FTransform(
				FQuat(-0.49031067f, -0.11344092f, 0.64335656f, 0.57690459f),
				FVector(10.26776695f, -7.73223305f, 7.53553343f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
		},
		{
			FTransform(
				FQuat(-0.49031067f, -0.11344092f, 0.64335662f, 0.57690459f),
				FVector(10.26776695f, -7.73223305f, 7.53553343f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
			FTransform(
				FQuat(-0.49031061f, -0.11344086f, 0.64335656f, 0.57690465f),
				FVector(10.26776695f, -7.73223305f, 7.53553343f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
		},
		{
			FTransform(
				FQuat(-0.35355338f, -0.14644660f, 0.35355338f, 0.85355335f),
				FVector(1.00000000f, -2.00000000f, 4.00000000f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
			FTransform(
				FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
				FVector(1.00000000f, -2.00000000f, 4.00000000f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
		},
		{
			FTransform(
				FQuat(-0.35355338f, -0.14644660f, 0.35355338f, 0.85355335f),
				FVector(1.00000000f, -2.00000000f, 4.00000000f),
				FVector(1.25000000f, 1.25000000f, 1.25000000f)
			),
			FTransform(
				FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
				FVector(1.00000000f, -2.00000000f, 4.00000000f),
				FVector(1.25000000f, 1.25000000f, 1.25000000f)
			),
		},
	};

	for (uint8 LocationInteger = (uint8)EAttachLocation::KeepRelativeOffset; LocationInteger <= (uint8)EAttachLocation::SnapToTargetIncludingScale; ++LocationInteger)
	{
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("{"));
#endif
		EAttachLocation::Type Location = (EAttachLocation::Type)LocationInteger;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ChildActor->AttachRootComponentToActor(ParentActor, NAME_None, Location, true);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// check parent actor is unaffected by attachment
		Test->TestEqual<FVector>(TEXT("Parent location was affected by attachment"), ParentActor->GetActorLocation(), AttachTestConstants::ParentLocation);
		Test->TestEqual<FQuat>(TEXT("Parent rotation was affected by attachment"), ParentActor->GetActorQuat(), AttachTestConstants::ParentRotation);
		Test->TestEqual<FVector>(TEXT("Parent scale was affected by attachment"), ParentActor->GetActorScale3D(), AttachTestConstants::ParentScale);

		// check we have expected transforms for each mode
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("\tFTransform("));
		UE_LOG(LogTemp, Log, TEXT("\t\tFQuat(%.8ff, %.8ff, %.8ff, %.8ff),"), ChildActor->GetActorQuat().X, ChildActor->GetActorQuat().Y, ChildActor->GetActorQuat().Z, ChildActor->GetActorQuat().W);
		UE_LOG(LogTemp, Log, TEXT("\t\tFVector(%.8ff, %.8ff, %.8ff),"), ChildActor->GetActorLocation().X, ChildActor->GetActorLocation().Y, ChildActor->GetActorLocation().Z);
		UE_LOG(LogTemp, Log, TEXT("\t\tFVector(%.8ff, %.8ff, %.8ff)"), ChildActor->GetActorScale3D().X, ChildActor->GetActorScale3D().Y, ChildActor->GetActorScale3D().Z);
		UE_LOG(LogTemp, Log, TEXT("\t),"));
#endif

		Test->TestTrue(FString::Printf(TEXT("Child world location was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorLocation().ToString(), *LegacyExpectedChildTransforms[LocationInteger][0].GetLocation().ToString()), ChildActor->GetActorLocation().Equals(LegacyExpectedChildTransforms[LocationInteger][0].GetLocation(), KINDA_SMALL_NUMBER));
		Test->TestTrue(FString::Printf(TEXT("Child world rotation was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorQuat().ToString(), *LegacyExpectedChildTransforms[LocationInteger][0].GetRotation().ToString()), ChildActor->GetActorQuat().Equals(LegacyExpectedChildTransforms[LocationInteger][0].GetRotation(), KINDA_SMALL_NUMBER));
		Test->TestTrue(FString::Printf(TEXT("Child world scale was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorScale3D().ToString(), *LegacyExpectedChildTransforms[LocationInteger][0].GetScale3D().ToString()), ChildActor->GetActorScale3D().Equals(LegacyExpectedChildTransforms[LocationInteger][0].GetScale3D(), KINDA_SMALL_NUMBER));

		ChildActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		// check we have expected values after detachment
		Test->TestEqual<FVector>(TEXT("Parent location was affected by detachment"), ParentActor->GetActorLocation(), AttachTestConstants::ParentLocation);
		Test->TestEqual<FQuat>(TEXT("Parent rotation was affected by detachment"), ParentActor->GetActorQuat(), AttachTestConstants::ParentRotation);
		Test->TestEqual<FVector>(TEXT("Parent scale was affected by detachment"), ParentActor->GetActorScale3D(), AttachTestConstants::ParentScale);

		// check we have expected transforms for each mode
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("\tFTransform("));
		UE_LOG(LogTemp, Log, TEXT("\t\tFQuat(%.8ff, %.8ff, %.8ff, %.8ff),"), ChildActor->GetActorQuat().X, ChildActor->GetActorQuat().Y, ChildActor->GetActorQuat().Z, ChildActor->GetActorQuat().W);
		UE_LOG(LogTemp, Log, TEXT("\t\tFVector(%.8ff, %.8ff, %.8ff),"), ChildActor->GetActorLocation().X, ChildActor->GetActorLocation().Y, ChildActor->GetActorLocation().Z);
		UE_LOG(LogTemp, Log, TEXT("\t\tFVector(%.8ff, %.8ff, %.8ff)"), ChildActor->GetActorScale3D().X, ChildActor->GetActorScale3D().Y, ChildActor->GetActorScale3D().Z);
		UE_LOG(LogTemp, Log, TEXT("\t),"));
#endif

		Test->TestTrue(FString::Printf(TEXT("Child relative location was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorLocation().ToString(), *LegacyExpectedChildTransforms[LocationInteger][1].GetLocation().ToString()), ChildActor->GetActorLocation().Equals(LegacyExpectedChildTransforms[LocationInteger][1].GetLocation(), KINDA_SMALL_NUMBER));
		Test->TestTrue(FString::Printf(TEXT("Child relative rotation was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorQuat().ToString(), *LegacyExpectedChildTransforms[LocationInteger][1].GetRotation().ToString()), ChildActor->GetActorQuat().Equals(LegacyExpectedChildTransforms[LocationInteger][1].GetRotation(), KINDA_SMALL_NUMBER));
		Test->TestTrue(FString::Printf(TEXT("Child relative scale was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorScale3D().ToString(), *LegacyExpectedChildTransforms[LocationInteger][1].GetScale3D().ToString()), ChildActor->GetActorScale3D().Equals(LegacyExpectedChildTransforms[LocationInteger][1].GetScale3D(), KINDA_SMALL_NUMBER));
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("},"));
#endif
	}
#endif

#if TEST_NEW_ATTACHMENTS
	// Check each component against each rule in all combinations, pre and post-detachment
	static const FTransform ExpectedChildTransforms[3][3][3][2] =
	{
		{
			{
				{
					FTransform(
						FQuat(-0.49031073f, -0.11344108f, 0.64335668f, 0.57690459f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.16042995f, -0.06645225f, 0.37686956f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.49031073f, -0.11344108f, 0.64335668f, 0.57690459f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.16042995f, -0.06645225f, 0.37686956f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.49031073f, -0.11344108f, 0.64335668f, 0.57690459f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.16042995f, -0.06645225f, 0.37686956f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.16042994f, -0.06645226f, 0.37686956f, 0.90984380f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.16042991f, -0.06645230f, 0.37686959f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.16042991f, -0.06645229f, 0.37686959f, 0.90984380f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.16042989f, -0.06645229f, 0.37686959f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.16042989f, -0.06645229f, 0.37686956f, 0.90984380f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.16042989f, -0.06645229f, 0.37686959f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
		},
		{
			{
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.99999976f, -8.00000000f, -4.00000095f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999976f, -8.00000000f, -4.00000095f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.99999881f, -8.00000095f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999881f, -8.00000095f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
		},
		{
			{
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
		},
	};

	const FVector ParentPreAttachmentLocation = ParentActor->GetActorLocation();
	const FQuat ParentPreAttachmentRotation = ParentActor->GetActorQuat();
	const FVector ParentPreAttachmentScale = ParentActor->GetActorScale3D();

	for (uint8 RuleInteger0 = (uint8)EAttachmentRule::KeepRelative; RuleInteger0 <= (uint8)EAttachmentRule::SnapToTarget; ++RuleInteger0)
	{
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("{"));
#endif
		for (uint8 RuleInteger1 = (uint8)EAttachmentRule::KeepRelative; RuleInteger1 <= (uint8)EAttachmentRule::SnapToTarget; ++RuleInteger1)
		{
#if DUMP_EXPECTED_TRANSFORMS
			UE_LOG(LogTemp, Log, TEXT("\t{"));
#endif
			for (uint8 RuleInteger2 = (uint8)EAttachmentRule::KeepRelative; RuleInteger2 <= (uint8)EAttachmentRule::SnapToTarget; ++RuleInteger2)
			{
#if DUMP_EXPECTED_TRANSFORMS
				UE_LOG(LogTemp, Log, TEXT("\t\t{"));
#endif
				EAttachmentRule Rule0 = (EAttachmentRule)RuleInteger0;
				EAttachmentRule Rule1 = (EAttachmentRule)RuleInteger1;
				EAttachmentRule Rule2 = (EAttachmentRule)RuleInteger2;

				FAttachmentTransformRules Rules(Rule0, Rule1, Rule2, false);

				ChildActor->AttachToActor(ParentActor, Rules);

				// check parent actor is unaffected by attachment
				Test->TestEqual<FVector>(TEXT("Parent location was affected by attachment"), ParentActor->GetActorLocation(), ParentPreAttachmentLocation);
				Test->TestEqual<FQuat>(TEXT("Parent rotation was affected by attachment"), ParentActor->GetActorQuat(), ParentPreAttachmentRotation);
				Test->TestEqual<FVector>(TEXT("Parent scale was affected by attachment"), ParentActor->GetActorScale3D(), ParentPreAttachmentScale);

				// check we have expected transforms for each mode
#if DUMP_EXPECTED_TRANSFORMS
				UE_LOG(LogTemp, Log, TEXT("\t\t\tFTransform("));
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFQuat(%.8ff, %.8ff, %.8ff, %.8ff),"), ChildActor->GetActorQuat().X, ChildActor->GetActorQuat().Y, ChildActor->GetActorQuat().Z, ChildActor->GetActorQuat().W);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFVector(%.8ff, %.8ff, %.8ff),"), ChildActor->GetActorLocation().X, ChildActor->GetActorLocation().Y, ChildActor->GetActorLocation().Z);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFVector(%.8ff, %.8ff, %.8ff)"), ChildActor->GetActorScale3D().X, ChildActor->GetActorScale3D().Y, ChildActor->GetActorScale3D().Z);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t),"));
#endif

				Test->TestTrue(FString::Printf(TEXT("Child world location was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorLocation().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetLocation().ToString()), ChildActor->GetActorLocation().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetLocation(), UE_KINDA_SMALL_NUMBER));
				Test->TestTrue(FString::Printf(TEXT("Child world rotation was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorQuat().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetRotation().ToString()), ChildActor->GetActorQuat().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetRotation(), UE_KINDA_SMALL_NUMBER));
				Test->TestTrue(FString::Printf(TEXT("Child world scale was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorScale3D().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetScale3D().ToString()), ChildActor->GetActorScale3D().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetScale3D(), UE_KINDA_SMALL_NUMBER));

				ChildActor->DetachFromActor(FDetachmentTransformRules(Rules, true));

				// check we have expected values after detachment
				Test->TestEqual<FVector>(TEXT("Parent location was affected by detachment"), ParentActor->GetActorLocation(), ParentPreAttachmentLocation);
				Test->TestEqual<FQuat>(TEXT("Parent rotation was affected by detachment"), ParentActor->GetActorQuat(), ParentPreAttachmentRotation);
				Test->TestEqual<FVector>(TEXT("Parent scale was affected by detachment"), ParentActor->GetActorScale3D(), ParentPreAttachmentScale);

				// check we have expected transforms for each mode
#if DUMP_EXPECTED_TRANSFORMS
				UE_LOG(LogTemp, Log, TEXT("\t\t\tFTransform("));
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFQuat(%.8ff, %.8ff, %.8ff, %.8ff),"), ChildActor->GetActorQuat().X, ChildActor->GetActorQuat().Y, ChildActor->GetActorQuat().Z, ChildActor->GetActorQuat().W);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFVector(%.8ff, %.8ff, %.8ff),"), ChildActor->GetActorLocation().X, ChildActor->GetActorLocation().Y, ChildActor->GetActorLocation().Z);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFVector(%.8ff, %.8ff, %.8ff)"), ChildActor->GetActorScale3D().X, ChildActor->GetActorScale3D().Y, ChildActor->GetActorScale3D().Z);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t),"));
#endif

				Test->TestTrue(FString::Printf(TEXT("Child relative location was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorLocation().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetLocation().ToString()), ChildActor->GetActorLocation().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetLocation(), UE_KINDA_SMALL_NUMBER));
				Test->TestTrue(FString::Printf(TEXT("Child relative rotation was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorQuat().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetRotation().ToString()), ChildActor->GetActorQuat().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetRotation(), UE_KINDA_SMALL_NUMBER));
				Test->TestTrue(FString::Printf(TEXT("Child relative scale was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorScale3D().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetScale3D().ToString()), ChildActor->GetActorScale3D().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetScale3D(), UE_KINDA_SMALL_NUMBER));
#if DUMP_EXPECTED_TRANSFORMS
				UE_LOG(LogTemp, Log, TEXT("\t\t},"));
#endif
			}
#if DUMP_EXPECTED_TRANSFORMS
			UE_LOG(LogTemp, Log, TEXT("\t},"));
#endif
		}
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("},"));
#endif
	}
#endif // TEST_NEW_ATTACHMENTS
}


void AttachmentTest_SetupParentAndChild(UWorld* World, AActor*& InOutParentActor, AActor*& InOutChildActor)
{
	ADefaultPawn* ParentActor = NewObject<ADefaultPawn>(World->PersistentLevel);
	ParentActor->SetActorLocation(AttachTestConstants::ParentLocation);
	ParentActor->SetActorRotation(AttachTestConstants::ParentRotation);
	ParentActor->SetActorScale3D(AttachTestConstants::ParentScale);

	ADefaultPawn* ChildActor = NewObject<ADefaultPawn>(World->PersistentLevel);
	ChildActor->SetActorLocation(AttachTestConstants::ChildLocation);
	ChildActor->SetActorRotation(AttachTestConstants::ChildRotation);
	ChildActor->SetActorScale3D(AttachTestConstants::ChildScale);

	InOutParentActor = ParentActor;
	InOutChildActor = ChildActor;
}

void AttachmentTest_AttachWhenNotAttached(UWorld* World, FAutomationTestBase* Test)
{
	AActor* ParentActor = nullptr;
	AActor* ChildActor = nullptr;
	AttachmentTest_SetupParentAndChild(World, ParentActor, ChildActor);

	AttachmentTest_CommonTests(ParentActor, ChildActor, Test);
}

void AttachmentTest_AttachWhenAttached(UWorld* World, FAutomationTestBase* Test)
{
	ADefaultPawn* PreviousParentActor = NewObject<ADefaultPawn>(World->PersistentLevel);
	PreviousParentActor->SetActorLocation(FVector::ZeroVector);
	PreviousParentActor->SetActorRotation(FQuat::Identity);
	PreviousParentActor->SetActorScale3D(FVector(1.0f));

	AActor* ParentActor = nullptr;
	AActor* ChildActor = nullptr;
	AttachmentTest_SetupParentAndChild(World, ParentActor, ChildActor);

#if TEST_DEPRECATED_ATTACHMENTS
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ChildActor->AttachRootComponentToActor(PreviousParentActor, NAME_None, EAttachLocation::KeepWorldPosition, true);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else
	ChildActor->AttachToActor(PreviousParentActor, FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));
#endif

	AttachmentTest_CommonTests(ParentActor, ChildActor, Test);
}

bool FAutomationAttachment::RunTest(const FString& Parameters)
{
	UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	AttachmentTest_AttachWhenNotAttached(World, this);
	AttachmentTest_AttachWhenAttached(World, this);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
