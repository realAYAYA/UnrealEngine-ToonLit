// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"
#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGParamData.h"

#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataMakeTransform.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#if WITH_EDITOR

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataMakeTransformTest, FPCGTestBaseClass, "pcg.tests.Metadata.MakeTransform", PCGTestsCommon::TestFlags)

namespace
{
	const FName Vec3Attribute = TEXT("Vec3");
	const FName QuatAttribute = TEXT("Quat");

	void ConnectData(FPCGDataCollection& InputData, TObjectPtr<UPCGData> Data)
	{
		FPCGTaggedData& TaggedData1 = InputData.TaggedData.Emplace_GetRef();
		TaggedData1.Data = Data;
		TaggedData1.Pin = PCGMetadataTransformConstants::Translation;

		FPCGTaggedData& TaggedData2 = InputData.TaggedData.Emplace_GetRef();
		TaggedData2.Data = Data;
		TaggedData2.Pin = PCGMetadataTransformConstants::Rotation;

		FPCGTaggedData& TaggedData3 = InputData.TaggedData.Emplace_GetRef();
		TaggedData3.Data = Data;
		TaggedData3.Pin = PCGMetadataTransformConstants::Scale;
	}

	void GenerateSpatialData(PCGTestsCommon::FTestData& TestData)
	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreateRandomPointData(10, TestData.Seed);
		
		ConnectData(TestData.InputData, PointData);

		const bool bAllowsInterpolation = false;
		const bool bOverrideParent = false;

		PointData->Metadata->CreateVectorAttribute(Vec3Attribute, FVector::ZeroVector, bAllowsInterpolation, bOverrideParent);
		PointData->Metadata->CreateQuatAttribute(QuatAttribute, FQuat::Identity, bAllowsInterpolation, bOverrideParent);

		TArray<FPCGPoint>& SourcePoints = PointData->GetMutablePoints();

		// Randomize attribute values, and leave the second half to default value
		FRandomStream RandomSource(TestData.Seed);
		for (int I = 0; I < SourcePoints.Num() / 2; ++I)
		{
			UPCGMetadataAccessorHelpers::SetVectorAttribute(SourcePoints[I], PointData->Metadata, Vec3Attribute, RandomSource.VRand());
			UPCGMetadataAccessorHelpers::SetQuatAttribute(SourcePoints[I], PointData->Metadata, QuatAttribute, FRotator(RandomSource.FRand(), RandomSource.FRand(), RandomSource.FRand()).Quaternion());
		}
	}

	void GenerateParamData(PCGTestsCommon::FTestData& TestData)
	{
		TObjectPtr<UPCGParamData> ParamData = PCGTestsCommon::CreateEmptyParamData();

		ConnectData(TestData.InputData, ParamData);

		PCGMetadataEntryKey EntryKey = ParamData->Metadata->AddEntry();
		const bool bAllowsInterpolation = false;
		const bool bOverrideParent = false;

		ParamData->Metadata->CreateVectorAttribute(Vec3Attribute, FVector::ZeroVector, bAllowsInterpolation, bOverrideParent);
		ParamData->Metadata->CreateQuatAttribute(QuatAttribute, FQuat::Identity, bAllowsInterpolation, bOverrideParent);

		// Randomize attribute values
		FRandomStream RandomSource(TestData.Seed);
		if (FPCGMetadataAttribute<FVector>* Attribute = static_cast<FPCGMetadataAttribute<FVector>*>(ParamData->Metadata->GetMutableAttribute(Vec3Attribute)))
		{
			Attribute->SetValue(EntryKey, RandomSource.VRand());
		}

		if (FPCGMetadataAttribute<FQuat>* Attribute = static_cast<FPCGMetadataAttribute<FQuat>*>(ParamData->Metadata->GetMutableAttribute(QuatAttribute)))
		{
			Attribute->SetValue(EntryKey, FRotator(RandomSource.FRand(), RandomSource.FRand(), RandomSource.FRand()).Quaternion());
		}
	}
}


