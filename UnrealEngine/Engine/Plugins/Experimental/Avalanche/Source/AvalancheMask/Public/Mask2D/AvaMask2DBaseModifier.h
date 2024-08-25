// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMaskTypes.h"
#include "AvaPropertyChangeDispatcher.h"
#include "GeometryMaskTypes.h"
#include "Modifiers/AvaArrangeBaseModifier.h"
#include "StructView.h"
#include "UObject/ObjectPtr.h"

#include "AvaMask2DBaseModifier.generated.h"

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

UENUM(BlueprintType)
enum class EAvaMask2DMode : uint8
{
	Read		UMETA(DisplayName = "Target", ToolTip = "Use the specified Mask Channel to apply to this geometry"),
	Write		UMETA(DisplayName = "Source", ToolTip = "Use the specified Mask Channel to render this geometry to"),
};

/** Used for target actors to store essential information */
USTRUCT()
struct FAvaMask2DActorData
{
	GENERATED_BODY()

	/** The canvas texture to apply to this actor materials */
	UPROPERTY()
	TWeakObjectPtr<UTexture> CanvasTextureWeak;

	/** Original assigned materials, used to create instances from, and restore to when modifier removed. */
	UPROPERTY()
	TMap<FAvaMask2DComponentMaterialPath, TObjectPtr<UMaterialInterface>> OriginalMaterials;

	/** Instantiated materials corresponding to the above. */
	UPROPERTY()
	TMap<FAvaMask2DComponentMaterialPath, TObjectPtr<UMaterialInstanceDynamic>> GeneratedMaterialInstances;
};

/** Uses scene actors to create a mask texture and applies it to attached actors */
UCLASS(Abstract)
class AVALANCHEMASK_API UAvaMask2DBaseModifier
	: public UAvaArrangeBaseModifier
{
	GENERATED_BODY()

	friend class UAvaMask2DModifierShared;
	
public:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	bool UseParentChannel() const { return bUseParentChannel; }
	void SetUseParentChannel(const bool bInUseParentChannel);

	const FName GetChannel() const;
	void SetChannel(FName InChannel);

	bool IsInverted() const { return bInverted; }
	void SetIsInverted(const bool bInInvert);

	bool UseBlur() const { return bUseBlur; }
	void UseBlur(bool bInUseBlur);

	float GetBlurStrength() const { return BlurStrength; }
	void SetBlurStrength(float InBlurStrength);

	bool UseFeathering() const { return bUseFeathering; }
	void UseFeathering(bool bInUseFeathering);

	int32 GetOuterFeatherRadius() const { return OuterFeatherRadius; }
	void SetOuterFeatherRadius(int32 InFeatherRadius);

	int32 GetInnerFeatherRadius() const { return InnerFeatherRadius; }
	void SetInnerFeatherRadius(int32 InFeatherRadius);

	/** Utility to generate a unique mask name based on the associated Actor */
	FName GenerateUniqueMaskName() const;

	/** Utility to find an existing modifier on the provided actor or it's parent */
	static UAvaMask2DBaseModifier* FindMaskModifierOnActor(const AActor* InActor);

protected:
	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierRemoved(EActorModifierCoreDisableReason InReason) override;
	virtual void OnModifiedActorTransformed() override;
	virtual void SavePreState() override;
	virtual void RestorePreState() override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	virtual void SaveActorPreState(AActor* InActor, FAvaMask2DActorData& InActorData);
	virtual void RestoreActorPreState(AActor* InActor, const FAvaMask2DActorData& InActorData);

	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorParentChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousParentActor, const TArray<TWeakObjectPtr<AActor>>& InNewParentActor) override;
	virtual void OnSceneTreeTrackedActorChildrenChanged(int32 InIdx, const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	//~ End IAvaSceneTreeUpdateModifierExtension

	void OnUseParentChannelChanged();
	void OnChannelChanged();
	void OnInvertedChanged();
	void OnBlurChanged();
	void OnFeatherChanged();
	void OnCanvasChanged();

	/** Stores specific properties on the canvas locally. */	
	void CanvasParamsToLocal();

	/** Apply locally stored parameters to the canvas. */
	void LocalParamsToCanvas();

	void SetupChannelName();
	virtual void SetupMaskComponent(UActorComponent* InComponent);
	virtual void RemoveFromActor(AActor* InActor);

	void OnMaskSetCanvas(const UGeometryMaskCanvas* InCanvas, AActor* InActor);

	UActorComponent* FindOrAddMaskComponent(TSubclassOf<UActorComponent> InComponentClass, AActor* InActor);

	template<
		typename InComponentClass
		UE_REQUIRES(std::is_base_of_v<UActorComponent, InComponentClass>)>
	InComponentClass* FindOrAddMaskComponent(AActor* InActor)
	{
		return Cast<InComponentClass>(FindOrAddMaskComponent(InComponentClass::StaticClass(), InActor));
	}

	static bool ActorSupportsMaskReadWrite(const AActor* InActor);

	/** Returns true if parent channel was found */
	bool TryResolveParentChannel();

	void TryResolveCanvas();
	UTexture* TryResolveCanvasTexture(AActor* InActor, FAvaMask2DActorData& InActorData);

	UAvaObjectHandleSubsystem* GetObjectHandleSubsystem();
	UAvaMaskMaterialInstanceSubsystem* GetMaterialInstanceSubsystem();

	/** Returns or resolves the currently referenced canvas. */
	UGeometryMaskCanvas* GetCurrentCanvas();
	
	virtual void OnMaterialsChanged(UPrimitiveComponent* InPrimitiveComponent, const TArray<TSharedPtr<IAvaMaskMaterialHandle>>& InMaterialHandles);

protected:
	/** Whether to get the channel from the parent (first one that specifies a mask channel) */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = "UseParentChannel", Setter = "SetUseParentChannel", Category = "Mask2D", meta = (AllowPrivateAccess = "true", ValidEnumValues = "Self,Parent"))
	bool bUseParentChannel = false;

	/** Channel found when GetChannelFromParent is true */
	UPROPERTY(VisibleAnywhere, Category = "Mask2D", meta = (EditCondition = "bUseParentChannel", EditConditionHides))
	FName ParentChannel = NAME_None;

	/** Channel to read to or write from */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter, Setter, Category = "Mask2D", meta = (EditCondition = "!bUseParentChannel", EditConditionHides, AllowPrivateAccess = "true"))
	FName Channel = NAME_None;

	/** Whether to apply the mask as inverted (visible becomes invisible and vice versa) */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = "IsInverted", Setter = "SetIsInverted", Category = "Mask2D", meta = (AllowPrivateAccess = "true"))
	bool bInverted = false;

	UPROPERTY(BlueprintReadWrite, Getter = "UseBlur", Setter = "UseBlur", Category = "Mask2D|Shared", meta = (AllowPrivateAccess = "true"))
	bool bUseBlur = false;

	UPROPERTY(BlueprintReadWrite, Getter, Setter, Category = "Mask2D|Shared", meta = (ClampMin = 0.0, AllowPrivateAccess = "true", EditCondition = "bUseBlur", EditConditionHides))
	float BlurStrength = 16.0f;

	UPROPERTY(BlueprintReadWrite, Getter = "UseFeathering", Setter = "UseFeathering", Category = "Mask2D|Shared", meta = (AllowPrivateAccess = "true"))
	bool bUseFeathering = false;

	UPROPERTY(BlueprintReadWrite, Getter, Setter, Category = "Mask2D|Shared", meta = (ClampMin = 0, AllowPrivateAccess = "true", EditCondition = "bUseFeathering", EditConditionHides))
	int32 OuterFeatherRadius = 16;

	UPROPERTY(BlueprintReadWrite, Getter, Setter, Category = "Mask2D|Shared", meta = (ClampMin = 0, AllowPrivateAccess = "true", EditCondition = "bUseFeathering", EditConditionHides))
	int32 InnerFeatherRadius = 16;

