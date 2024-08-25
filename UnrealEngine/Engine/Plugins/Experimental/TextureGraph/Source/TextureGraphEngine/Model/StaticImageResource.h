// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModelObject.h"
#include "Device/FX/Device_FX.h"
#include "Mix/MixUpdateCycle.h"
#include "StaticImageResource.generated.h"

//////////////////////////////////////////////////////////////////////////
/// Base static image resource class
////////////////////////////////////////////////////////////////////////// 
UCLASS(Blueprintable, BlueprintType)
class TEXTUREGRAPHENGINE_API UStaticImageResource : public UModelObject
{
public:
	GENERATED_BODY()
	
private:
	friend class Job_LoadStaticImageResource;
	
	UPROPERTY()
	FString 						AssetUUID;						/// Unique id for the asset within the entire system
	
	TiledBlobPtr					BlobObj;						/// The blob that represents the data for this source

	virtual AsyncTiledBlobRef		Load(MixUpdateCyclePtr Cycle);
	
public:
	virtual							~UStaticImageResource() override;
	
	virtual TiledBlobPtr			GetBlob(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredDesc, int32 TargetId);

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE const FString&		GetAssetUUID() const { return AssetUUID; }
	FORCEINLINE void				SetAssetUUID(const FString& UUID) { AssetUUID = UUID; }
};

