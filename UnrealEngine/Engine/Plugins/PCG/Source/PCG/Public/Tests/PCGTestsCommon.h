// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "PCGSettings.h"

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

class IPCGElement;
class UPCGComponent;
class UPCGNode;
class UPCGParamData;
class UPCGPointData;
class UPCGPolyLineData;
class UPCGPrimitiveData;
class UPCGSurfaceData;
class UPCGVolumeData;
struct FPCGContext;
struct FPCGPinProperties;
struct FPCGPoint;

namespace PCGTestsCommon
{
	constexpr int TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** If you are using a FTestData, you can use FTestData::InitializeTestContext. Initialize a context and set the number of tasks available to 1. */
	TUniquePtr<FPCGContext> InitializeTestContext(IPCGElement* InElement, const FPCGDataCollection& InputData, UPCGComponent* InSourceComponent, const UPCGNode* InNode);

	struct PCG_API FTestData
	{
		explicit FTestData(int32 Seed = 42, UPCGSettings* DefaultSettings = nullptr, TSubclassOf<AActor> ActorClass = AActor::StaticClass());
		~FTestData();

		void Reset(UPCGSettings* InSettings = nullptr);

		/** Initialize a context and set the number of tasks available to 1. */
		TUniquePtr<FPCGContext> InitializeTestContext(const UPCGNode* InNode = nullptr) const;

		AActor* TestActor;
		UPCGComponent* TestPCGComponent;
		FPCGDataCollection InputData;
		FPCGDataCollection OutputData;
		UPCGSettings* Settings;
		int32 Seed;
		FRandomStream RandomStream;
	};

	AActor* CreateTemporaryActor();
	PCG_API UPCGPointData* CreateEmptyPointData();
	UPCGParamData* CreateEmptyParamData();

	/** Creates a PointData with a single point at the origin */
	PCG_API UPCGPointData* CreatePointData();
	/** Creates a PointData with a single point at the provided location */
	PCG_API UPCGPointData* CreatePointData(const FVector& InLocation);

	/** Creates a PointData with PointCount many points, and randomizes the Transform, Color, and Density */
	PCG_API UPCGPointData* CreateRandomPointData(int32 PointCount, int32 Seed, bool RandomDensity = false);

	UPCGPolyLineData* CreatePolyLineData();
	UPCGSurfaceData* CreateSurfaceData();
	UPCGVolumeData* CreateVolumeData(const FBox& InBounds = FBox::BuildAABB(FVector::ZeroVector, FVector::OneVector * 100));
	UPCGPrimitiveData* CreatePrimitiveData();

	TArray<FPCGDataCollection> GenerateAllowedData(const FPCGPinProperties& PinProperties);

	/** Validates that two Spatial Points are identical */
	bool PointsAreIdentical(const FPCGPoint& FirstPoint, const FPCGPoint& SecondPoint);

	/** Generates settings based upon a UPCGSettings subclass */
	template<typename SettingsType>
	SettingsType* GenerateSettings(FTestData& TestData, TFunction<void(FTestData&)> ExtraSettingsDelegate = nullptr)
	{
		SettingsType* TypedSettings = NewObject<SettingsType>();
		check(TypedSettings);

		TestData.Settings = TypedSettings;
		TestData.Settings->Seed = TestData.Seed;

		TestData.InputData.TaggedData.Emplace_GetRef().Data = TestData.Settings;
		TestData.InputData.TaggedData.Last().Pin = FName(TEXT("Settings"));

		if (ExtraSettingsDelegate)
		{
			ExtraSettingsDelegate(TestData);
		}

		return TypedSettings;
	}
}

class FPCGTestBaseClass : public FAutomationTestBase
{
public:
	using FAutomationTestBase::FAutomationTestBase;
	/** Hook to expose private test function publicly */
	bool RunPCGTest(const FString& Parameters) { return RunTest(Parameters); }

protected:
	/** Generates all valid input combinations */
	bool SmokeTestAnyValidInput(UPCGSettings* InSettings, TFunction<bool(const FPCGDataCollection&, const FPCGDataCollection&)> ValidationFn = TFunction<bool(const FPCGDataCollection&, const FPCGDataCollection&)>());
};
