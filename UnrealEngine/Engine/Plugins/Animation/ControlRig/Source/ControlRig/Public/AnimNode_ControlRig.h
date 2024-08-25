// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRig.h"
#include "Animation/InputScaleBias.h"
#include "AnimNode_ControlRigBase.h"
#include "Animation/AnimBulkCurves.h"
#include "AnimNode_ControlRig.generated.h"

class UNodeMappingContainer;

/**
 * Animation node that allows animation ControlRig output to be used in an animation graph
 */
USTRUCT()
struct CONTROLRIG_API FAnimNode_ControlRig : public FAnimNode_ControlRigBase
{
	GENERATED_BODY()

public:

	FAnimNode_ControlRig();
	~FAnimNode_ControlRig();

	virtual UControlRig* GetControlRig() const override { return ControlRig; }
	virtual TSubclassOf<UControlRig> GetControlRigClass() const override { return ControlRigClass; }

	// FAnimNode_Base interface
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext & Output) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }
	void SetIOMapping(bool bInput, const FName& SourceProperty, const FName& TargetCurve);
	FName GetIOMapping(bool bInput, const FName& SourceProperty) const;

	virtual void InitializeProperties(const UObject* InSourceInstance, UClass* InTargetClass) override;
	virtual void PropagateInputProperties(const UObject* InSourceInstance) override;
	
private:
	void HandleOnInitialized_AnyThread(URigVMHost*, const FName&);
#if WITH_EDITOR
	virtual void HandleObjectsReinstanced_Impl(UObject* InSourceObject, UObject* InTargetObject, const TMap<UObject*, UObject*>& OldToNewInstanceMap) override;
#endif
private:

	// The class to use for the rig. 
	UPROPERTY(EditAnywhere, Category = ControlRig)
	TSubclassOf<UControlRig> ControlRigClass;

	// The default class to use for the rig. This is needed
	// only if the Control Rig Class is exposed as a pin.
	UPROPERTY()
	TSubclassOf<UControlRig> DefaultControlRigClass;

	/** Cached ControlRig */
	UPROPERTY(transient)
	TObjectPtr<UControlRig> ControlRig;

	/** Cached ControlRigs per class */
	UPROPERTY(transient)
	TMap<UClass*, TObjectPtr<UControlRig>> ControlRigPerClass;

	// alpha value handler
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault))
	float Alpha;

	UPROPERTY(EditAnywhere, Category = Settings)
	EAnimAlphaInputType AlphaInputType;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault, DisplayName = "bEnabled", DisplayAfter = "AlphaScaleBias"))
	uint8 bAlphaBoolEnabled : 1;

	// Override the initial transforms with those taken from the mesh component
	UPROPERTY(EditAnywhere, Category=Settings, meta = (DisplayName = "Set Initial Transforms From Mesh"))
	uint8 bSetRefPoseFromSkeleton : 1;

	UPROPERTY(EditAnywhere, Category=Settings)
	FInputScaleBias AlphaScaleBias;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (DisplayName = "Blend Settings"))
	FInputAlphaBoolBlend AlphaBoolBlend;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault))
	FName AlphaCurveName;

	UPROPERTY(EditAnywhere, Category = Settings)
	FInputScaleBiasClamp AlphaScaleBiasClamp;

	// we only save mapping, 
	// we have to query control rig when runtime 
	// to ensure type and everything is still valid or not
	UPROPERTY()
	TMap<FName, FName> InputMapping;

	UPROPERTY()
	TMap<FName, FName> OutputMapping;

	TMap<FName, FName> InputTypes;
	TMap<FName, FName> OutputTypes;
	TArray<uint8*> DestParameters;

	/*
	 * Max LOD that this node is allowed to run
	 * For example if you have LODThreshold to be 2, it will run until LOD 2 (based on 0 index)
	 * when the component LOD becomes 3, it will stop update/evaluate
	 * currently transition would be issue and that has to be re-visited
	 */
	UPROPERTY(EditAnywhere, Category = Performance, meta = (PinHiddenByDefault, DisplayName = "LOD Threshold"))
	int32 LODThreshold;

private:
	struct FControlRigCurveMapping
	{
		FControlRigCurveMapping() = default;
		
		FControlRigCurveMapping(FName InSourceName, FName InTargetName)
			: Name(InTargetName)
			, SourceName(InSourceName)
		{}

		FName Name = NAME_None;
		FName SourceName = NAME_None;
	};

	using FCurveMappings = UE::Anim::TNamedValueArray<FDefaultAllocator, FControlRigCurveMapping>;

	// Bulk curves for I/O
	FCurveMappings InputCurveMappings;
	FCurveMappings OutputCurveMappings;

protected:
	virtual UClass* GetTargetClass() const override;
	virtual void UpdateInput(UControlRig* InControlRig, const FPoseContext& InOutput) override;
	virtual void UpdateOutput(UControlRig* InControlRig, FPoseContext& InOutput) override;

	void SetControlRigClass(TSubclassOf<UControlRig> InControlRigClass);
	bool UpdateControlRigIfNeeded(const UAnimInstance* InAnimInstance, const FBoneContainer& InRequiredBones);

	// Helper function to update the initial ref pose within the Control Rig if needed
	void UpdateControlRigRefPoseIfNeeded(const FAnimInstanceProxy* InProxy, bool bIncludePoseInHash = false);
	
	// A hash to encode the pointer to anim instance, an
	TOptional<int32> RefPoseSetterHash;

public:

	void PostSerialize(const FArchive& Ar);

	friend class UAnimGraphNode_ControlRig;
	friend class UAnimNodeControlRigLibrary;
};

template<>
struct TStructOpsTypeTraits<FAnimNode_ControlRig> : public TStructOpsTypeTraitsBase2<FAnimNode_ControlRig>
{
	enum
	{
		WithPostSerialize = true,
	};
};