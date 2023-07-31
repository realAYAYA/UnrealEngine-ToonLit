// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "EngineDefines.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "NavLinkHostInterface.h"
#include "NavLinkProxy.generated.h"

class UBillboardComponent;
class UNavLinkCustomComponent;
class UNavLinkRenderingComponent;
class UPathFollowingComponent;
struct FNavigationRelevantData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FSmartLinkReachedSignature, AActor*, MovingActor, const FVector&, DestinationPoint );

UCLASS(Blueprintable, autoCollapseCategories=(SmartLink, Actor), hideCategories=(Input))
class AIMODULE_API ANavLinkProxy : public AActor, public INavLinkHostInterface, public INavRelevantInterface
{
	GENERATED_UCLASS_BODY()

	/** Navigation links (point to point) added to navigation data */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=SimpleLink)
	TArray<FNavigationLink> PointLinks;
	
	/** Navigation links (segment to segment) added to navigation data
	*	@todo hidden from use until we fix segment links. Not really working now*/
	UPROPERTY()
	TArray<FNavigationSegmentLink> SegmentLinks;

private:
	/** Smart link: can affect path following */
	UPROPERTY(VisibleAnywhere, Category=SmartLink)
	TObjectPtr<UNavLinkCustomComponent> SmartLinkComp;
public:

	/** Smart link: toggle relevancy */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	bool bSmartLinkIsRelevant;

#if WITH_EDITORONLY_DATA
private:
	/** Editor Preview */
	UPROPERTY()
	TObjectPtr<UNavLinkRenderingComponent> EdRenderComp;

	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent;
public:
#endif // WITH_EDITORONLY_DATA

	// BEGIN INavRelevantInterface
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual FBox GetNavigationBounds() const override;
	virtual bool IsNavigationRelevant() const override;
	// END INavRelevantInterface

	// BEGIN INavLinkHostInterface
	virtual bool GetNavigationLinksClasses(TArray<TSubclassOf<UNavLinkDefinition> >& OutClasses) const override;
	virtual bool GetNavigationLinksArray(TArray<FNavigationLink>& OutLink, TArray<FNavigationSegmentLink>& OutSegments) const override;
	// END INavLinkHostInterface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
#endif // WITH_EDITOR
	virtual void PostRegisterAllComponents() override;
	virtual void PostLoad() override;

#if ENABLE_VISUAL_LOG
protected:
	virtual void BeginPlay() override;

public:
#endif // ENABLE_VISUAL_LOG

	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false, bool bIncludeFromChildActors = false) const override;

	//////////////////////////////////////////////////////////////////////////
	// Blueprint interface for smart links

	/** called when agent reaches smart link during path following, use ResumePathFollowing() to give control back */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveSmartLinkReached(AActor* Agent, const FVector& Destination);

	/** resume normal path following */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	void ResumePathFollowing(AActor* Agent);

	/** check if smart link is enabled */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	bool IsSmartLinkEnabled() const;

	/** change state of smart link */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	void SetSmartLinkEnabled(bool bEnabled);

	/** check if any agent is moving through smart link right now */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	bool HasMovingAgents() const;

#if WITH_EDITOR
	/** Copies navlink end points from the first entry in PointLinks array. This 
	 *	function is a helper function making up for smart links not drawing
	 *	the FVector widgets in the editor. */
	UFUNCTION(CallInEditor, Category = SmartLink, meta = (DisplayName="CopyEndPointsFromSimpleLink"))
	void CopyEndPointsFromSimpleLinkToSmartLink();
#endif // WITH_EDITOR

protected:

	UPROPERTY(BlueprintAssignable)
	FSmartLinkReachedSignature OnSmartLinkReached;

	void NotifySmartLinkReached(UNavLinkCustomComponent* LinkComp, UObject* PathingAgent, const FVector& DestPoint);

public:
	/** Returns SmartLinkComp subobject **/
	UNavLinkCustomComponent* GetSmartLinkComp() const { return SmartLinkComp; }
#if WITH_EDITORONLY_DATA
	/** Returns EdRenderComp subobject **/
	UNavLinkRenderingComponent* GetEdRenderComp() const { return EdRenderComp; }
	/** Returns SpriteComponent subobject **/
	UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
#endif
};
