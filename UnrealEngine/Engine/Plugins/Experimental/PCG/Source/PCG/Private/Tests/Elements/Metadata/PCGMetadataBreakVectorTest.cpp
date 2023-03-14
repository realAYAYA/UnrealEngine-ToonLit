// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"
#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGParamData.h"

#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataBreakVector.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#if WITH_EDITOR

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataBreakVectorTest, FPCGTestBaseClass, "pcg.tests.Metadata.BreakVector", PCGTestsCommon::TestFlags)

namespace PCGBreakVectorTest
{
	enum class EPCGComponentToCheck : uint8
	{
		X,
		Z,
		Y,
		W
	};

	const FName Vec2Attribute = TEXT("Vec2");
	const FName Vec3Attribute = TEXT("Vec3");
	const FName Vec4Attribute = TEXT("Vec4");
	const FName RotatorAttribute = TEXT("Rotator");
	const FName FloatAttribute = TEXT("Float");

	void GenerateSpatialData(PCGTestsCommon::FTestData& TestData)
	{
		FPCGTaggedData& SpatialTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreateRandomPointData(10, TestData.Seed);
		SpatialTaggedData.Data = PointData;
		SpatialTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

		const bool bAllowsInterpolation = false;
		const bool bOverrideParent = false;

		PointData->Metadata->CreateVector2Attribute(Vec2Attribute, FVector2D::ZeroVector, bAllowsInterpolation, bOverrideParent);
		PointData->Metadata->CreateVectorAttribute(Vec3Attribute, FVector::Zero(), bAllowsInterpolation, bOverrideParent);
		PointData->Metadata->CreateVector4Attribute(Vec4Attribute, FVector4::Zero(), bAllowsInterpolation, bOverrideParent);
		PointData->Metadata->CreateRotatorAttribute(RotatorAttribute, FRotator::ZeroRotator, bAllowsInterpolation, bOverrideParent);
		PointData->Metadata->CreateFloatAttribute(FloatAttribute, 0, bAllowsInterpolation, bOverrideParent);

		TArray<FPCGPoint>& SourcePoints = PointData->GetMutablePoints();

		// Randomize attribute values, and leave the second half to default value
		FRandomStream RandomSource(TestData.Seed);
		for (int I = 0; I < SourcePoints.Num() / 2; ++I)
		{
			UPCGMetadataAccessorHelpers::SetVector2Attribute(SourcePoints[I], PointData->Metadata, Vec2Attribute, FVector2D(RandomSource.VRand()));
			UPCGMetadataAccessorHelpers::SetVectorAttribute(SourcePoints[I], PointData->Metadata, Vec3Attribute, RandomSource.VRand());
			UPCGMetadataAccessorHelpers::SetVector4Attribute(SourcePoints[I], PointData->Metadata, Vec4Attribute, FVector4(RandomSource.VRand(), RandomSource.FRand()));
			UPCGMetadataAccessorHelpers::SetRotatorAttribute(SourcePoints[I], PointData->Metadata, RotatorAttribute, FRotator(RandomSource.FRand(), RandomSource.FRand(), RandomSource.FRand()));
			UPCGMetadataAccessorHelpers::SetFloatAttribute(SourcePoints[I], PointData->Metadata, FloatAttribute, RandomSource.FRand());
		}
	}

