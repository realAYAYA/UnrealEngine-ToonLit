// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPolyLineData.h"
#include "Data/PCGPrimitiveData.h"
#include "Data/PCGSurfaceData.h"
#include "Data/PCGVolumeData.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#endif

namespace PCGTestsCommon
{
	FTestData::FTestData(int32 RandomSeed, UPCGSettings* DefaultSettings, TSubclassOf<AActor> ActorClass)
		: Settings(DefaultSettings)
		, Seed(RandomSeed)
		, RandomStream(Seed)
	{
#if WITH_EDITOR
		check(GEditor);
		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		check(EditorWorld);

		// No getting the level dirty
		FActorSpawnParameters TransientActorParameters;
		TransientActorParameters.bHideFromSceneOutliner = true;
		TransientActorParameters.bTemporaryEditorActor = true;
		TransientActorParameters.ObjectFlags = RF_Transient;
		TestActor = EditorWorld->SpawnActor<AActor>(ActorClass, TransientActorParameters);
		check(TestActor);

		TestPCGComponent = NewObject<UPCGComponent>(TestActor, FName(TEXT("Test PCG Component")), RF_Transient);
		check(TestPCGComponent);
		// By default PCG components for tests will be non-partitioned
		TestPCGComponent->bIsPartitioned = false;
		TestActor->AddInstanceComponent(TestPCGComponent);
		TestPCGComponent->RegisterComponent();

		UPCGGraph* TestGraph = NewObject<UPCGGraph>(TestPCGComponent, FName(TEXT("Test PCG Graph")), RF_Transient);
		check(TestGraph);
		TestPCGComponent->SetGraphLocal(TestGraph);
#else
		TestActor = nullptr;
		TestPCGComponent = nullptr;
		Settings = nullptr;
#endif
	}

	FTestData::~FTestData()
	{
#if WITH_EDITOR
		if (GEditor)
		{
			if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
			{
				if (TestActor)
				{
					EditorWorld->DestroyActor(TestActor);
				}
			}
		}
#endif // WITH_EDITOR
	}

	void FTestData::Reset(UPCGSettings* InSettings)
	{
		// Clear all the data
		RandomStream.Reset();
		InputData.TaggedData.Empty();
		OutputData.TaggedData.Empty();
		Settings = InSettings;
	}

	AActor* CreateTemporaryActor()
	{
		return NewObject<AActor>();
	}

	UPCGPointData* CreateEmptyPointData()
	{
		return NewObject<UPCGPointData>();
	}

	UPCGPointData* CreatePointData()
	{
		UPCGPointData* SinglePointData = CreateEmptyPointData();

		check(SinglePointData);

		SinglePointData->GetMutablePoints().Emplace();

		return SinglePointData;
	}

	UPCGPointData* CreatePointData(const FVector& InLocation)
	{
		UPCGPointData* SinglePointData = CreatePointData();

		check(SinglePointData);
		check(SinglePointData->GetMutablePoints().Num() == 1);

		SinglePointData->GetMutablePoints()[0].Transform.SetLocation(InLocation);

		return SinglePointData;
	}

	/** Creates a point data with PointCount many points, and randomizes the Transform and Color */
	UPCGPointData* CreateRandomPointData(int32 PointCount, int32 Seed)
	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreateEmptyPointData();
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

		Points.Reserve(PointCount);
	
		FRandomStream RandomSource(Seed);
		for (int I = 0; I < PointCount; ++I)
		{
			FQuat Rotation(FRotator(RandomSource.FRandRange(0.f, 360.f)).Quaternion()); 
			FVector Scale(RandomSource.VRand());
			FVector Location(RandomSource.VRand());

			FPCGPoint& Point = Points.Emplace_GetRef(FTransform(Rotation, Location, Scale), 1.f, I);
			Point.Color = RandomSource.VRand();
			Point.Density = 1.f;
			Point.Seed = I;
		}

