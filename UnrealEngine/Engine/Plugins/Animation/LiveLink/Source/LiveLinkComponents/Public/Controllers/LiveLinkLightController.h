// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkControllerBase.h"
#include "LiveLinkTransformController.h"
#include "Engine/EngineTypes.h"
#include "LiveLinkLightController.generated.h"


/**
 * Controller that uses LiveLink light data to drive a light component. 
 * UPointLightComponent and USpotLightComponent are supported for specific properties
 */
UCLASS()
class LIVELINKCOMPONENTS_API ULiveLinkLightController : public ULiveLinkControllerBase
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FComponentReference ComponentToControl_DEPRECATED;

	UPROPERTY()
	FLiveLinkTransformControllerData TransformData_DEPRECATED;
#endif

public:

	//~ Begin ULiveLinkControllerBase interface
	virtual void Tick(float DeltaTime, const FLiveLinkSubjectFrameData& SubjectData) override;
	virtual bool IsRoleSupported(const TSubclassOf<ULiveLinkRole>& RoleToSupport) override;
	virtual TSubclassOf<UActorComponent> GetDesiredComponentClass() const override;
	//~ End ULiveLinkControllerBase interface

	//~ Begin UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface
};