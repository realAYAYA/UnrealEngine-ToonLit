// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StageActor/IDisplayClusterStageActor.h"

#include "GameFramework/Actor.h"

#include "DisplayClusterLightCardActor.generated.h"

class ADisplayClusterRootActor;
class UActorComponent;
class UDisplayClusterLabelComponent;
class UMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class EDisplayClusterLightCardMask : uint8
{
	Circle,
	Square,
	UseTextureAlpha,
	Polygon,
	MAX	UMETA(Hidden)
};

USTRUCT(Blueprintable)
struct FLightCardAlphaGradientSettings
{
	GENERATED_BODY()

	/** Enables/disables alpha gradient effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	bool bEnableAlphaGradient = false;

	/** Starting alpha value in the gradient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnableAlphaGradient"))
	float StartingAlpha = 0;

	/** Ending alpha value in the gradient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnableAlphaGradient"))
	float EndingAlpha = 1;

	/** The angle (degrees) determines the gradient direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (EditCondition = "bEnableAlphaGradient"))
	float Angle = 0;
};

UCLASS(Blueprintable, DisplayName = "Light Card", HideCategories = (Tick, Physics, Collision, Replication, Cooking, Input, Actor))
class DISPLAYCLUSTER_API ADisplayClusterLightCardActor : public AActor, public IDisplayClusterStageActor
{
	GENERATED_BODY()

public:
	/** The default size of the porjection plane UV light cards are rendered to */
	static const float UVPlaneDefaultSize;

	/** The default distance from the view of the projection plane UV light cards are rendered to */
	static const float UVPlaneDefaultDistance;

public:
	ADisplayClusterLightCardActor(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual FName GetCustomIconName() const override;
#endif

	/** Gets the light card mesh components */
	void GetLightCardMeshComponents(TArray<UMeshComponent*>& MeshComponents) const;

	/** Returns the current static mesh used by this light card */
	UStaticMesh* GetStaticMesh() const;

	/** Sets a new static mesh for the light card */
	void SetStaticMesh(UStaticMesh* InStaticMesh);

	/** Updates the card's material instance parameters */
	void UpdateLightCardMaterialInstance();

	/** Updates the polygon texture from the polygon points */
	void UpdatePolygonTexture();

	/** Updates the light card actor visibility */
	void UpdateLightCardVisibility();

	/** Updates the UV Indicator */
	void UpdateUVIndicator();

	/** Show or hide the light card label  */
	void ShowLightCardLabel(bool bValue, float ScaleValue, ADisplayClusterRootActor* InRootActor);

	/** Set the current owner of the light card */
	void SetRootActorOwner(ADisplayClusterRootActor* InRootActor);
	
	/** Return the current owner, providing one was set */
	TWeakObjectPtr<ADisplayClusterRootActor> GetRootActorOwner() const { return RootActorOwner; }
	
	/** Add this light card to a light card layer on the given root actor */
	void AddToLightCardLayer(ADisplayClusterRootActor* InRootActor);
	
	// ~Begin IDisplayClusterStageActor interface
	/** Updates the Light Card transform based on its positional properties (Lat, Long, etc.) */
	virtual void UpdateStageActorTransform() override;
	
	/**
	 * Gets the transform in world space of the light card component
	 */
	virtual FTransform GetStageActorTransform(bool bRemoveOrigin = false) const override;

	/** Gets the object oriented bounding box of the light card component */
	virtual FBox GetBoxBounds(bool bLocalSpace = false) const override;
	
	virtual void SetLongitude(double InValue) override;
	virtual double GetLongitude() const override;

	virtual void SetLatitude(double InValue) override;
	virtual double GetLatitude() const override;

	virtual void SetDistanceFromCenter(double InValue) override;
	virtual double GetDistanceFromCenter() const override;

	virtual void SetSpin(double InValue) override;
	virtual double GetSpin() const override;

	virtual void SetPitch(double InValue) override;
	virtual double GetPitch() const override;

	virtual void SetYaw(double InValue) override;
	virtual double GetYaw() const override;

	virtual void SetRadialOffset(double InValue) override;
	virtual double GetRadialOffset() const override;

	virtual bool IsUVActor() const override;

	virtual void SetOrigin(const FTransform& InOrigin) override;
	virtual FTransform GetOrigin() const override;

	virtual void SetScale(const FVector2D& InScale) override;
	virtual FVector2D GetScale() const override;

	virtual void SetUVCoordinates(const FVector2D& InUVCoordinates) override;
	virtual FVector2D GetUVCoordinates() const override;

	virtual void GetPositionalProperties(FPositionalPropertyArray& OutPropertyPairs) const override;
	// ~End IDisplayClusterStageActor interface
	
public:

	/** Radius of light card polar coordinates. Does not include the effect of RadialOffset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation", meta = (EditCondition = "!bIsUVLightCard", HideEditConditionToggle, EditConditionHides))
	double DistanceFromCenter;

	/** Related to the Azimuth of light card polar coordinates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation", meta = (UIMin = 0, ClampMin = 0, UIMax = 360, ClampMax = 360, EditCondition = "!bIsUVLightCard", HideEditConditionToggle, EditConditionHides))
	double Longitude;

	/** Related to the Elevation of light card polar coordinates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation", meta = (UIMin = -90, ClampMin = -90, UIMax = 90, ClampMax = 90, EditCondition = "!bIsUVLightCard", HideEditConditionToggle, EditConditionHides))
	double Latitude;

	/** The UV coordinates of the light card, if it is in UV space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation", meta = (DisplayName = "UV Coodinates", EditCondition = "bIsUVLightCard", HideEditConditionToggle, EditConditionHides))
	FVector2D UVCoordinates = FVector2D(0.5, 0.5);

	/** Roll rotation of light card around its plane axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation", meta = (UIMin = -360, ClampMin = -360, UIMax = 360, ClampMax = 360))
	double Spin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation", meta = (UIMin = -360, ClampMin = -360, UIMax = 360, ClampMax = 360, EditCondition = "!bIsUVLightCard", HideEditConditionToggle, EditConditionHides))
	double Pitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation", meta = (UIMin = -360, ClampMin = -360, UIMax = 360, ClampMax = 360, EditCondition = "!bIsUVLightCard", HideEditConditionToggle, EditConditionHides))
	double Yaw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation")
	FVector2D Scale;

	/** Used by the flush constraint to offset the location of the light card form the wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation", meta = (EditCondition = "!bIsUVLightCard", HideEditConditionToggle, EditConditionHides))
	double RadialOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	EDisplayClusterLightCardMask Mask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture> Texture;

	/** Light card color, before any modifier is applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor Color;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (UIMin = 0, ClampMin = 0, UIMax = 10000, ClampMax = 10000))
	float Temperature;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (UIMin = -1, ClampMin = -1, UIMax = 1, ClampMax = 1))
	float Tint;

	/** 2^Exposure color value multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (UIMin = -100, ClampMin = -100, UIMax = 100, ClampMax = 100))
	float Exposure;

	/** Linear color value multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (UIMin = 0, ClampMin = 0))
	float Gain;

	/** Linear alpha multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (UIMin = 0, ClampMin = 0, UIMax = 1, ClampMax = 1))
	float Opacity;

	/** Feathers in the alpha from the edges */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (UIMin = 0, ClampMin = 0))
	float Feathering;

