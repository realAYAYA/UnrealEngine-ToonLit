// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "WorldPartition/ContentBundle/ContentBundleStatus.h"
#include "WorldPartition/ContentBundle/ContentBundleBase.h"

class UContentBundleDescriptor;
class FContentBundleClient;
class IWorldPartitionCookPackageContext;
class URuntimeHashExternalStreamingObjectBase;

class ENGINE_API FContentBundle : public FContentBundleBase
{
public:
	FContentBundle(TSharedPtr<FContentBundleClient>& InClient, UWorld* InWorld);

	//~ Begin IContentBundle Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual bool IsValid() const override;
	//~ End IContentBundle Interface

	static int32 GetContentBundleEpoch() { return ContentBundlesEpoch; }

protected:
	//~ Begin IContentBundle Interface
	virtual void DoInitialize() override;
	virtual void DoUninitialize() override;
	virtual void DoInjectContent() override;
	virtual void DoRemoveContent() override;
	//~ End IContentBundle Interface
	
private:
#if WITH_EDITOR
	void InitializeForPIE();
#endif

	static int32 ContentBundlesEpoch;

	UPackage* ExternalStreamingObjectPackage;
	URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject;
};