// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

class UPCGComponent;
class UPCGParamData;
class UPCGPointData;
class UPCGPolyLineData;
class UPCGPrimitiveData;
class UPCGSettings;
class UPCGSurfaceData;
class UPCGVolumeData;
struct FPCGPinProperties;
struct FPCGPoint;

namespace PCGTestsCommon
{
	constexpr int TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FTestData
	{
		FTestData(int32 Seed, UPCGSettings* DefaultSettings = nullptr, TSubclassOf<AActor> ActorClass = AActor::StaticClass());
		~FTestData();

		void Reset(UPCGSettings* InSettings = nullptr);

		AActor* TestActor;
		UPCGComponent* TestPCGComponent;
		FPCGDataCollection InputData;
		FPCGDataCollection OutputData;
		UPCGSettings* Settings;
		int32 Seed;
		FRandomStream RandomStream;
	};

	AActor* CreateTemporaryActor();
	UPCGPointData* CreateEmptyPointData();
	UPCGParamData* CreateEmptyParamData();

	/** Creates a PointData with a single point at the origin */
	UPCGPointData* CreatePointData();
	/** Creates a PointData with a single point at the provided location */
	UPCGPointData* CreatePointData(const FVector& InLocation);

	/** Creates a PointData with PointCount many points, and randomizes the Transform and Color */
	UPCGPointData* CreateRandomPointData(int32 PointCount, int32 Seed);

	UPCGPolyLineData* CreatePolyLineData();
	UPCGSurfaceData* CreateSurfaceData();
	UPCGVolumeData* CreateVolumeData(const FBox& InBounds = FBox::BuildAABB(FVector::ZeroVector, FVector::OneVector * 100));
	UPCGPrimitiveData* CreatePrimitiveData();

	TArray<FPCGDataCollection> GenerateAllowedData(const FPCGPinProperties& PinProperties);

	/** Validates that two Spatial Points are identical */
	bool PointsAreIdentical(const FPCGPoint& FirstPoint, const FPCGPoint& SecondPoint);
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