#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<UAvaMask2DBaseModifier> PropertyChangeDispatcher;
#endif

	UPROPERTY(Transient)
	TWeakObjectPtr<UAvaObjectHandleSubsystem> ObjectHandleSubsystem;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<UAvaMaskMaterialInstanceSubsystem> MaterialInstanceSubsystem;

	/**
	 * Cached auto channel name, used when the user adds the modifier and immediately sets the mode to Source,
	 * in which case a different channel name is chosen.
	 */
	UPROPERTY(Transient)
	FName AutoChannelName;

	/** Cached actor data to apply/restore */
	UPROPERTY(DuplicateTransient)
	TMap<TWeakObjectPtr<AActor>, FAvaMask2DActorData> ActorData;

	/** Reference to the last resolved canvas name, as stored in CanvasWeak */
	UPROPERTY(Transient, DuplicateTransient)
	FName LastResolvedCanvasName;

	/** Reference to the Canvas used */
	UPROPERTY(EditAnywhere, Transient, DuplicateTransient, Category = "Mask2D|Shared", NoClear,	meta = (DisplayName = "Canvas", AllowPrivateAccess = "true", ShowInnerProperties, NoResetToDefault))
	TWeakObjectPtr<UGeometryMaskCanvas> CanvasWeak;

	/** Reference to the underlying canvas texture */
	TWeakObjectPtr<UTexture> CanvasTextureWeak;

	bool bIsRestoring = false;

	UPROPERTY(DuplicateTransient)
	TMap<TWeakObjectPtr<UObject>, FInstancedStruct> MaterialCollectionHandleData;

	UPROPERTY(DuplicateTransient)
	TMap<TWeakObjectPtr<UMaterialInterface>, FInstancedStruct> MaterialHandleData;

	TMap<TObjectKey<UObject>, TSharedPtr<IAvaMaskMaterialCollectionHandle>> MaterialCollectionHandles;
};
