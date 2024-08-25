// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Templates/SubclassOf.h"
#include "AvaDynamicMeshConverterModifier.generated.h"

class UBrushComponent;
class UDynamicMeshComponent;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/** Components that can be converted to dynamic mesh */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EAvaDynamicMeshConverterModifierType : uint8
{
	None = 0 UMETA(Hidden),
	StaticMeshComponent = 1 << 0,
	DynamicMeshComponent = 1 << 1,
	SkeletalMeshComponent = 1 << 2,
	BrushComponent = 1 << 3,
	ProceduralMeshComponent = 1 << 4
};
ENUM_CLASS_FLAGS(EAvaDynamicMeshConverterModifierType);

UENUM(BlueprintType)
enum class EAvaDynamicMeshConverterModifierFilter : uint8
{
	None,
	Include,
	Exclude
};

USTRUCT()
struct FAvaDynamicMeshConverterModifierComponentState
{
	GENERATED_BODY()

	explicit FAvaDynamicMeshConverterModifierComponentState() {}
	explicit FAvaDynamicMeshConverterModifierComponentState(UPrimitiveComponent* InPrimitiveComponent);

	/** The component we are converting to dynamic mesh */
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> Component = nullptr;

	/** The default visibility of the actor converted component in game */
	UPROPERTY()
	bool bActorHiddenInGame = true;

	/** The default visibility of the actor converted component in editor */
	UPROPERTY()
	bool bActorHiddenInEditor = true;

	/** The default visibility of the actor converted component in game */
	UPROPERTY()
	bool bComponentHiddenInGame = false;

	/** The default visibility of the converted component in editor */
	UPROPERTY()
	bool bComponentVisible = true;
	/** The component converted dynamic mesh*/
	UE::Geometry::FDynamicMesh3 Mesh;
};

UCLASS(MinimalAPI, BlueprintType)
class UAvaDynamicMeshConverterModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	AVALANCHEMODIFIERS_API void SetSourceActorWeak(const TWeakObjectPtr<AActor>& InActor);
	TWeakObjectPtr<AActor> GetSourceActorWeak() const
	{
		return SourceActorWeak;
	}

	AVALANCHEMODIFIERS_API void SetComponentType(int32 InComponentType);
	int32 GetComponentType() const
	{
		return ComponentType;
	}

	AVALANCHEMODIFIERS_API void SetFilterActorMode(EAvaDynamicMeshConverterModifierFilter InFilter);
	EAvaDynamicMeshConverterModifierFilter GetFilterActorMode() const
	{
		return FilterActorMode;
	}

	AVALANCHEMODIFIERS_API void SetFilterActorClasses(const TSet<TSubclassOf<AActor>>& InClasses);
	const TSet<TSubclassOf<AActor>>& GetFilterActorClasses() const
	{
		return FilterActorClasses;
	}

	AVALANCHEMODIFIERS_API void SetIncludeAttachedActors(bool bInInclude);
	bool GetIncludeAttachedActors() const
	{
		return bIncludeAttachedActors;
	}

	AVALANCHEMODIFIERS_API void SetHideConvertedMesh(bool bInHide);
	bool GetHideConvertedMesh() const
	{
		return bHideConvertedMesh;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	/** Export current dynamic mesh to static mesh asset */
	UFUNCTION(CallInEditor, Category="DynamicMeshConverter")
	virtual void ConvertToStaticMeshAsset();
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void RestorePreState() override;
	virtual void Apply() override;
	virtual void OnModifierRemoved(EActorModifierCoreDisableReason InReason) override;
	//~ End UActorModifierCoreBase

	void OnSourceActorChanged();

	void ConvertComponents(TArray<FAvaDynamicMeshConverterModifierComponentState>& OutResults) const;
	bool HasFlag(EAvaDynamicMeshConverterModifierType InFlag) const;

	void AddDynamicMeshComponent();
	void RemoveDynamicMeshComponent();

	void GetFilteredActors(TArray<AActor*>& OutActors) const;

	void GetStaticMeshComponents(const TArray<AActor*>& InActors, TArray<UStaticMeshComponent*>& OutComponents) const;
	void GetDynamicMeshComponents(const TArray<AActor*>& InActors, TArray<UDynamicMeshComponent*>& OutComponents) const;
	void GetSkeletalMeshComponents(const TArray<AActor*>& InActors, TArray<USkeletalMeshComponent*>& OutComponents) const;
	void GetBrushComponents(const TArray<AActor*>& InActors, TArray<UBrushComponent*>& OutComponents) const;
	void GetProceduralMeshComponents(const TArray<AActor*>& InActors, TArray<UProceduralMeshComponent*>& OutComponents) const;

	/** Triggers the update of the mesh */
	UFUNCTION(CallInEditor, Category="DynamicMeshConverter")
	void ConvertMesh();

	/** What actor should we copy from, by default is self */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSourceActorWeak", Getter="GetSourceActorWeak", Category="DynamicMeshConverter", meta=(DisplayName="SourceActor", AllowPrivateAccess="true"))
	TWeakObjectPtr<AActor> SourceActorWeak = GetModifiedActor();

	/** Which components should we take into account for the conversion */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetComponentType", Getter="GetComponentType", Category="DynamicMeshConverter", meta=(Bitmask, BitmaskEnum="/Script/AvalancheModifiers.EAvaDynamicMeshConverterModifierType", AllowPrivateAccess="true"))
	int32 ComponentType = static_cast<int32>(
		EAvaDynamicMeshConverterModifierType::StaticMeshComponent |
		EAvaDynamicMeshConverterModifierType::DynamicMeshComponent |
		EAvaDynamicMeshConverterModifierType::SkeletalMeshComponent |
		EAvaDynamicMeshConverterModifierType::BrushComponent |
		EAvaDynamicMeshConverterModifierType::ProceduralMeshComponent);

	/** Actor filter mode : none, include or exclude specific actor class */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetFilterActorMode", Getter="GetFilterActorMode", Category="DynamicMeshConverter", meta=(AllowPrivateAccess="true"))
	EAvaDynamicMeshConverterModifierFilter FilterActorMode = EAvaDynamicMeshConverterModifierFilter::None;

	/** Actor class to use as filter when gathering actors to convert */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetFilterActorClasses", Getter="GetFilterActorClasses", Category="DynamicMeshConverter", meta=(EditCondition="FilterActorMode != EAvaDynamicMeshConverterModifierFilter::None", AllowPrivateAccess="true"))
	TSet<TSubclassOf<AActor>> FilterActorClasses;

	/** Checks and convert all attached actors below this actor */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetIncludeAttachedActors", Getter="GetIncludeAttachedActors", Category="DynamicMeshConverter", meta=(AllowPrivateAccess="true"))
	bool bIncludeAttachedActors = true;

	/** Change visibility of source mesh once they are converted to dynamic mesh, by default will convert itself so hide converted mesh is true */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetHideConvertedMesh", Getter="GetHideConvertedMesh", Category="DynamicMeshConverter", meta=(AllowPrivateAccess="true"))
	bool bHideConvertedMesh = true;

	/** Did we create the dynamic mesh component from this modifier or retrieved it */
	UPROPERTY()
	bool bComponentCreated = false;

	/** Components converted to dynamic mesh */
	UPROPERTY()
	TArray<FAvaDynamicMeshConverterModifierComponentState> ConvertedComponents;

	/** Cached mesh to set state without reconverting again */
	TOptional<UE::Geometry::FDynamicMesh3> ConvertedMesh;

	/** Do we convert mesh on next execution */
	bool bConvertMesh = false;
};