bool FPCGMetadataMakeTransformTest::RunTest(const FString& Parameters)
{	
	auto ValidateMetadataMakeVector = [this](PCGTestsCommon::FTestData& TestData) -> bool
	{
		const FName OutputAttributeName = TEXT("Output");

		UPCGMetadataMakeTransformSettings* Settings = CastChecked<UPCGMetadataMakeTransformSettings>(TestData.Settings);
		Settings->OutputAttributeName = OutputAttributeName;
		FPCGElementPtr MetadataBreakTransformElement = TestData.Settings->GetElement();

		TUniquePtr<FPCGContext> Context = TUniquePtr<FPCGContext>(MetadataBreakTransformElement->Initialize(TestData.InputData, TestData.TestPCGComponent, nullptr));
		Context->NumAvailableTasks = 1;

		while (!MetadataBreakTransformElement->Execute(Context.Get()))
		{}

		// We have the same metadata to all pins, so just check the first one
		const TArray<FPCGTaggedData>& TranslationInputs = Context->InputData.GetInputsByPin(PCGMetadataTransformConstants::Translation);

		if (!TestEqual("Right number of translation inputs", TranslationInputs.Num(), 1))
		{
			return false;
		}

		const FPCGTaggedData& Input = TranslationInputs[0];

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

		const FPCGMetadataAttributeBase* TranslationAttributeBase = SourceMetadata->GetConstAttribute(Settings->Input1AttributeName);
		check(TranslationAttributeBase);

		const FPCGMetadataAttributeBase* RotationAttributeBase = SourceMetadata->GetConstAttribute(Settings->Input2AttributeName);
		check(RotationAttributeBase);

		const FPCGMetadataAttributeBase* ScaleAttributeBase = SourceMetadata->GetConstAttribute(Settings->Input3AttributeName);
		check(ScaleAttributeBase);

		const TArray<FPCGTaggedData> Outputs = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);
		if (!TestEqual("Right number of outputs", Outputs.Num(), 1))
		{
			return false;
		}

		const FPCGTaggedData& Output = Outputs[0];

		if (!TestTrue("Valid output data", Output.Data != nullptr))
		{
			return false;
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
			return false;
		}

		if (!TestNotNull("Valid output metadata", OutMetadata))
		{
			return false;
		}

		const FPCGMetadataAttributeBase* OutAttributeBase = OutMetadata->GetConstAttribute(Settings->OutputAttributeName);

		if (!TestNotNull("Valid output attribute", OutAttributeBase))
		{
			return false;
		}

		const FPCGMetadataAttribute<FTransform>* OutAttribute = static_cast<const FPCGMetadataAttribute<FTransform>*>(OutAttributeBase);
		check(OutAttribute);

		const PCGMetadataEntryKey InEntryKeyCount = SourceMetadata->GetItemCountForChild();
		const PCGMetadataEntryKey OutEntryKeyCount = OutMetadata->GetItemCountForChild();

		if (!TestEqual("Identical EntryKey counts", InEntryKeyCount, OutEntryKeyCount))
		{
			return false;
		}

		const PCGMetadataValueKey InValueKeyCount = TranslationAttributeBase->GetValueKeyOffsetForChild();
		const PCGMetadataValueKey OutValueKeyCount = OutAttribute->GetValueKeyOffsetForChild();

		if (!TestEqual("Identical ValueKey counts", InValueKeyCount, OutValueKeyCount))
		{
			return false;
		}

		for (PCGMetadataEntryKey EntryKey = 0; EntryKey < InEntryKeyCount; ++EntryKey)
		{
			const PCGMetadataValueKey InValueKey = TranslationAttributeBase->GetValueKey(EntryKey);
			const PCGMetadataValueKey OutValueKey = OutAttribute->GetValueKey(EntryKey);

			if (!TestEqual("Identical value keys", InValueKey, OutValueKey))
			{
				return false;
			}

			FVector TranslationValue = static_cast<const FPCGMetadataAttribute<FVector>*>(TranslationAttributeBase)->GetValue(InValueKey);
			FQuat QuatValue = static_cast<const FPCGMetadataAttribute<FQuat>*>(RotationAttributeBase)->GetValue(InValueKey);
			FVector ScaleValue = static_cast<const FPCGMetadataAttribute<FVector>*>(ScaleAttributeBase)->GetValue(InValueKey);
			
			FTransform OutValue = OutAttribute->GetValue(OutValueKey);

			if (!TestEqual("Mismatch between input and output for translation", OutValue.GetTranslation(), TranslationValue))
			{
				return false;
			}

			if (!TestEqual("Mismatch between input and output for rotation", OutValue.GetRotation(), QuatValue))
			{
				return false;
			}

			if (!TestEqual("Mismatch between input and output for scale", OutValue.GetScale3D(), ScaleValue))
			{
				return false;
			}
		}

		return true;
	};

	bool bTestPassed = true;

	PCGTestsCommon::FTestData TestDataSpatial(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGMetadataMakeTransformSettings>(TestDataSpatial);
	GenerateSpatialData(TestDataSpatial);

	PCGTestsCommon::FTestData TestDataParams(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGMetadataMakeTransformSettings>(TestDataParams);
	GenerateParamData(TestDataParams);

	using PairWhatData = TPair<FString, PCGTestsCommon::FTestData*>;

	TStaticArray<PairWhatData, 2> AllTestData;
	AllTestData[0] = PairWhatData("Testing with point data as input", &TestDataSpatial);
	AllTestData[1] = PairWhatData("Testing with param data as input", &TestDataParams);

	for (PairWhatData& PairTestData : AllTestData)
	{
		AddInfo(PairTestData.Key);
		PCGTestsCommon::FTestData* TestData = PairTestData.Value;

		UPCGMetadataMakeTransformSettings* Settings = CastChecked<UPCGMetadataMakeTransformSettings>(TestData->Settings);

		Settings->ForceOutputConnections[0] = true;
		Settings->Input1AttributeName = Vec3Attribute;
		Settings->Input2AttributeName = QuatAttribute;
		Settings->Input3AttributeName = Vec3Attribute;

		bTestPassed &= ValidateMetadataMakeVector(*TestData);
	}

	return bTestPassed;
}

#endif // WITH_EDITOR