		return PointData;
	}

	UPCGPolyLineData* CreatePolyLineData()
	{
		// TODO: spline, landscape spline
		return nullptr;
	}

	UPCGSurfaceData* CreateSurfaceData()
	{
		// TODO: either landscape, texture, render target
		return nullptr;
	}

	UPCGVolumeData* CreateVolumeData(const FBox& InBounds)
	{
		UPCGVolumeData* VolumeData = NewObject<UPCGVolumeData>();
		VolumeData->Initialize(InBounds, nullptr);
		return VolumeData;
	}

	UPCGPrimitiveData* CreatePrimitiveData()
	{
		// TODO: need UPrimitiveComponent on an actor
		return nullptr;
	}

	UPCGParamData* CreateEmptyParamData()
	{
		return NewObject<UPCGParamData>();
	}

	TArray<FPCGDataCollection> GenerateAllowedData(const FPCGPinProperties& PinProperties)
	{
		TArray<FPCGDataCollection> Data;

		TMap<EPCGDataType, TFunction<UPCGData*(void)>> TypeToDataFn;
		TypeToDataFn.Add(EPCGDataType::Point, []() { return PCGTestsCommon::CreatePointData(); });
		TypeToDataFn.Add(EPCGDataType::PolyLine, []() { return PCGTestsCommon::CreatePolyLineData(); });
		TypeToDataFn.Add(EPCGDataType::Surface, []() { return PCGTestsCommon::CreateSurfaceData(); });
		TypeToDataFn.Add(EPCGDataType::Volume, []() { return PCGTestsCommon::CreateVolumeData(); });
		TypeToDataFn.Add(EPCGDataType::Primitive, []() { return PCGTestsCommon::CreatePrimitiveData(); });
		TypeToDataFn.Add(EPCGDataType::Param, []() { return PCGTestsCommon::CreateEmptyParamData(); });

		// Create empty data
		Data.Emplace();

		// Create single data & data pairs
		for (const auto& TypeToData : TypeToDataFn)
		{
			if (!(TypeToData.Key & PinProperties.AllowedTypes))
			{
				continue;
			}

			FPCGDataCollection& SingleCollection = Data.Emplace_GetRef();
			FPCGTaggedData& SingleTaggedData = SingleCollection.TaggedData.Emplace_GetRef();
			SingleTaggedData.Data = TypeToData.Value();
			SingleTaggedData.Pin = PinProperties.Label;

			if (!PinProperties.bAllowMultipleConnections)
			{
				continue;
			}

			for (const auto& SecondaryTypeToData : TypeToDataFn)
			{
				if (!(SecondaryTypeToData.Key & PinProperties.AllowedTypes))
				{
					continue;
				}

				FPCGDataCollection& MultiCollection = Data.Emplace_GetRef();
				FPCGTaggedData& FirstTaggedData = MultiCollection.TaggedData.Emplace_GetRef();
				FirstTaggedData.Data = TypeToData.Value();
				FirstTaggedData.Pin = PinProperties.Label;

				FPCGTaggedData& SecondTaggedData = MultiCollection.TaggedData.Emplace_GetRef();
				SecondTaggedData.Data = SecondaryTypeToData.Value();
				SecondTaggedData.Pin = PinProperties.Label;
			}
		}

		return Data;
	}

	bool PointsAreIdentical(const FPCGPoint& FirstPoint, const FPCGPoint& SecondPoint)
	{
		// Trivial checks first for pruning
		if (FirstPoint.Density != SecondPoint.Density || FirstPoint.Steepness != SecondPoint.Steepness ||
			FirstPoint.BoundsMin != SecondPoint.BoundsMin || FirstPoint.BoundsMax != SecondPoint.BoundsMax ||
			FirstPoint.Color != SecondPoint.Color)
		{
			return false;
		}

		// Transform checks with epsilon
		return FirstPoint.Transform.Equals(SecondPoint.Transform);
	}
}

bool FPCGTestBaseClass::SmokeTestAnyValidInput(UPCGSettings* InSettings, TFunction<bool(const FPCGDataCollection&, const FPCGDataCollection&)> ValidationFn)
{
	TestTrue("Valid settings", InSettings != nullptr);

	if (!InSettings)
	{
		return false;
	}

	FPCGElementPtr Element = InSettings->GetElement();

	TestTrue("Valid element", Element != nullptr);

	if (!Element)
	{
		return false;
	}

	TArray<FPCGPinProperties> InputProperties = InSettings->InputPinProperties();
	// For each pin: take nothing, take 1 of any supported type, take 2 of any supported types (if enabled)
	TArray<TArray<FPCGDataCollection>> InputsPerProperties;
	TArray<uint32> InputIndices;

	if (!InputProperties.IsEmpty())
	{
		for (const FPCGPinProperties& InputProperty : InputProperties)
		{
			InputsPerProperties.Add(PCGTestsCommon::GenerateAllowedData(InputProperty));
			InputIndices.Add(0);
		}
	}
	else
	{
		TArray<FPCGDataCollection>& EmptyCollection = InputsPerProperties.Emplace_GetRef();
		EmptyCollection.Emplace();
		InputIndices.Add(0);
	}

	check(InputIndices.Num() == InputsPerProperties.Num());

	bool bDone = false;
	while (!bDone)
	{
		// Prepare input
		FPCGDataCollection InputData;
		for (int32 PinIndex = 0; PinIndex < InputIndices.Num(); ++PinIndex)
		{
			InputData.TaggedData.Append(InputsPerProperties[PinIndex][InputIndices[PinIndex]].TaggedData);
		}

		TUniquePtr<FPCGContext> Context(Element->Initialize(InputData, nullptr, nullptr));
		Context->NumAvailableTasks = 1;
		
		// Execute element until done
		while (!Element->Execute(Context.Get()))
		{
		}

		if (ValidationFn)
		{
			TestTrue("Validation", ValidationFn(Context->InputData, Context->OutputData));
		}

		// Bump indices
		int BumpIndex = 0;
		while (BumpIndex < InputIndices.Num())
		{
			if (InputIndices[BumpIndex] == InputsPerProperties[BumpIndex].Num() - 1)
			{
				InputIndices[BumpIndex] = 0;
				++BumpIndex;
			}
			else
			{
				++InputIndices[BumpIndex];
				break;
			}
		}

		if (BumpIndex == InputIndices.Num())
		{
			bDone = true;
		}
	}

	return true;
}
