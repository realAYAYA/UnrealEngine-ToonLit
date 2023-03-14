// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"
#include "LiveLinkControllerBase.h"
#include "Engine/EngineTypes.h"
#include "LiveLinkTransformController.generated.h"

struct FLiveLinkTransformStaticData;
class USceneComponent;

USTRUCT(BlueprintType)
struct LIVELINKCOMPONENTS_API FLiveLinkTransformControllerData
{
	GENERATED_BODY()

	/** Set the transform of the component in world space of in its local reference frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LiveLink")
	bool bWorldTransform = false;

	/** Whether we should set the owning actor's location with the value coming from live link. */
	UPROPERTY(EditAnywhere, Category = "LiveLink", AdvancedDisplay)
	bool bUseLocation = true;

	/** Whether we should set the owning actor's rotation with the value coming from live link. */
	UPROPERTY(EditAnywhere, Category = "LiveLink", AdvancedDisplay)
	bool bUseRotation = true;
	
	/** Whether we should set the owning actor's scale with the value coming from live link. */
	UPROPERTY(EditAnywhere, Category = "LiveLink", AdvancedDisplay)
	bool bUseScale = true;

	/**
	 * Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 * Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LiveLink", AdvancedDisplay)
	bool bSweep = false;

	/**
	 * Whether we teleport the physics state (if physics collision is enabled for this object).
	 * If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 * If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 * If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LiveLink", AdvancedDisplay)
	bool bTeleport = true;
	
	void ApplyTransform(USceneComponent* SceneComponent, const FTransform& Transform, const FLiveLinkTransformStaticData& StaticData) const;
	void CheckForError(FName OwnerName, USceneComponent* SceneComponent) const;
};

/**
 */
UCLASS()
class LIVELINKCOMPONENTS_API ULiveLinkTransformController : public ULiveLinkControllerBase
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FComponentReference ComponentToControl_DEPRECATED;
#endif //WITH_EDITORONLY_DATA
	
	UPROPERTY(EditAnywhere, Category="LiveLink", meta=(ShowOnlyInnerProperties))
	FLiveLinkTransformControllerData TransformData;

public:
	//~Begin ULiveLinkControllerBase interface
	virtual void OnEvaluateRegistered() override;
	virtual void Tick(float DeltaTime, const FLiveLinkSubjectFrameData& SubjectData) override;
	virtual bool IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport) override;
	virtual TSubclassOf<UActorComponent> GetDesiredComponentClass() const override;
	virtual void SetAttachedComponent(UActorComponent* ActorComponent) override;
	//~End ULiveLinkControllerBase interface

	//~ Begin UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface
};