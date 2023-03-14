// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimationEditContext.h"
#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "EdMode.h"
#include "Math/Sphere.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "AnimationEditMode.generated.h"

class FAnimationEditMode;
class FText;

/**
 *	A compatibility context object to support IPersonaEditMode-based code. It simply calls into a different
 *	IAnimationEditContext in its implementations.
 */
UCLASS(MinimalAPI)
class UAnimationEditModeContext : public UObject, public IAnimationEditContext
{
	GENERATED_BODY()
public:
	virtual bool GetCameraTarget(FSphere& OutTarget) const override;
	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const override;
private:
	IAnimationEditContext* EditMode = nullptr;
	static UAnimationEditModeContext* CreateFor(IAnimationEditContext* InEditMode)
	{
		UAnimationEditModeContext* NewPersonaContext = NewObject<UAnimationEditModeContext>();
		NewPersonaContext->EditMode = InEditMode;
		return NewPersonaContext;
	}
	friend FAnimationEditMode;
};

class ANIMATIONEDITMODE_API FAnimationEditMode : public FEdMode, public IAnimationEditContext
{
public:
	FAnimationEditMode();

	FAnimationEditMode(const FAnimationEditMode&) = delete;
	FAnimationEditMode& operator=(const FAnimationEditMode&) = delete;

	virtual void Enter() override;
	virtual void Exit() override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
private:
	TObjectPtr<UAnimationEditModeContext> AnimationEditModeContext;
};
