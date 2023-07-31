// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "NavAreas/NavArea.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "NavLinkCustomInterface.h"
#include "NavRelevantComponent.h"
#include "NavLinkCustomComponent.generated.h"


struct FNavigationRelevantData;

/**
 *  Encapsulates NavLinkCustomInterface interface, can be used with Actors not relevant for navigation
 *  
 *  Additional functionality:
 *  - can be toggled
 *  - can create obstacle area for easier/forced separation of link end points
 *  - can broadcast state changes to nearby agents
 */

UCLASS()
class NAVIGATIONSYSTEM_API UNavLinkCustomComponent : public UNavRelevantComponent, public INavLinkCustomInterface
{
	GENERATED_UCLASS_BODY()

	DECLARE_DELEGATE_ThreeParams(FOnMoveReachedLink, UNavLinkCustomComponent* /*ThisComp*/, UObject* /*PathComp*/, const FVector& /*DestPoint*/);
	DECLARE_DELEGATE_TwoParams(FBroadcastFilter, UNavLinkCustomComponent* /*ThisComp*/, TArray<UObject*>& /*NotifyList*/);

	// BEGIN INavLinkCustomInterface
	virtual void GetLinkData(FVector& LeftPt, FVector& RightPt, ENavLinkDirection::Type& Direction) const override;
	virtual void GetSupportedAgents(FNavAgentSelector& OutSupportedAgents) const override;
	virtual TSubclassOf<UNavArea> GetLinkAreaClass() const override;
	virtual uint32 GetLinkId() const override;
	virtual void UpdateLinkId(uint32 NewUniqueId) override;
	virtual bool IsLinkPathfindingAllowed(const UObject* Querier) const override;
	virtual bool OnLinkMoveStarted(UObject* PathComp, const FVector& DestPoint) override;
	virtual void OnLinkMoveFinished(UObject* PathComp) override;
	// END INavLinkCustomInterface

	//~ Begin UNavRelevantComponent Interface
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual void CalcAndCacheBounds() const override;
	//~ End UNavRelevantComponent Interface

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	//~ End UActorComponent Interface

	void ApplyComponentInstanceData(struct FNavLinkCustomInstanceData* ComponentInstanceData);

	//~ Begin UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif
	//~ End UObject Interface

	/** set basic link data: end points and direction */
	void SetLinkData(const FVector& RelativeStart, const FVector& RelativeEnd, ENavLinkDirection::Type Direction);
	virtual FNavigationLink GetLinkModifier() const;

	/** set area class to use when link is enabled */
	void SetEnabledArea(TSubclassOf<UNavArea> AreaClass);
	TSubclassOf<UNavArea> GetEnabledArea() const { return EnabledAreaClass; }

	/** set area class to use when link is disabled */
	void SetDisabledArea(TSubclassOf<UNavArea> AreaClass);
	TSubclassOf<UNavArea> GetDisabledArea() const { return DisabledAreaClass; }

	void SetSupportedAgents(const FNavAgentSelector& InSupportedAgents) { SupportedAgents = InSupportedAgents; }
	FNavAgentSelector GetSupportedAgents() const { return SupportedAgents; }

	/** add box obstacle during generation of navigation data
	  * this can be used to create empty area under doors */
	void AddNavigationObstacle(TSubclassOf<UNavArea> AreaClass, const FVector& BoxExtent, const FVector& BoxOffset = FVector::ZeroVector);

	/** removes simple obstacle */
	void ClearNavigationObstacle();

	/** set properties of trigger around link entry point(s), that will notify nearby agents about link state change */
	void SetBroadcastData(float Radius, ECollisionChannel TraceChannel = ECC_Pawn, float Interval = 0.0f);

	void SendBroadcastWhenEnabled(bool bEnabled);
	void SendBroadcastWhenDisabled(bool bEnabled);

	/** set delegate to filter  */
	void SetBroadcastFilter(FBroadcastFilter const& InDelegate);

	/** change state of smart link (used area class) */
	void SetEnabled(bool bNewEnabled);
	bool IsEnabled() const { return bLinkEnabled; }
	
	/** set delegate to notify about reaching this link during path following */
	void SetMoveReachedLink(FOnMoveReachedLink const& InDelegate);

	/** check is any agent is currently moving though this link */
	bool HasMovingAgents() const;

	/** get link start point in world space */
	FVector GetStartPoint() const;

	/** get link end point in world space */
	FVector GetEndPoint() const;

	//////////////////////////////////////////////////////////////////////////
	// helper functions for setting delegates

