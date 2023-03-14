// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"
#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGHelpers.h"

#include "Data/PCGPointData.h"
#include "Elements/PCGCopyPoints.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#if WITH_EDITOR

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCopyPointsTest, FPCGTestBaseClass, "pcg.tests.CopyPoints.Basic", PCGTestsCommon::TestFlags)

bool FPCGCopyPointsTest::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGCopyPointsSettings>(TestData);
	UPCGCopyPointsSettings* Settings = CastChecked<UPCGCopyPointsSettings>(TestData.Settings);
	FPCGElementPtr CopyPointsElement = TestData.Settings->GetElement();

	TObjectPtr<UPCGPointData> EmptyData = PCGTestsCommon::CreateEmptyPointData();
	TObjectPtr<UPCGPointData> SourceData = PCGTestsCommon::CreateRandomPointData(10, TestData.Seed);
	TObjectPtr<UPCGPointData> TargetData = PCGTestsCommon::CreateRandomPointData(4, TestData.Seed+1);

	const FName LengthName = TEXT("Length");
	const FName WidthName = TEXT("Width");
	const FName HeightName = TEXT("Height");

	const bool bAllowsInterpolation = false;
	const bool bOverrideParent = false;

	SourceData->Metadata->CreateFloatAttribute(LengthName, 0, bAllowsInterpolation, bOverrideParent);
	SourceData->Metadata->CreateFloatAttribute(WidthName, 0, bAllowsInterpolation, bOverrideParent);
	TargetData->Metadata->CreateFloatAttribute(LengthName, 1, bAllowsInterpolation, bOverrideParent);
	TargetData->Metadata->CreateFloatAttribute(HeightName, 1, bAllowsInterpolation, bOverrideParent);

	TArray<FPCGPoint>& SourcePoints = SourceData->GetMutablePoints();
	TArray<FPCGPoint>& TargetPoints = TargetData->GetMutablePoints();

	FRandomStream RandomSource(TestData.Seed);
	for (int I = 0; I < SourcePoints.Num(); ++I)
	{
		// first half of the points have a non-default Length attribute value, the rest have a non-default width attribute value
	 	UPCGMetadataAccessorHelpers::SetFloatAttribute(SourcePoints[I], SourceData->Metadata, (I < SourcePoints.Num() / 2) ? LengthName : WidthName, RandomSource.FRand());
	}

	for (int I = 0; I < TargetPoints.Num(); ++I)
	{
		// first quarter of the points have a non-default Length attribute value, the rest have a non-default Height attribute value
		UPCGMetadataAccessorHelpers::SetFloatAttribute(TargetPoints[I], TargetData->Metadata, (I < SourcePoints.Num() / 4) ? LengthName : HeightName, RandomSource.FRand());
	}

	FPCGTaggedData& SourceTaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	SourceTaggedData.Data = SourceData;
	SourceTaggedData.Pin = PCGCopyPointsConstants::SourcePointsLabel;

	FPCGTaggedData& TargetTaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	TargetTaggedData.Data = TargetData;
	TargetTaggedData.Pin = PCGCopyPointsConstants::TargetPointsLabel;

	// Test only supports float attributes for simplicity
	auto ValidateCopyPointsPoints = [this, &TestData, CopyPointsElement, Settings]() -> bool
	{
		TUniquePtr<FPCGContext> Context = MakeUnique<FPCGContext>(*CopyPointsElement->Initialize(TestData.InputData, TestData.TestPCGComponent, nullptr));
		Context->NumAvailableTasks = 1;

		while (!CopyPointsElement->Execute(Context.Get()))
		{}

		const TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGCopyPointsConstants::SourcePointsLabel);
		const TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGCopyPointsConstants::TargetPointsLabel);
		const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

		if (!TestTrue("Valid number of inputs", Sources.Num() == 1 && Targets.Num() == 1))
		{
			return false;
		}

		if (!TestTrue("Valid number of outputs", Sources.Num() == 1))
		{
			return false;
		}

		const FPCGTaggedData& Source = Sources[0];
		const FPCGTaggedData& Target = Targets[0];
		const FPCGTaggedData& Output = Outputs[0];

		check(Source.Data && Target.Data);

		const UPCGSpatialData* SourceSpatialData = Cast<UPCGSpatialData>(Source.Data);
		const UPCGSpatialData* TargetSpatialData = Cast<UPCGSpatialData>(Target.Data);

		check(SourceSpatialData && TargetSpatialData);

		const UPCGPointData* SourcePointData = SourceSpatialData->ToPointData(Context.Get());
		const UPCGPointData* TargetPointData = TargetSpatialData->ToPointData(Context.Get());

		check(SourcePointData && TargetPointData);

		if (!TestTrue("Valid output data", Output.Data != nullptr))
		{
			return false;
		}

		const UPCGSpatialData* OutSpatialData = Cast<UPCGSpatialData>(Output.Data);

		if (!TestNotNull("Valid output SpatialData", OutSpatialData))
		{
			return false;
		}

		const UPCGPointData* OutPointData = OutSpatialData->ToPointData(Context.Get());

		if (!TestNotNull("Valid output PointData", OutPointData))
		{
			return false;
		}

		const TArray<FPCGPoint>& SourcePoints = SourcePointData->GetPoints();
		const TArray<FPCGPoint>& TargetPoints = TargetPointData->GetPoints();
		const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

		if (!TestEqual("Valid number of output points", SourcePoints.Num() * TargetPoints.Num(), OutPoints.Num()))
		{ 
			return false;
		}

		const UPCGMetadata* RootMetadata = nullptr;
		const UPCGMetadata* NonRootMetadata = nullptr;
		EPCGCopyPointsMetadataInheritanceMode RootMode;
		EPCGCopyPointsMetadataInheritanceMode NonRootMode;

		if (Settings->AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::Source)
		{
			RootMetadata = SourcePointData->Metadata;
			NonRootMetadata = TargetPointData->Metadata;
			RootMode = EPCGCopyPointsMetadataInheritanceMode::Source;
			NonRootMode = EPCGCopyPointsMetadataInheritanceMode::Target;
		}
		else // if (Settings->AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::Target)
		{
			RootMetadata = TargetPointData->Metadata;
			NonRootMetadata = SourcePointData->Metadata;
			RootMode = EPCGCopyPointsMetadataInheritanceMode::Target;
			NonRootMode = EPCGCopyPointsMetadataInheritanceMode::Source;
		}

		if (!TestTrue("Valid input metadata", RootMetadata && NonRootMetadata))
		{
			return false;
		}

		CA_ASSUME(RootMetadata); // Static analyzer doesn't understand semantics of TestTrue.
		CA_ASSUME(NonRootMetadata);
		
		bool bTestPassed = true;

		TArray<const FPCGMetadataAttribute<float>*> InheritedAttributes;
		TArray<EPCGCopyPointsMetadataInheritanceMode> InheritedAttributeModes;
		TArray<const FPCGMetadataAttribute<float>*> OutAttributes;

		TArray<FName> AttributeNames;
		TArray<EPCGMetadataTypes> AttributeTypes;
		RootMetadata->GetAttributes(AttributeNames, AttributeTypes);

		for (const FName& AttributeName : AttributeNames)
		{
			if (TestTrue("Contains attribute found in source", OutPointData->Metadata->HasAttribute(AttributeName)))
			{
				const FPCGMetadataAttribute<float>* InheritedAttribute = static_cast<const FPCGMetadataAttribute<float>*>(RootMetadata->GetConstAttribute(AttributeName));
				const FPCGMetadataAttribute<float>* OutAttribute = static_cast<const FPCGMetadataAttribute<float>*>(OutPointData->Metadata->GetConstAttribute(AttributeName));

				InheritedAttributes.Add(InheritedAttribute);
				InheritedAttributeModes.Add(RootMode);
				OutAttributes.Add(OutAttribute);
			}
			else
			{
				bTestPassed = false;
			}
		}

		NonRootMetadata->GetAttributes(AttributeNames, AttributeTypes);

		for (const FName& AttributeName : AttributeNames)
		{
			if (!RootMetadata->HasAttribute(AttributeName))
			{
				if (TestTrue("Contains attribute found in target", OutPointData->Metadata->HasAttribute(AttributeName)))
				{
					const FPCGMetadataAttribute<float>* InheritedAttribute = static_cast<const FPCGMetadataAttribute<float>*>(NonRootMetadata->GetConstAttribute(AttributeName));
					const FPCGMetadataAttribute<float>* OutAttribute = static_cast<const FPCGMetadataAttribute<float>*>(OutPointData->Metadata->GetConstAttribute(AttributeName));

					InheritedAttributes.Add(InheritedAttribute);
					InheritedAttributeModes.Add(NonRootMode);
					OutAttributes.Add(OutAttribute);
				}
				else
				{
					bTestPassed = false;
				}
			}
		}

		check(InheritedAttributeModes.Num() == InheritedAttributeModes.Num() && InheritedAttributeModes.Num() == OutAttributes.Num())

		for (int PointIndex = 0; PointIndex < OutPoints.Num(); ++PointIndex)
		{
			const FPCGPoint& SourcePoint = SourcePoints[PointIndex / TargetPoints.Num()];
			const FPCGPoint& TargetPoint = TargetPoints[PointIndex % TargetPoints.Num()];
			const FPCGPoint& OutPoint = OutPoints[PointIndex];

			bTestPassed &= TestEqual("SourcePoint and OutPoint have same Density", SourcePoint.Density, OutPoint.Density);
			bTestPassed &= TestEqual("SourcePoint and OutPoint have same BoundsMin", SourcePoint.BoundsMin, OutPoint.BoundsMin);
			bTestPassed &= TestEqual("SourcePoint and OutPoint have same BoundsMax", SourcePoint.BoundsMax, OutPoint.BoundsMax);
			bTestPassed &= TestEqual("SourcePoint and OutPoint have same Steepness", SourcePoint.Steepness, OutPoint.Steepness);

			// Validate transform inheritance
			const FTransform& SourceTransform = SourcePoint.Transform;
			const FTransform& TargetTransform = TargetPoint.Transform;
			const FTransform& OutTransform = OutPoint.Transform;

			const FVector Location = TargetPoint.Transform.TransformPosition(SourcePoint.Transform.GetLocation());
			FQuat Rotation;
			FVector Scale;
			FVector4 Color;
			int32 Seed;

			if (Settings->RotationInheritance == EPCGCopyPointsInheritanceMode::Relative)
			{
				Rotation = TargetTransform.GetRotation() * SourceTransform.GetRotation();
			}
			else if (Settings->RotationInheritance == EPCGCopyPointsInheritanceMode::Source)
			{
				Rotation = SourceTransform.GetRotation();
			}
			else // if (Settings->RotationInheritance == EPCGCopyPointsInheritanceMode::Target)
			{
				Rotation = TargetTransform.GetRotation();
			}

			if (Settings->ScaleInheritance == EPCGCopyPointsInheritanceMode::Relative)
			{
				Scale = SourceTransform.GetScale3D() * TargetTransform.GetScale3D();
			}
			else if (Settings->ScaleInheritance == EPCGCopyPointsInheritanceMode::Source)
			{
				Scale = SourceTransform.GetScale3D();
			}
			else // if (Settings->ScaleInheritance == EPCGCopyPointsInheritanceMode::Target)
			{
				Scale = TargetTransform.GetScale3D();
			}

			if (Settings->ColorInheritance == EPCGCopyPointsInheritanceMode::Relative)
			{
				Color = SourcePoint.Color * TargetPoint.Color;
			}
			else if (Settings->ColorInheritance == EPCGCopyPointsInheritanceMode::Source)
			{
				Color = SourcePoint.Color;
			}
			else // if (Settings->ColorInheritance == EPCGCopyPointsInheritanceMode::Target)
			{
				Color = TargetPoint.Color;
			}

			if (Settings->ColorInheritance == EPCGCopyPointsInheritanceMode::Relative)
			{
				Seed = PCGHelpers::ComputeSeed((int)Location.X, (int)Location.Y, (int)Location.Z);
			}
			else if (Settings->ColorInheritance == EPCGCopyPointsInheritanceMode::Source)
			{
				Seed = SourcePoint.Seed;
			}
			else // if (Settings->ColorInheritance == EPCGCopyPointsInheritanceMode::Target)
			{
				Seed = TargetPoint.Seed;
			}

			bTestPassed &= TestEqual("Valid rotation", Rotation, OutTransform.GetRotation());
			bTestPassed &= TestEqual("Valid scale", Scale, OutTransform.GetScale3D());
			bTestPassed &= TestEqual("Valid color", Color, OutPoint.Color);
			bTestPassed &= TestEqual("Valid location", Location, OutTransform.GetLocation());
			bTestPassed &= TestEqual("Valid location", Seed, OutPoint.Seed);		

			// Validate point value keys
			for (int AttributeIndex = 0; AttributeIndex < InheritedAttributes.Num(); ++AttributeIndex)
			{
				const FPCGMetadataAttribute<float>* InheritedAttribute = InheritedAttributes[AttributeIndex];
				const FPCGMetadataAttribute<float>* OutAttribute = OutAttributes[AttributeIndex];
				check(InheritedAttribute && OutAttribute);

				const PCGMetadataEntryKey EntryKey = (InheritedAttributeModes[AttributeIndex] == EPCGCopyPointsMetadataInheritanceMode::Source) ? SourcePoint.MetadataEntry : TargetPoint.MetadataEntry;
				bTestPassed &= TestEqual("Valid metadata value", InheritedAttribute->GetValueKey(EntryKey), OutAttribute->GetValueKey(OutPoint.MetadataEntry));
			}
		}

		return bTestPassed;
	};

	bool bTestPassed = true;

	// Test 1 - inherit from transform multiplication
	Settings->RotationInheritance = EPCGCopyPointsInheritanceMode::Relative;
	Settings->ScaleInheritance = EPCGCopyPointsInheritanceMode::Relative;
	Settings->ColorInheritance = EPCGCopyPointsInheritanceMode::Relative;
	Settings->SeedInheritance = EPCGCopyPointsInheritanceMode::Relative;
	bTestPassed &= ValidateCopyPointsPoints();

	// Test 2 - inherit from source points
	Settings->RotationInheritance = EPCGCopyPointsInheritanceMode::Source;
	Settings->ScaleInheritance = EPCGCopyPointsInheritanceMode::Source;
	Settings->ColorInheritance = EPCGCopyPointsInheritanceMode::Source;
	Settings->SeedInheritance = EPCGCopyPointsInheritanceMode::Source;
	Settings->AttributeInheritance = EPCGCopyPointsMetadataInheritanceMode::Source;
	bTestPassed &= ValidateCopyPointsPoints();

	// Test 3 - inherit from target points
	Settings->RotationInheritance = EPCGCopyPointsInheritanceMode::Target;
	Settings->ScaleInheritance = EPCGCopyPointsInheritanceMode::Target;
	Settings->ColorInheritance = EPCGCopyPointsInheritanceMode::Target;
	Settings->SeedInheritance = EPCGCopyPointsInheritanceMode::Target;
	Settings->AttributeInheritance = EPCGCopyPointsMetadataInheritanceMode::Target;
	bTestPassed &= ValidateCopyPointsPoints();

	// Test 4 - empty source data
	SourceTaggedData.Data = EmptyData;
	bTestPassed &= ValidateCopyPointsPoints();
	SourceTaggedData.Data = SourceData;

	// Test 5 - empty target data
	TargetTaggedData.Data = EmptyData;
	bTestPassed &= ValidateCopyPointsPoints();
	TargetTaggedData.Data = TargetData;

	return bTestPassed;
}

#endif // WITH_EDITOR