	void GenerateParamData(PCGTestsCommon::FTestData& TestData)
	{
		FPCGTaggedData& ParamTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
		TObjectPtr<UPCGParamData> ParamData = PCGTestsCommon::CreateEmptyParamData();
		ParamTaggedData.Data = ParamData;
		ParamTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

		PCGMetadataEntryKey EntryKey = ParamData->Metadata->AddEntry();
		const bool bAllowsInterpolation = false;
		const bool bOverrideParent = false;

		ParamData->Metadata->CreateVector2Attribute(Vec2Attribute, FVector2D::ZeroVector, bAllowsInterpolation, bOverrideParent);
		ParamData->Metadata->CreateVectorAttribute(Vec3Attribute, FVector::Zero(), bAllowsInterpolation, bOverrideParent);
		ParamData->Metadata->CreateVector4Attribute(Vec4Attribute, FVector4::Zero(), bAllowsInterpolation, bOverrideParent);
		ParamData->Metadata->CreateRotatorAttribute(RotatorAttribute, FRotator::ZeroRotator, bAllowsInterpolation, bOverrideParent);
		ParamData->Metadata->CreateFloatAttribute(FloatAttribute, 0, bAllowsInterpolation, bOverrideParent);

		// Randomize attribute values
		FRandomStream RandomSource(TestData.Seed);
		if (FPCGMetadataAttribute<FVector2D>* Attribute = static_cast<FPCGMetadataAttribute<FVector2D>*>(ParamData->Metadata->GetMutableAttribute(Vec2Attribute)))
		{
			Attribute->SetValue(EntryKey, FVector2D(RandomSource.VRand()));
		}

		if (FPCGMetadataAttribute<FVector>* Attribute = static_cast<FPCGMetadataAttribute<FVector>*>(ParamData->Metadata->GetMutableAttribute(Vec3Attribute)))
		{
			Attribute->SetValue(EntryKey, RandomSource.VRand());
		}

		if (FPCGMetadataAttribute<FVector4>* Attribute = static_cast<FPCGMetadataAttribute<FVector4>*>(ParamData->Metadata->GetMutableAttribute(Vec4Attribute)))
		{
			Attribute->SetValue(EntryKey, FVector4(RandomSource.VRand(), RandomSource.FRand()));
		}

		if (FPCGMetadataAttribute<FRotator>* Attribute = static_cast<FPCGMetadataAttribute<FRotator>*>(ParamData->Metadata->GetMutableAttribute(RotatorAttribute)))
		{
			Attribute->SetValue(EntryKey, FRotator(RandomSource.FRand(), RandomSource.FRand(), RandomSource.FRand()));
		}

		if (FPCGMetadataAttribute<float>* Attribute = static_cast<FPCGMetadataAttribute<float>*>(ParamData->Metadata->GetMutableAttribute(FloatAttribute)))
		{
			Attribute->SetValue(EntryKey, RandomSource.FRand());
		}
	}
}


