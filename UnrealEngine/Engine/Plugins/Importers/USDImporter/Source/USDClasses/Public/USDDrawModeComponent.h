// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"

#include "USDDrawModeComponent.generated.h"

class UMaterialInstance;
class UUsdAssetCache2;
enum class EUsdUpAxis : uint8;

UENUM()
enum class EUsdDrawMode : int32
{
	Origin,
	Bounds,
	Cards,
	// For now we just hide these, so that the user can't pick them on the component details panel.
	// This because we don't yet have a good way of having component changes trigger resyncs: OnObjectPropertyChanged
	// is fired immediately for each property change, and resyncing right away may cause some trouble in case it
	// deletes components (which it will in this case) and the previous callstack of the OnObjectPropertyChanged still
	// wants to interact with the component, or fire OnObjectPropertyChanged for other properties, etc.
	Default UMETA(Hidden),
	Inherited UMETA(Hidden)
};

// When we're on EUsdDrawMode::Cards, describes how to draw our cards
UENUM()
enum class EUsdModelCardGeometry : int32
{
	Cross,
	Box,
	FromTexture UMETA(Hidden)	 // We don't really support this one, and will fallback to "Box"
};

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EUsdModelCardFace : int32
{
	None = 0 UMETA(Hidden),
	XPos = 1,
	YPos = 2,
	ZPos = 4,
	XNeg = 8,
	YNeg = 16,
	ZNeg = 32
};
ENUM_CLASS_FLAGS(EUsdModelCardFace);

namespace UsdUtils
{
	USDCLASSES_API EUsdModelCardFace GetOppositeFaceOnSameAxis(EUsdModelCardFace Face);
}

/**
 * Component type that is used to draw bounds, cards and origin axes when translating a prim that has the UsdGeomModelAPI schema
 * applied.
 */
UCLASS(ClassGroup = (USD), meta = (BlueprintSpawnableComponent))
class USDCLASSES_API UUsdDrawModeComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	// Separate vectors instead of an FBox so we can get Sequencer tracks for these as is
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetBoundsMin, Category = "USD")
	FVector BoundsMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetBoundsMax, Category = "USD")
	FVector BoundsMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetDrawMode, Category = "USD")
	EUsdDrawMode DrawMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetBoundsColor, Category = "USD")
	FLinearColor BoundsColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCardGeometry, Category = "USD|Cards")
	EUsdModelCardGeometry CardGeometry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCardTextureXPos, Category = "USD|Cards")
	TObjectPtr<UTexture2D> CardTextureXPos;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCardTextureYPos, Category = "USD|Cards")
	TObjectPtr<UTexture2D> CardTextureYPos;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCardTextureZPos, Category = "USD|Cards")
	TObjectPtr<UTexture2D> CardTextureZPos;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCardTextureXNeg, Category = "USD|Cards")
	TObjectPtr<UTexture2D> CardTextureXNeg;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCardTextureYNeg, Category = "USD|Cards")
	TObjectPtr<UTexture2D> CardTextureYNeg;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCardTextureZNeg, Category = "USD|Cards")
	TObjectPtr<UTexture2D> CardTextureZNeg;

private:
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstance>> MaterialInstances;

	// EUsdModelCardFace flags to enable the card faces to draw.
	// We need these because the required USD behavior specifies a difference between not having a texture because
	// one wasn't authored (at which point we flip the opposite face), and not having a texture because it failed
	// to resolve (at which point we draw vertex color). In both of those cases we'll end up with nullptr on the
	// CardTexture property, but this will help us tell which case we're talking about.
	// Note that this must be an int32 instead of the actual EUsdModelCardFace type because otherwise during any
	// transaction the UPROPERTY Enum code will freak out if we have more than one flag set, presumably consider
	// the combination of flags an invalid value for the enum, and then just set it to zero instead (see UE-200646).
	UPROPERTY()
	int32 AuthoredFaces;

public:
	UFUNCTION(BlueprintSetter, Category = "USD")
	void SetBoundsMin(const FVector& NewMin);

	UFUNCTION(BlueprintSetter, Category = "USD")
	void SetBoundsMax(const FVector& NewMax);

	UFUNCTION(BlueprintSetter, Category = "USD")
	void SetDrawMode(EUsdDrawMode NewDrawMode);

	UFUNCTION(BlueprintSetter, Category = "USD")
	void SetBoundsColor(FLinearColor NewColor);

	UFUNCTION(BlueprintSetter, Category = "USD")
	void SetCardGeometry(EUsdModelCardGeometry NewGeometry);

	UFUNCTION(BlueprintSetter, Category = "USD|Cards")
	void SetCardTextureXPos(UTexture2D* NewTexture);

	UFUNCTION(BlueprintSetter, Category = "USD|Cards")
	void SetCardTextureYPos(UTexture2D* NewTexture);

	UFUNCTION(BlueprintSetter, Category = "USD|Cards")
	void SetCardTextureZPos(UTexture2D* NewTexture);

	UFUNCTION(BlueprintSetter, Category = "USD|Cards")
	void SetCardTextureXNeg(UTexture2D* NewTexture);

	UFUNCTION(BlueprintSetter, Category = "USD|Cards")
	void SetCardTextureYNeg(UTexture2D* NewTexture);

	UFUNCTION(BlueprintSetter, Category = "USD|Cards")
	void SetCardTextureZNeg(UTexture2D* NewTexture);

	UFUNCTION(BlueprintPure, Category = "USD|Cards")
	UTexture2D* GetTextureForFace(EUsdModelCardFace Face) const;

	UFUNCTION(BlueprintCallable, Category = "USD|Cards")
	void SetTextureForFace(EUsdModelCardFace Face, UTexture2D* Texture);

public:
	UUsdDrawModeComponent();
	virtual ~UUsdDrawModeComponent() override{};

	// Begin UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual HHitProxy* CreateMeshHitProxy(int32 SectionIndex, int32 MaterialIndex) const override;
#endif
	// End UPrimitiveComponent interface

	EUsdModelCardFace GetAuthoredFaces() const;
	void SetAuthoredFaces(EUsdModelCardFace NewAuthoredFaces);

private:
	UMaterialInstance* GetOrCreateTextureMaterial(EUsdModelCardFace Face);
	void RefreshMaterialInstances();
};