	template< class UserClass >	
	FORCEINLINE void SetMoveReachedLink(UserClass* TargetOb, typename FOnMoveReachedLink::TMethodPtr< UserClass > InFunc)
	{
		SetMoveReachedLink(FOnMoveReachedLink::CreateUObject(TargetOb, InFunc));
	}
	template< class UserClass >	
	FORCEINLINE void SetMoveReachedLink(UserClass* TargetOb, typename FOnMoveReachedLink::TConstMethodPtr< UserClass > InFunc)
	{
		SetMoveReachedLink(FOnMoveReachedLink::CreateUObject(TargetOb, InFunc));
	}

	template< class UserClass >	
	FORCEINLINE void SetBroadcastFilter(UserClass* TargetOb, typename FBroadcastFilter::TMethodPtr< UserClass > InFunc)
	{
		SetBroadcastFilter(FBroadcastFilter::CreateUObject(TargetOb, InFunc));
	}
	template< class UserClass >	
	FORCEINLINE void SetBroadcastFilter(UserClass* TargetOb, typename FBroadcastFilter::TConstMethodPtr< UserClass > InFunc)
	{
		SetBroadcastFilter(FBroadcastFilter::CreateUObject(TargetOb, InFunc));
	}
	
protected:

	/** link Id assigned by navigation system */
	UPROPERTY()
	uint32 NavLinkUserId;

	/** area class to use when link is enabled */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	TSubclassOf<UNavArea> EnabledAreaClass;

	/** area class to use when link is disabled */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	TSubclassOf<UNavArea> DisabledAreaClass;

	/** restrict area only to specified agents */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	FNavAgentSelector SupportedAgents;

	/** start point, relative to owner */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	FVector LinkRelativeStart;

	/** end point, relative to owner */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	FVector LinkRelativeEnd;

	/** direction of link */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	TEnumAsByte<ENavLinkDirection::Type> LinkDirection;

	/** is link currently in enabled state? (area class) */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	uint32 bLinkEnabled : 1;

	/** should link notify nearby agents when it changes state to enabled */
	UPROPERTY(EditAnywhere, Category=Broadcast)
	uint32 bNotifyWhenEnabled : 1;

	/** should link notify nearby agents when it changes state to disabled */
	UPROPERTY(EditAnywhere, Category=Broadcast)
	uint32 bNotifyWhenDisabled : 1;

	/** if set, box obstacle area will be added to generation */
	UPROPERTY(EditAnywhere, Category=Obstacle)
	uint32 bCreateBoxObstacle : 1;

	/** offset of simple box obstacle */
	UPROPERTY(EditAnywhere, Category=Obstacle)
	FVector ObstacleOffset;

	/** extent of simple box obstacle */
	UPROPERTY(EditAnywhere, Category=Obstacle)
	FVector ObstacleExtent;

	/** area class for simple box obstacle */
	UPROPERTY(EditAnywhere, Category=Obstacle)
	TSubclassOf<UNavArea> ObstacleAreaClass;

	/** radius of state change broadcast */
	UPROPERTY(EditAnywhere, Category=Broadcast)
	float BroadcastRadius;

	/** interval for state change broadcast (0 = single broadcast) */
	UPROPERTY(EditAnywhere, Category=Broadcast, Meta=(UIMin="0.0", ClampMin="0.0"))
	float BroadcastInterval;

	/** trace channel for state change broadcast */
	UPROPERTY(EditAnywhere, Category=Broadcast)
	TEnumAsByte<ECollisionChannel> BroadcastChannel;

	/** delegate to call when link is reached */
	FBroadcastFilter OnBroadcastFilter;

	/** list of agents moving though this link */
	TArray<TWeakObjectPtr<UObject> > MovingAgents;

	/** delegate to call when link is reached */
	FOnMoveReachedLink OnMoveReachedLink;

	/** Handle for efficient management of BroadcastStateChange timer */
	FTimerHandle TimerHandle_BroadcastStateChange;

	/** notify nearby agents about link changing state */
	void BroadcastStateChange();
	
	/** gather agents to notify about state change */
	void CollectNearbyAgents(TArray<UObject*>& NotifyList);
};

/** Used to store navlink data during RerunConstructionScripts */
USTRUCT()
struct FNavLinkCustomInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()

public:
	FNavLinkCustomInstanceData() = default;
	FNavLinkCustomInstanceData(const UNavLinkCustomComponent* SourceComponent)
		: FActorComponentInstanceData(SourceComponent)
		, NavLinkUserId(0)
	{}

	virtual ~FNavLinkCustomInstanceData() = default;

	virtual bool ContainsData() const override
	{
		return true;
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		Super::ApplyToComponent(Component, CacheApplyPhase);

		if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
		{
			CastChecked<UNavLinkCustomComponent>(Component)->ApplyComponentInstanceData(this);
		}
	}

	UPROPERTY()
	uint32 NavLinkUserId = 0;
};