bool FPCGMetadataBreakVectorTest::RunTest(const FString& Parameters)
{	
	auto ValidateMetadataBreakVector = [this](PCGTestsCommon::FTestData& TestData, bool bIsValid = true) -> bool
	{
		const FName OutputAttributeName = TEXT("Output");

		UPCGMetadataBreakVectorSettings* Settings = CastChecked<UPCGMetadataBreakVectorSettings>(TestData.Settings);
		Settings->OutputAttributeName = OutputAttributeName;
		FPCGElementPtr MetadataBreakVectorElement = TestData.Settings->GetElement();

		TUniquePtr<FPCGContext> Context = TUniquePtr<FPCGContext>(MetadataBreakVectorElement->Initialize(TestData.InputData, TestData.TestPCGComponent, nullptr));
		Context->NumAvailableTasks = 1;

		while (!MetadataBreakVectorElement->Execute(Context.Get()))
		{}

		// If the test is not valid, just check that we have no output and early out
		if (!bIsValid)
		{
			return TestTrue(TEXT("Invalid operation, no outputs"), Context->OutputData.GetInputs().IsEmpty());
		}

		const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

		if (!TestEqual("Right number of inputs", Inputs.Num(), 1))
		{
			return false;
		}

		bool bTestPassed = true;

		const FPCGTaggedData& Input = Inputs[0];

		UPCGMetadata* SourceMetadata = nullptr; 
		if (const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data))
		{
			SourceMetadata = SpatialInput->Metadata;
		}
		else if (const UPCGParamData* ParamsInput = Cast<const UPCGParamData>(Input.Data))
		{
			SourceMetadata = ParamsInput->Metadata;
		}
		else
		{
			return false;
		}

		check(SourceMetadata);

		const FPCGMetadataAttributeBase* SourceAttributeBase = SourceMetadata->GetConstAttribute(Settings->InputAttributeName);
		check(SourceAttributeBase);

		auto ValidateComponentOutput = [&](const FPCGTaggedData& Output, const FName& OutAttributeName, PCGBreakVectorTest::EPCGComponentToCheck ComponentToCheck)
		{
			if (!TestTrue("Valid output data", Output.Data != nullptr))
			{
				bTestPassed = false;
				return;
			}

			UPCGMetadata* OutMetadata = nullptr;
			if (const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Output.Data))
			{
				OutMetadata = SpatialInput->Metadata;
			}
			else if (const UPCGParamData* ParamsInput = Cast<const UPCGParamData>(Output.Data))
			{
				OutMetadata = ParamsInput->Metadata;
			}
			else
			{
				return;
			}

			if (!TestNotNull("Valid output metadata", OutMetadata))
			{
				bTestPassed = false;
				return;
			}

			const FPCGMetadataAttributeBase* OutAttributeBase = OutMetadata->GetConstAttribute(OutAttributeName);

			if (!TestNotNull("Valid output attribute", OutAttributeBase))
			{
				bTestPassed = false;
				return;
			}

			if (!TestEqual("Output attribute is a valid type (double)", OutAttributeBase->GetTypeId(), PCG::Private::MetadataTypes<double>::Id))
			{
				bTestPassed = false;
				return;
			}

			const FPCGMetadataAttribute<double>* OutAttribute = static_cast<const FPCGMetadataAttribute<double>*>(OutAttributeBase);
			check(OutAttribute);

			const PCGMetadataEntryKey InEntryKeyCount = SourceMetadata->GetItemCountForChild();
			const PCGMetadataEntryKey OutEntryKeyCount = OutMetadata->GetItemCountForChild();

			if (!TestEqual("Identical EntryKey counts", InEntryKeyCount, OutEntryKeyCount))
			{
				bTestPassed = false;
				return;
			}

			const PCGMetadataValueKey InValueKeyCount = SourceAttributeBase->GetValueKeyOffsetForChild();
			const PCGMetadataValueKey OutValueKeyCount = OutAttribute->GetValueKeyOffsetForChild();

			if (!TestEqual("Identical ValueKey counts", InValueKeyCount, OutValueKeyCount))
			{
				bTestPassed = false;
				return;
			}

			for (PCGMetadataEntryKey EntryKey = 0; EntryKey < InEntryKeyCount; ++EntryKey)
			{
				const PCGMetadataValueKey InValueKey = SourceAttributeBase->GetValueKey(EntryKey);
				const PCGMetadataValueKey OutValueKey = OutAttribute->GetValueKey(EntryKey);

				bTestPassed &= TestEqual("Identical value keys", InValueKey, OutValueKey);

				FVector4 SourceValue;
				if (SourceAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FVector>::Id)
				{
					SourceValue = static_cast<const FPCGMetadataAttribute<FVector>*>(SourceAttributeBase)->GetValue(InValueKey);
				}
				else if (SourceAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FVector2D>::Id)
				{
					const FVector2D Vec2 = static_cast<const FPCGMetadataAttribute<FVector2D>*>(SourceAttributeBase)->GetValue(InValueKey);
					SourceValue = FVector4(Vec2.X, Vec2.Y);
				}
				else if (SourceAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FVector4>::Id)
				{
					SourceValue = static_cast<const FPCGMetadataAttribute<FVector4>*>(SourceAttributeBase)->GetValue(InValueKey);
				}
				else //if (SourceAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FRotator>::Id)
				{
					const FRotator Rotator = static_cast<const FPCGMetadataAttribute<FRotator>*>(SourceAttributeBase)->GetValue(InValueKey);
					SourceValue.X = Rotator.Roll;
					SourceValue.Y = Rotator.Pitch;
					SourceValue.Z = Rotator.Yaw;
				}

				const double OutValue = OutAttribute->GetValue(OutValueKey);

				if (ComponentToCheck == PCGBreakVectorTest::EPCGComponentToCheck::X)
				{
					bTestPassed &= TestEqual("X", SourceValue.X, OutValue);
				}
				else if (ComponentToCheck == PCGBreakVectorTest::EPCGComponentToCheck::Y)
				{
					bTestPassed &= TestEqual("Y", SourceValue.Y, OutValue);
				}
				else if (ComponentToCheck == PCGBreakVectorTest::EPCGComponentToCheck::Z)
				{
					bTestPassed &= TestEqual("Z", SourceValue.Z, OutValue);
				}
				else if (ComponentToCheck == PCGBreakVectorTest::EPCGComponentToCheck::W)
				{
					bTestPassed &= TestEqual("W", SourceValue.W, OutValue);
				}
			}
		};

		const TArray<FPCGTaggedData> OutputsX = Context->OutputData.GetInputsByPin(PCGMetadataBreakVectorConstants::XLabel);
		const TArray<FPCGTaggedData> OutputsY = Context->OutputData.GetInputsByPin(PCGMetadataBreakVectorConstants::YLabel);
		const TArray<FPCGTaggedData> OutputsZ = Context->OutputData.GetInputsByPin(PCGMetadataBreakVectorConstants::ZLabel);
		const TArray<FPCGTaggedData> OutputsW = Context->OutputData.GetInputsByPin(PCGMetadataBreakVectorConstants::WLabel);

		const FName DestinationAttributeForX = Settings->GetOutputAttributeName(Settings->OutputAttributeName, 0);
		const FName DestinationAttributeForY = Settings->GetOutputAttributeName(Settings->OutputAttributeName, 1);
		const FName DestinationAttributeForZ = Settings->GetOutputAttributeName(Settings->OutputAttributeName, 2);
		const FName DestinationAttributeForW = Settings->GetOutputAttributeName(Settings->OutputAttributeName, 3);

		if (Settings->ForceOutputConnections[0])
		{
			if (TestEqual("Appropriate number of outputs generated fox X", OutputsX.Num(), 1))
			{
				ValidateComponentOutput(OutputsX[0], DestinationAttributeForX, PCGBreakVectorTest::EPCGComponentToCheck::X);
			}
			else
			{
				bTestPassed = false;
			}
		}

		if (Settings->ForceOutputConnections[1])
		{
			if (TestEqual("Appropriate number of outputs generated for Y", OutputsY.Num(), 1))
			{
				ValidateComponentOutput(OutputsY[0], DestinationAttributeForY, PCGBreakVectorTest::EPCGComponentToCheck::Y);
			}
			else
			{
				bTestPassed = false;
			}
		}

		if (Settings->ForceOutputConnections[2])
		{
			if (TestEqual("Appropriate number of outputs generated for Z", OutputsZ.Num(), 1))
			{
				ValidateComponentOutput(OutputsZ[0], DestinationAttributeForZ, PCGBreakVectorTest::EPCGComponentToCheck::Z);
			}
			else
			{
				bTestPassed = false;
			}
		}

		if (Settings->ForceOutputConnections[3] && SourceAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FVector4>::Id)
		{
			if (TestEqual("Appropriate number of outputs generated for W", OutputsW.Num(), 1))
			{
				ValidateComponentOutput(OutputsW[0], DestinationAttributeForW, PCGBreakVectorTest::EPCGComponentToCheck::W);
			}
			else
			{
				bTestPassed = false;
			}
		}

		return bTestPassed;
	};

	bool bTestPassed = true;

	PCGTestsCommon::FTestData TestDataSpatial(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGMetadataBreakVectorSettings>(TestDataSpatial);
	PCGBreakVectorTest::GenerateSpatialData(TestDataSpatial);

	PCGTestsCommon::FTestData TestDataParams(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGMetadataBreakVectorSettings>(TestDataParams);
	PCGBreakVectorTest::GenerateParamData(TestDataParams);

	using PairWhatData = TPair<FString, PCGTestsCommon::FTestData*>;

	TStaticArray<PairWhatData, 2> AllTestData;
	AllTestData[0] = PairWhatData("Testing with point data as input", &TestDataSpatial);
	AllTestData[1] = PairWhatData("Testing with param data as input", &TestDataParams);

	// Setup error catching. This error should happen only twice (invalid type)
	AddExpectedError(TEXT("Attribute Float is not a supported type for input 0"), EAutomationExpectedErrorFlags::Contains, 2);

	for (PairWhatData& PairTestData : AllTestData)
	{
		AddInfo(PairTestData.Key);
		PCGTestsCommon::FTestData* TestData = PairTestData.Value;

		UPCGMetadataBreakVectorSettings* Settings = CastChecked<UPCGMetadataBreakVectorSettings>(TestData->Settings);

		Settings->ForceOutputConnections[0] = true;
		Settings->ForceOutputConnections[1] = true;
		Settings->ForceOutputConnections[2] = false;
		Settings->ForceOutputConnections[3] = false;

		{
			AddInfo(TEXT("Test with FVector2D as input attribute"));
			Settings->InputAttributeName = PCGBreakVectorTest::Vec2Attribute;
			bTestPassed &= ValidateMetadataBreakVector(*TestData);
		}

		Settings->ForceOutputConnections[2] = true;

		{
			AddInfo(TEXT("Test with FVector as input attribute"));
			Settings->InputAttributeName = PCGBreakVectorTest::Vec3Attribute;
			bTestPassed &= ValidateMetadataBreakVector(*TestData);
		}

		{
			AddInfo(TEXT("Test with FVector4 as input attribute"));
			Settings->InputAttributeName = PCGBreakVectorTest::Vec4Attribute;
			Settings->ForceOutputConnections[3] = true;
			bTestPassed &= ValidateMetadataBreakVector(*TestData);
			Settings->ForceOutputConnections[3] = false;
		}

		{
			AddInfo(TEXT("Test with FRotator as input attribute"));
			Settings->InputAttributeName = PCGBreakVectorTest::RotatorAttribute;
			bTestPassed &= ValidateMetadataBreakVector(*TestData);
		}

		{
			AddInfo(TEXT("Test with Float as input attribute (invalid type)"));
			Settings->InputAttributeName = PCGBreakVectorTest::FloatAttribute;

			bTestPassed &= ValidateMetadataBreakVector(*TestData, /*bIsValid=*/false);
		}
	}

	return bTestPassed;
}

#endif // WITH_EDITOR
