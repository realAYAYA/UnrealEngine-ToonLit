// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotify_PlayMontageNotify.generated.h"


//////////////////////////////////////////////////////////////////////////
// UAnimNotify_PlayMontageNotify
//////////////////////////////////////////////////////////////////////////

UCLASS(editinlinenew, const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Montage Notify"))
class ANIMGRAPHRUNTIME_API UAnimNotify_PlayMontageNotify : public UAnimNotify
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	
	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif
protected:

	// Name of notify that is passed to the PlayMontage K2Node.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notify")
	FName NotifyName;
};


//////////////////////////////////////////////////////////////////////////
// UAnimNotify_PlayMontageNotifyWindow
//////////////////////////////////////////////////////////////////////////

UCLASS(editinlinenew, const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Montage Notify Window"))
class ANIMGRAPHRUNTIME_API UAnimNotify_PlayMontageNotifyWindow : public UAnimNotifyState
{
	GENERATED_UCLASS_BODY()

public:
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif
protected:

	// Name of notify that is passed to ability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notify")
	FName NotifyName;
};