	/** Settings related to an alpha gradient effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLightCardAlphaGradientSettings AlphaGradient;

	/** A flag that controls wether the light card's location and rotation are locked to its "owning" root actor */
	UPROPERTY()
	bool bLockToOwningRootActor = true;

	/** Indicates if the light card exists in 3D space or in UV space */
	UPROPERTY()
	bool bIsUVLightCard = false;

	/** Polygon points when using this type of mask */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TArray<FVector2D> Polygon;

	/** Used to flag this light card as a proxy of a "real" light card. Used by the LightCard Editor */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> PolygonMask = nullptr;

	static FName GetExtenderNameToComponentMapMemberName()
	{
		return GET_MEMBER_NAME_CHECKED(ADisplayClusterLightCardActor, ExtenderNameToComponentMap);
	}

protected:
	/** Creates components for IDisplayClusterLightCardActorExtender */
	void CreateComponentsForExtenders();

	/** Removes components that were added by IDisplayClusterLightCardActorExtender */
	void CleanUpComponentsForExtenders();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Default")
	TObjectPtr<USceneComponent> DefaultSceneRootComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Default")
	TObjectPtr<USpringArmComponent> MainSpringArmComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Default")
	TObjectPtr<USceneComponent> LightCardTransformerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default")
	TObjectPtr<UStaticMeshComponent> LightCardComponent;

	/** Components added by the IDisplayLightCardActorExtender */
	UPROPERTY(VisibleAnywhere, Category = "Default")
	TMap<FName, TObjectPtr<UActorComponent>> ExtenderNameToComponentMap;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Default")
	TObjectPtr<UDisplayClusterLabelComponent> LabelComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Default")
	TObjectPtr<UStaticMeshComponent> UVIndicatorComponent;
#endif

	/** The current owner of the light card */
	TWeakObjectPtr<ADisplayClusterRootActor> RootActorOwner;

private:
	/** Stores the user translucency value when labels are displayed */
	TOptional<int32> SavedTranslucencySortPriority;
	
};
