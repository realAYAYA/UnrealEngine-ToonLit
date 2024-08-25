// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMask2DBaseModifier.h"
#include "AvaPropertyChangeDispatcher.h"
#include "Modifiers/AvaArrangeBaseModifier.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

#include "AvaMask2DReadModifier.generated.h"

class AActor;
class IAvaActorHandle;
class IAvaComponentHandle;
class IAvaMaskMaterialCollectionHandle;
class IAvaMaskMaterialHandle;
class IGeometryMaskReadInterface;
class IGeometryMaskWriteInterface;
class UActorComponent;
class UAvaMaskMaterialInstanceSubsystem;
class UAvaObjectHandleSubsystem;
class UGeometryMaskCanvas;
class UCanvasRenderTarget2D;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UTexture;
struct FAvaShapeParametricMaterial;
struct FMaterialParameterInfo;

/** Uses scene actors to create a mask texture and applies it to attached actors */
UCLASS(BlueprintType)
class AVALANCHEMASK_API UAvaMask2DReadModifier
	: public UAvaMask2DBaseModifier
{
	GENERATED_BODY()

	friend class UAvaMask2DModifierShared;
	
public:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	const float GetBaseOpacity() const { return BaseOpacity; }
	void SetBaseOpacity(float InBaseOpacity);

protected:
	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	/** Get target blend mode dependent on modifier settings. */
	EBlendMode GetBlendMode() const;

	void OnBaseOpacityChanged();

	virtual void SetupMaskComponent(UActorComponent* InComponent) override;
	void SetupMaskReadComponent(IGeometryMaskReadInterface* InMaskReader);

	bool ApplyRead(AActor* InActor, FAvaMask2DActorData& InActorData);

protected:
	/** Base opacity/alpha to use in Read mode */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter, Setter, Category = "Mask2D", meta = (ClampMin = 0.0, ClampMax = 1.0, AllowPrivateAccess = "true"))
	float BaseOpacity = 0.0f;

#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<UAvaMask2DReadModifier> PropertyChangeDispatcher;
#endif
};
