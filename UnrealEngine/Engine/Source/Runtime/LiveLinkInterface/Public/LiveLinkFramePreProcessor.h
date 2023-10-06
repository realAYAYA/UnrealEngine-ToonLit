// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "Templates/SubclassOf.h"

#include "LiveLinkFramePreProcessor.generated.h"


/**
 * Basic object to apply preprocessing to a live link frame.
 * Inherit from it to add specific operations / options for a certain type of data
 * @note It can be called from any thread
 */
class ILiveLinkFramePreProcessorWorker
{
public:
	virtual TSubclassOf<ULiveLinkRole> GetRole() const = 0;
	virtual bool PreProcessFrame(FLiveLinkFrameDataStruct& InOutFrame) const = 0;
};


/**
 * Basic object to apply preprocessing to a live link frame. 
 * Inherit from it to add specific operations / options for a certain type of data
 * @note It can only be used on the Game Thread. See ILiveLinkFramePreProcessorWorker for the any thread implementation.
 */
UCLASS(Abstract, Blueprintable, editinlinenew, ClassGroup = (LiveLink), MinimalAPI)
class ULiveLinkFramePreProcessor : public UObject
{
	GENERATED_BODY()

public:
	using FWorkerSharedPtr = TSharedPtr<ILiveLinkFramePreProcessorWorker, ESPMode::ThreadSafe>;

	LIVELINKINTERFACE_API virtual TSubclassOf<ULiveLinkRole> GetRole() const PURE_VIRTUAL(ULiveLinkFramePreProcessor::GetFromRole, return TSubclassOf<ULiveLinkRole>(););
	LIVELINKINTERFACE_API virtual FWorkerSharedPtr FetchWorker() PURE_VIRTUAL(ULiveLinkFramePreProcessor::FetchWorker, return FWorkerSharedPtr(););
};
