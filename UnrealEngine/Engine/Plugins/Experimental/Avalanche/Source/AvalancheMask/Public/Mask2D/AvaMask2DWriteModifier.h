// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMask2DBaseModifier.h"
#include "AvaPropertyChangeDispatcher.h"
#include "GeometryMaskTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

#include "AvaMask2DWriteModifier.generated.h"

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
class UCanvasRenderTarget2D;
class UGeometryMaskCanvas;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UTexture;
struct FAvaShapeParametricMaterial;
struct FMaterialParameterInfo;

/** Uses scene actors to create a mask texture and applies it to attached actors */
UCLASS(BlueprintType)
class AVALANCHEMASK_API UAvaMask2DWriteModifier
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

	EGeometryMaskCompositeOperation GetWriteOperation() const { return WriteOperation; }
	void SetWriteOperation(const EGeometryMaskCompositeOperation InWriteOperation);

protected:
	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	void OnWriteOperationChanged();

	void SetupMaskComponent(UActorComponent* InComponent);
	void SetupMaskWriteComponent(IGeometryMaskWriteInterface* InMaskWriter);

	bool ApplyWrite(AActor* InActor, FAvaMask2DActorData& InActorData);

protected:
	/** How to write to the chosen mask channel */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter, Setter, Category = "Mask2D", meta = (ValidEnumValues = "Add, Subtract", AllowPrivateAccess = "true"))
	EGeometryMaskCompositeOperation WriteOperation = EGeometryMaskCompositeOperation::Add;

#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<UAvaMask2DWriteModifier> PropertyChangeDispatcher;
#endif
};
