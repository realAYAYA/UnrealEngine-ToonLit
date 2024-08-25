// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MLDeformerMorphModelInstance.h"
#include "Misc/Optional.h"
#include "NearestNeighborModelInstance.generated.h"

class UNearestNeighborModel;
class UNearestNeighborModelInputInfo;
class UNearestNeighborModelInstance;
class UNearestNeighborOptimizedNetwork;
class UNearestNeighborOptimizedNetworkInstance;
namespace UE::NearestNeighborModel
{
    class FNearestNeighborEditorModel;
    class FNearestNeighborEditorModelActor;
};

UCLASS(Blueprintable)
class NEARESTNEIGHBORMODEL_API UNearestNeighborModelInstance
    : public UMLDeformerMorphModelInstance
{
    GENERATED_BODY()

public:
	// UMLDeformerModelInstance overrides
    virtual void Init(USkeletalMeshComponent* SkelMeshComponent) override;
    virtual void Execute(float ModelWeight) override;
    virtual bool SetupInputs() override;
    virtual FString CheckCompatibility(USkeletalMeshComponent* InSkelMeshComponent, bool LogIssues=false) override;
    virtual void Tick(float DeltaTime, float ModelWeight) override;
    virtual int64 SetBoneTransforms(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex) override;
    // ~END UMLDeformerModelInstance overrides

    UFUNCTION(BlueprintCallable, Category = "NearestNeighborModel")
    void Reset();

    friend class UNearestNeighborModelInputInfo;
    friend class UNearestNeighborModelInstance;
    friend class UNearestNeighborOptimizedNetworkInstance;
    friend class UE::NearestNeighborModel::FNearestNeighborEditorModel;
    friend class UE::NearestNeighborModel::FNearestNeighborEditorModelActor;

#if WITH_EDITOR
    TArray<uint32> GetNearestNeighborIds() const;
#endif

private:
    using UNetworkPtr = TWeakObjectPtr<UNearestNeighborOptimizedNetwork>;
    using UConstNetworkPtr = const TWeakObjectPtr<const UNearestNeighborOptimizedNetwork>;

    void InitInstanceData(int32 NumMorphWeights = INDEX_NONE);

    UNearestNeighborModel* GetCastModel() const;
    void InitOptimizedNetworkInstance();
    TOptional<TArrayView<float>> GetInputView() const;
    TOptional<TArrayView<float>> GetOutputView() const;

    void RunNearestNeighborModel(float DeltaTime, float ModelWeight);

    /** This is the slow version of network inference. It is used by python. */
    UFUNCTION(BlueprintCallable, Category = "Python")
    TArray<float> Eval(const TArray<float>& InputData);

#if WITH_EDITOR
    TArray<uint32> NearestNeighborIds;
#endif

    TArray<float> PreviousWeights;
    TArray<float> DistanceBuffer;
    bool bNeedsReset = true;

    UPROPERTY()
    TObjectPtr<UNearestNeighborOptimizedNetworkInstance> OptimizedNetworkInstance;
};
