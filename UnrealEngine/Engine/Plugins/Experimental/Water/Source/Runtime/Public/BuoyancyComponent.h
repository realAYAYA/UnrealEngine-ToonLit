// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "WaterBodyTypes.h"
#include "BuoyancyManager.h"
#include "BuoyancyComponent.generated.h"

class UWaterBodyComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPontoonEnteredWater, const FSphericalPontoon&, Pontoon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPontoonExitedWater, const FSphericalPontoon&, Pontoon);

UCLASS(Blueprintable, Config = Game, meta = (BlueprintSpawnableComponent))
class WATER_API UBuoyancyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuoyancyComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UActorComponent Interface.	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	//~ Begin UObject Interface.	
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	virtual void Update(float DeltaTime);

	virtual void ApplyForces(float DeltaTime, FVector LinearVelocity, float ForwardSpeed, float ForwardSpeedKmh, UPrimitiveComponent* PrimitiveComponent);

	virtual void FinalizeAuxData() {}

	virtual TUniquePtr<FBuoyancyComponentAsyncInput> SetCurrentAsyncInputOutput(int32 InputIdx, FBuoyancyManagerAsyncOutput* CurOutput, FBuoyancyManagerAsyncOutput* NextOutput, float Alpha, int32 BuoyancyManagerTimestamp);
	void SetCurrentAsyncInputOutputInternal(FBuoyancyComponentAsyncInput* CurInput, int32 InputIdx, FBuoyancyManagerAsyncOutput* CurOutput, FBuoyancyManagerAsyncOutput* NextOutput, float Alpha, int32 BuoyancyManagerTimestamp);
	void FinalizeSimCallbackData(FBuoyancyManagerAsyncInput& Input);
	void GameThread_ProcessIntermediateAsyncOutput(const FBuoyancyManagerAsyncOutput& AsyncOutput);
	virtual void GameThread_ProcessIntermediateAsyncOutput(const FBuoyancyComponentAsyncOutput& Output);

	bool IsUsingAsyncPath() const;

	virtual TUniquePtr<FBuoyancyComponentAsyncAux> CreateAsyncAux() const;

	virtual void SetupWaterBodyOverlaps();

	UPrimitiveComponent* GetSimulatingComponent() { return SimulatingComponent; }
	bool HasPontoons() const { return BuoyancyData.Pontoons.Num() > 0; }
	void AddCustomPontoon(float Radius, FName CenterSocketName);
	void AddCustomPontoon(float Radius, const FVector& RelativeLocation);
	virtual int32 UpdatePontoons(float DeltaTime, float ForwardSpeed, float ForwardSpeedKmh, UPrimitiveComponent* PrimitiveComponent);
	void UpdatePontoonCoefficients();
	FVector ComputeWaterForce(const float DeltaTime, const FVector LinearVelocity) const;
	FVector ComputeLinearDragForce(const FVector& PhyiscsVelocity) const;
	FVector ComputeAngularDragTorque(const FVector& AngularVelocity) const;

	void EnteredWaterBody(UWaterBodyComponent* WaterBodyComponent);
	void ExitedWaterBody(UWaterBodyComponent* WaterBodyComponent);

	UFUNCTION(BlueprintCallable, Category = Buoyancy)
	const TArray<UWaterBodyComponent*>& GetCurrentWaterBodyComponents() const { return CurrentWaterBodyComponents; }
	TArray<UWaterBodyComponent*>& GetCurrentWaterBodyComponents() { return CurrentWaterBodyComponents; }

	UFUNCTION(BlueprintCallable, Category = Buoyancy)
	bool IsOverlappingWaterBody() const { return bIsOverlappingWaterBody; }

	virtual bool IsActive() const { return bCanBeActive && IsOverlappingWaterBody(); }

	void SetCanBeActive(bool bInCanBeActive) { bCanBeActive = bInCanBeActive; }

	UFUNCTION(BlueprintCallable, Category = Buoyancy)
	bool IsInWaterBody() const { return bIsInWaterBody; }

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use BuoyancyData.Pontoons instead."))
	TArray<FSphericalPontoon> Pontoons_DEPRECATED;

	void GetWaterSplineKey(FVector Location, TMap<const UWaterBodyComponent*, float>& OutMap, TMap<const UWaterBodyComponent*, float>& OutSegmentMap) const;
	float GetWaterHeight(FVector Position, const TMap<const UWaterBodyComponent*, float>& SplineKeyMap, float DefaultHeight, UWaterBodyComponent*& OutWaterBodyComponent, float& OutWaterDepth, FVector& OutWaterPlaneLocation, FVector& OutWaterPlaneNormal, FVector& OutWaterSurfacePosition, FVector& OutWaterVelocity, int32& OutWaterBodyIdx, bool bShouldIncludeWaves = true);
	float GetWaterHeight(FVector Position, const TMap<const UWaterBodyComponent*, float>& SplineKeyMap, float DefaultHeight, bool bShouldIncludeWaves = true);

	UFUNCTION(BlueprintCallable, Category = Cosmetic)
	void OnPontoonEnteredWater(const FSphericalPontoon& Pontoon);

	UFUNCTION(BlueprintCallable, Category = Cosmetic)
	void OnPontoonExitedWater(const FSphericalPontoon& Pontoon);

	UPROPERTY(BlueprintAssignable, Category = Cosmetic)
	FOnPontoonEnteredWater OnEnteredWaterDelegate;

	UPROPERTY(BlueprintAssignable, Category = Cosmetic)
	FOnPontoonExitedWater OnExitedWaterDelegate;

	UFUNCTION(BlueprintCallable, Category = Buoyancy)
	void GetLastWaterSurfaceInfo(FVector& OutWaterPlaneLocation, FVector& OutWaterPlaneNormal,
	FVector& OutWaterSurfacePosition, float& OutWaterDepth, int32& OutWaterBodyIdx, FVector& OutWaterVelocity);

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Buoyancy)
	FBuoyancyData BuoyancyData;

protected:
	virtual void ApplyBuoyancy(UPrimitiveComponent* PrimitiveComponent);
	void ComputeBuoyancy(FSphericalPontoon& Pontoon, float ForwardSpeedKmh);
	void ComputePontoonCoefficients();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWaterBodyComponent>> CurrentWaterBodyComponents;

	// Primitive component that will be used for physics simulation.
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> SimulatingComponent;

	// async data

	FBuoyancyComponentAsyncInput* CurAsyncInput;
	FBuoyancyComponentAsyncOutput* CurAsyncOutput;
	FBuoyancyComponentAsyncOutput* NextAsyncOutput;
	EAsyncBuoyancyComponentDataType CurAsyncType;
	float OutputInterpAlpha = 0.f;

	struct FAsyncOutputWrapper
	{
		int32 Idx;
		int32 Timestamp;

		FAsyncOutputWrapper()
			: Idx(INDEX_NONE)
			, Timestamp(INDEX_NONE)
		{
		}
	};

	TArray<FAsyncOutputWrapper> OutputsWaitingOn;

	// async end

	uint32 PontoonConfiguration;
	TMap<uint32, TArray<float>> ConfiguredPontoonCoefficients;
	int32 VelocityPontoonIndex;
	int8 bIsOverlappingWaterBody : 1;
	int8 bCanBeActive : 1;
	int8 bIsInWaterBody : 1;
public:
	uint8 bUseAsyncPath : 1;

};