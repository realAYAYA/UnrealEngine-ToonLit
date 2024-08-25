// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_OCIO

#include "ColorSpace.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "OpenColorIOWrapper.h"
#include "TransferFunctions.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealOpenColorIOTest, Log, All);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenColorIOTransferFunctionsTest, "System.OpenColorIO.DecodeToWorkingColorSpace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOpenColorIOTransferFunctionsTest::RunTest(const FString& Parameters)
{
	using namespace UE::Color;

	bool bSuccess = true;
	const FLinearColor TestColor = FLinearColor(0.9f, 0.5f, 0.2f, 1.0f);

	for (uint8 TestEncoding = static_cast<uint8>(EEncoding::None); TestEncoding < static_cast<uint8>(EEncoding::Max); ++TestEncoding)
	{
		FLinearColor Expected = UE::Color::Decode(static_cast<EEncoding>(TestEncoding), TestColor);

		FOpenColorIOWrapperSourceColorSettings TestSettings;
		TestSettings.EncodingOverride = static_cast<EEncoding>(TestEncoding);

		FOpenColorIOWrapperProcessor Processor = FOpenColorIOWrapperProcessor::CreateTransformToWorkingColorSpace(TestSettings);

		FLinearColor Actual = TestColor;
		Processor.TransformColor(Actual);

		// Note: We make the tolerance relative to the values themselves to account for larger values in PQ.
		const float Tolerance = UE_KINDA_SMALL_NUMBER * 0.5f * (Actual.R + Expected.R);

		if (!Actual.Equals(Expected, Tolerance))
		{
			const FString TestNameToPrint = FString::Printf(TEXT("OpenColorIO: %u:%u"), (uint32)TestSettings.EncodingOverride, (uint32)TestSettings.ColorSpace);
			AddError(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s."), *TestNameToPrint, *Expected.ToString(), *Actual.ToString()), 1);
			bSuccess = false;
		}
	}


	// Name collision test
	int32 Count = 0;
	TSet<FString> Keys;
	FRandomStream RandomStream;
	RandomStream.Initialize(42);
	for (uint8 TestEncoding = static_cast<uint8>(EEncoding::None); TestEncoding < static_cast<uint8>(EEncoding::Max); ++TestEncoding)
	{
		for (uint8 TestColorSpace = static_cast<uint8>(EColorSpace::None); TestColorSpace <= static_cast<uint8>(EColorSpace::Max); ++TestColorSpace)
		{
			for (uint8 TestChromatic = 0; TestChromatic < 2; ++TestChromatic)
			{
				FOpenColorIOWrapperSourceColorSettings TestSettings;
				TestSettings.EncodingOverride = static_cast<EEncoding>(TestEncoding);

				if (TestColorSpace < static_cast<uint8>(EColorSpace::Max))
				{
					TestSettings.ColorSpace = static_cast<EColorSpace>(TestColorSpace);
				}
				else
				{
					// We locally use EColorSpace::Max as a test custom color space.
					TStaticArray<FVector2d, 4> RandomChromaticities;
					RandomChromaticities[0] = FVector2d(RandomStream.GetUnitVector());
					RandomChromaticities[1] = FVector2d(RandomStream.GetUnitVector());
					RandomChromaticities[2] = FVector2d(RandomStream.GetUnitVector());
					RandomChromaticities[3] = FVector2d(RandomStream.GetUnitVector());

					TestSettings.ColorSpaceOverride = MoveTemp(RandomChromaticities);
				}
				
				TestSettings.ChromaticAdaptationMethod = static_cast<EChromaticAdaptationMethod>(TestChromatic);

				Keys.Add(FOpenColorIOWrapperProcessor::GetTransformToWorkingColorSpaceName(TestSettings));
				Count++;
			}
		}
	}

	bSuccess &= TestEqual(TEXT("OpenColorIO: Name hash collision test"), Keys.Num(), Count);

	return bSuccess;
}

#endif