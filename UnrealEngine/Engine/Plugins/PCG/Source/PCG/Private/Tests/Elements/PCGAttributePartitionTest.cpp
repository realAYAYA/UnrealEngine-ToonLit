// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"

#include "Elements/Metadata/PCGMetadataPartition.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_Points, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.Points", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_AttributeSet, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.AttributeSet", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_Order, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.Order", PCGTestsCommon::TestFlags)

bool FPCGAttributePartition_Points::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
	check(Settings);

	// By default there should be one selector with @Last attribute
	UTEST_EQUAL("There is one Partition Attribute Selector by default", Settings->PartitionAttributeSelectors.Num(), 1);

	Settings->PartitionAttributeSelectors[0].SetPointProperty(EPCGPointProperties::Density);

	UPCGPointData* InputPointData = NewObject<UPCGPointData>();
	TArray<FPCGPoint>& Points = InputPointData->GetMutablePoints();
	Points.Reserve(100);
	for (int32 i = 0; i < 100; ++i)
	{
		FPCGPoint& Point = Points.Emplace_GetRef();
		Point.Density = (i % 10) / 10.0f;
	}

	FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	InputTaggedData.Data = InputPointData;
	InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	UTEST_EQUAL("There are 10 outputs", Context->OutputData.TaggedData.Num(), 10);

	for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
	{
		const UPCGPointData* OutputPointData = Cast<const UPCGPointData>(Context->OutputData.TaggedData[i].Data);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d is a point data"), i), OutputPointData);

		const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

		UTEST_EQUAL(*FString::Printf(TEXT("Output %d has 10 points"), i), OutputPoints.Num(), 10);

		float TempValue = -1.0f;
		bool bFirstPoint = true;
		bool bAllEquals = true;
		for (const FPCGPoint& Point : OutputPoints)
		{
			if (bFirstPoint)
			{
				TempValue = Point.Density;
				bFirstPoint = false;
			}
			else
			{
				bAllEquals &= TempValue == Point.Density;
			}
		}

		UTEST_TRUE(*FString::Printf(TEXT("Output points for output %d have all the same density"), i), bAllEquals);
	}

	return true;
}

/**
* Test that the partitions are in the right order.
* Points in each partition should appear in the same order that they were in the original set.
* Partitions should be in the same order than the partition value appear in the original set.
* For example, if the original set was [(0, 4), (1, 4), (2, 2), (3, 2)], and we partition on the second value, the result should be
* [(0, 4), (1, 4)] and [(2, 2), (3, 2)]
*/
bool FPCGAttributePartition_Order::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
	check(Settings);

	// By default there should be one selector with @Last attribute
	UTEST_EQUAL("There is one Partition Attribute Selector by default", Settings->PartitionAttributeSelectors.Num(), 1);

	const FName AttributeName = TEXT("Attr");
	Settings->PartitionAttributeSelectors[0].SetAttributeName(AttributeName);

	UPCGPointData* InputPointData = NewObject<UPCGPointData>();
	FPCGMetadataAttribute<int>* Attribute = InputPointData->Metadata->CreateAttribute<int>(AttributeName, 0, false, false);

	constexpr int NumPoints = 10;

	TArray<FPCGPoint>& Points = InputPointData->GetMutablePoints();
	Points.Reserve(NumPoints);
	for (int32 i = 0; i < NumPoints; ++i)
	{
		FPCGPoint& Point = Points.Emplace_GetRef();
		// Store the index in the density
		Point.Density = i;
		InputPointData->Metadata->InitializeOnSet(Point.MetadataEntry);
		Attribute->SetValue(Point.MetadataEntry, (2 * i) / NumPoints);
	}

	// Reverse the points order
	for (int32 i = 0; i < NumPoints / 2; ++i)
	{
		std::swap(Points[i], Points[NumPoints - i - 1]);
	}

	FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	InputTaggedData.Data = InputPointData;
	InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	UTEST_EQUAL("There are 2 outputs", Context->OutputData.TaggedData.Num(), 2);

	for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
	{
		const UPCGPointData* OutputPointData = Cast<const UPCGPointData>(Context->OutputData.TaggedData[i].Data);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d is a point data"), i), OutputPointData);
		check(OutputPointData);

		const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();
		const FPCGMetadataAttribute<int>* OutputAttribute = OutputPointData->Metadata->GetConstTypedAttribute<int>(AttributeName);

		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d has the expected attribute"), i), OutputAttribute);
		check(OutputAttribute);

		UTEST_EQUAL(*FString::Printf(TEXT("Output %d has %d points"), i, NumPoints / 2), OutputPoints.Num(), NumPoints / 2);

		for (int32 j = 0; j < OutputPoints.Num() - 1; ++j)
		{
			UTEST_TRUE(*FString::Printf(TEXT("Output %d: Point_%d is at the right place"), i, j), OutputPoints[j].Density > OutputPoints[j + 1].Density);
			// First partition should have the value 1, and second partition should have the value 0
			UTEST_EQUAL(*FString::Printf(TEXT("Output %d: Point_%d has the right attribute value"), i, j), OutputAttribute->GetValue(OutputPoints[j].MetadataEntry), (i + 1) % 2);
		}
	}

	return true;
}

bool FPCGAttributePartition_AttributeSet::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
	check(Settings);

	// By default there should be one selector with @Last attribute
	UTEST_EQUAL("There is one Partition Attribute Selector by default", Settings->PartitionAttributeSelectors.Num(), 1);

	const FName InputAttributeName = TEXT("Double");
	Settings->PartitionAttributeSelectors[0].SetAttributeName(InputAttributeName);

	UPCGParamData* InputParam = NewObject<UPCGParamData>();
	FPCGMetadataAttribute<double>* Attribute = InputParam->Metadata->CreateAttribute<double>(InputAttributeName, 0.0, true, false);
	for (int32 i = 0; i < 100; ++i)
	{
		Attribute->SetValue(InputParam->Metadata->AddEntry(), (i % 10) / 10.0);
	}

	FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	InputTaggedData.Data = InputParam;
	InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	UTEST_EQUAL("There are 10 outputs", Context->OutputData.TaggedData.Num(), 10);

	for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
	{
		const UPCGParamData* OutputParamData = Cast<const UPCGParamData>(Context->OutputData.TaggedData[i].Data);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d is a param data"), i), OutputParamData);

		UTEST_EQUAL(*FString::Printf(TEXT("Output %d has 10 entries"), i), OutputParamData->Metadata->GetLocalItemCount(), 10ll);
		const FPCGMetadataAttribute<double>* OutAttribute = OutputParamData->Metadata->GetConstTypedAttribute<double>(InputAttributeName);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d has the 'Double' attribute"), i), OutAttribute);


		double TempValue = -1.0;
		bool bFirstItem = true;
		bool bAllEquals = true;
		for (PCGMetadataEntryKey Key = 0; Key < OutputParamData->Metadata->GetLocalItemCount(); ++Key)
		{
			if (bFirstItem)
			{
				TempValue = OutAttribute->GetValueFromItemKey(Key);
				bFirstItem = false;
			}
			else
			{
				bAllEquals &= TempValue == OutAttribute->GetValueFromItemKey(Key);
			}
		}

		UTEST_TRUE(*FString::Printf(TEXT("Output values for output %d are all the same"), i), bAllEquals);
	}

	return true;
}