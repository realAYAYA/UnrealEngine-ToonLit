// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimNode_LinkedAnimGraph.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimLayerInterface.h"

#include "AnimNode_LinkedAnimLayer.generated.h"

struct FAnimInstanceProxy;
class UUserDefinedStruct;
struct FAnimBlueprintFunction;
class IAnimClassInterface;

USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FAnimNode_LinkedAnimLayer : public FAnimNode_LinkedAnimGraph
{
	GENERATED_BODY()

public:
	/** 
	 * Optional interface. If this is set then this node will only accept (both statically and dynamically) anim instances that implement this interface.
	 * If not set, then this is considered a 'self' layer. This value is set when Layer is changed in the details panel.
	 */
	UPROPERTY()
	TSubclassOf<UAnimLayerInterface> Interface;

	/** The layer in the interface to use */
	UPROPERTY(EditAnywhere, Category = Settings)
	FName Layer;

	/** Set the layer's 'overlay' externally managed linked instance. */
	void SetLinkedLayerInstance(const UAnimInstance* InOwningAnimInstance, UAnimInstance* InNewLinkedInstance);

	/** FAnimNode_Base interface */
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	virtual bool NeedsOnInitializeAnimInstance() const override { return true; }

	/** FAnimNode_CustomProperty interface */
	virtual void InitializeProperties(const UObject* InSourceInstance, UClass* InTargetClass) override;
	
	/** FAnimNode_LinkedAnimGraph interface */
	virtual FName GetDynamicLinkFunctionName() const override;
	virtual UAnimInstance* GetDynamicLinkTarget(UAnimInstance* InOwningAnimInstance) const override;
	virtual UClass* GetTargetClass() const override 
	{
		return *Interface;
	}

#if WITH_EDITOR
	// Event fired when the instance we are running has changed
	FSimpleMulticastDelegate& OnInstanceChanged() { return OnInstanceChangedEvent; }
	virtual void HandleObjectsReinstanced_Impl(UObject* InSourceObject, UObject* InTargetObject, const TMap<UObject*, UObject*>& OldToNewInstanceMap) override;
#endif

protected:
	void InitializeSelfLayer(const UAnimInstance* SelfAnimInstance);

	// Initialize the source properties to copy from
	void InitializeSourceProperties(const UAnimInstance* InAnimInstance);

#if WITH_EDITOR
	// Event fired when the instance we are running has changed
	FSimpleMulticastDelegate OnInstanceChangedEvent;
#endif
};

UE_DEPRECATED(4.24, "FAnimNode_Layer has been renamed to FAnimNode_LinkedAnimLayer")
typedef FAnimNode_LinkedAnimLayer FAnimNode_Layer;