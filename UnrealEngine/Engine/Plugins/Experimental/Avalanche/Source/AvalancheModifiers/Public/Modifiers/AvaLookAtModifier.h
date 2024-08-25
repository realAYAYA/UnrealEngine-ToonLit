// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaAttachmentBaseModifier.h"
#include "Extensions/AvaTransformUpdateModifierExtension.h"
#include "AvaLookAtModifier.generated.h"

class AActor;
enum class EAvaAxis : uint8;

/**
 * Rotates the modifying actor to point it's specified axis at another actor.
 */
UCLASS(MinimalAPI, BlueprintType)
class UAvaLookAtModifier : public UAvaAttachmentBaseModifier
	, public IAvaTransformUpdateHandler
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|LookAt")
	AVALANCHEMODIFIERS_API void SetReferenceActor(const FAvaSceneTreeActor& InReferenceActor);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|LookAt")
	const FAvaSceneTreeActor& GetReferenceActor() const
	{
		return ReferenceActor;
	}

	/** Sets the axis that will point towards the reference actor. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|LookAt")
	AVALANCHEMODIFIERS_API void SetAxis(const EAvaAxis NewAxis);

	/** Returns the axis that will point towards t he reference actor. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|LookAt")
	EAvaAxis GetAxis() const
	{
		return Axis;
	}

	/** Sets the look-at direction to be flipped. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|LookAt")
	AVALANCHEMODIFIERS_API void SetFlipAxis(const bool bNewFlipAxis);

	/** Returns true if flipping the look-at rotation axis. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|LookAt")
	bool GetFlipAxis() const
	{
		return bFlipAxis;
	}

protected:
	//~ Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void Apply() override;
	virtual void OnModifiedActorTransformed() override;
	//~ End UActorModifierCoreBase

	//~ Begin IAvaTransformUpdateExtension
	virtual void OnTransformUpdated(AActor* InActor, bool bInParentMoved) override;
	//~ End IAvaTransformUpdateExtension

	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorChanged(int32 InIdx, AActor* InPreviousActor, AActor* InNewActor) override;
	//~ End IAvaSceneTreeUpdateModifierExtension

	void OnReferenceActorChanged();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetReferenceActor", Getter="GetReferenceActor", Category="LookAt", meta=(ShowOnlyInnerProperties, AllowPrivateAccess="true"))
	FAvaSceneTreeActor ReferenceActor;

	/** The actor to look at. */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use ReferenceActor instead"))
	TWeakObjectPtr<AActor> ReferenceActorWeak;

	/** The axis that will point towards the reference actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetAxis", Getter="GetAxis", Category="LookAt", meta=(AllowPrivateAccess="true"))
	EAvaAxis Axis;

	/** If true, will flip the look-at direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetFlipAxis", Getter="GetFlipAxis", Category="LookAt", meta=(AllowPrivateAccess="true"))
	bool bFlipAxis = false;

	UPROPERTY()
	bool bDeprecatedPropertiesMigrated = false;
};
