// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "LiveLinkVirtualSubject.h"

#include "LiveLinkBlueprintVirtualSubject.generated.h"

// Base class for creating virtual subjects in Blueprints
UCLASS(Blueprintable, Abstract)
class LIVELINK_API ULiveLinkBlueprintVirtualSubject : public ULiveLinkVirtualSubject
{
	GENERATED_BODY()

public:
	ULiveLinkBlueprintVirtualSubject() = default;

	virtual void Initialize(FLiveLinkSubjectKey InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, ILiveLinkClient* InLiveLinkClient) override;
	virtual void Update() override;

	UFUNCTION(BlueprintImplementableEvent, Category="LiveLink")
	void OnInitialize();

	UFUNCTION(BlueprintImplementableEvent, Category="LiveLink")
	void OnUpdate();

	void UpdateVirtualSubjectStaticData(const FLiveLinkBaseStaticData* InStaticData);
	void UpdateVirtualSubjectFrameData(const FLiveLinkBaseFrameData* InFrameData, bool bInShouldStampCurrentTime);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "LiveLink", meta = (CustomStructureParam = "InStruct", BlueprintInternalUseOnly = "true", AllowAbstract = "false"))
	bool UpdateVirtualSubjectStaticData_Internal(const FLiveLinkBaseStaticData& InStruct);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "LiveLink", meta = (CustomStructureParam = "InStruct", BlueprintInternalUseOnly = "true", AllowAbstract = "false"))
	bool UpdateVirtualSubjectFrameData_Internal(const FLiveLinkBaseFrameData& InStruct, bool bInShouldStampCurrentTime);

protected:
	friend class ULiveLinkBlueprintVirtualSubjectFactory;

private:
	UScriptStruct* GetRoleStaticStruct();
	UScriptStruct* GetRoleFrameStruct();

	FLiveLinkStaticDataStruct CachedStaticData;
	

	DECLARE_FUNCTION(execUpdateVirtualSubjectStaticData_Internal);
	DECLARE_FUNCTION(execUpdateVirtualSubjectFrameData_Internal);
};
