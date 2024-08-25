// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/FMutableTaskGraph.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/TextureMipDataProviderFactory.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuR/Image.h"
#include "Streaming/TextureMipDataProvider.h"

#include "MuCO/DefaultImageProvider.h"
#include "MuR/System.h"

#include "CustomizableObjectMipDataProvider.generated.h"

enum EPixelFormat : uint8;
namespace mu { class Parameters; }
namespace mu { class System; }

class FThreadSafeCounter;
class UCustomizableObjectInstance;
class UObject;
class UTexture;

/** This struct stores the data relevant for the construction of a specific texture. 
* This includes all the data required to rebuild the image (or any of its mips).
*/
class FMutableUpdateContext
{
public:
	FMutableUpdateContext() = default;
	
	FMutableUpdateContext(const FString& InCustomizableObjectPathName, const FString& InInstancePathName, mu::Ptr<mu::System> InSystem,
		TSharedPtr<mu::Model, ESPMode::ThreadSafe> InModel, mu::Ptr<const mu::Parameters> InParameters, int32 InState);

	~FMutableUpdateContext();

	const FString& GetCustomizableObjectPathName() const;

	const FString& GetInstancePathName() const;
	
	mu::Ptr<mu::System> GetSystem() const;
	
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> GetModel() const;

	mu::Ptr<const mu::Parameters> GetParameters() const;
	
	int32 GetState() const;

	const TArray<mu::Ptr<const mu::Image>>& GetImageParameterValues() const;

	FString CapturedDescriptor;
	
private:
	FString CustomizableObjectPathName;
	FString InstancePathName;

	mu::Ptr<mu::System> System;
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model;
	mu::Ptr<const mu::Parameters> Parameters;
	int32 State = -1;

	TArray<mu::Ptr<const mu::Image>> ImageParameterValues;
};


struct FMutableMipUpdateLevel
{
	FMutableMipUpdateLevel(int32 InMipLevel, void* InDest, int32 InSizeX, int32 InSizeY, int32 InDataSize, EPixelFormat InFormat) :
		Dest(InDest), MipLevel(InMipLevel), SizeX(InSizeX), SizeY(InSizeY), DataSize(InDataSize), Format(InFormat) {}

	void* Dest; // Only access from the FMutableTextureMipDataProvider, owned by the FTextureMipInfoArray so don't delete
	int32 MipLevel;
	int32 SizeX;
	int32 SizeY;
	int32 DataSize;
	EPixelFormat Format;
};


/** Runtime data used during a mutable image mipmap update */
struct FMutableImageOperationData
{
	/** This option comes from the operation request. It is used to reduce the number of mipmaps that mutable must generate for images.  */
	int32 MipsToSkip = 0;
	FMutableImageReference RequestedImage;

	TSharedPtr<FMutableUpdateContext> UpdateContext;

	mu::ImagePtrConst Result;

	TArray<FMutableMipUpdateLevel> Levels;

	// Used to sync with the FMutableTextureMipDataProvider and FRenderAssetUpdate::Tick
	FThreadSafeCounter* Counter;
	FTextureUpdateSyncOptions::FCallback RescheduleCallback;

	/** Access to the Counter must be protected with this because it may be accessed from another thread to null it. */
	FCriticalSection CounterTaskLock;

	// Image Update Memory stats
	int64 ImageUpdateStartBytes = 0;
};


class FMutableTextureMipDataProvider : public FTextureMipDataProvider
{
public:
	FMutableTextureMipDataProvider(const UTexture* Texture, UCustomizableObjectInstance* InCustomizableObjectInstance, const FMutableImageReference& InImageRef);

	virtual void Init(const FTextureUpdateContext& Context, const FTextureUpdateSyncOptions& SyncOptions) override;
	virtual int32 GetMips(const FTextureUpdateContext& Context, int32 StartingMipIndex, const FTextureMipInfoArray& MipInfos, const FTextureUpdateSyncOptions& SyncOptions) override;
	virtual bool PollMips(const FTextureUpdateSyncOptions& SyncOptions) override;
	virtual void CleanUp(const FTextureUpdateSyncOptions& SyncOptions) override;
	virtual void Cancel(const FTextureUpdateSyncOptions& SyncOptions) override;
	virtual ETickThread GetCancelThread() const override;
	void AbortPollMips() override;

private:
	void CancelCounterSafely();
	void PrintWarningAndAdvanceToCleanup();

public:
	// Todo: Simplify by replacing the reference to the Instance with some static parametrization or hash with enough info to reconstruct the texture
	UPROPERTY(Transient)
	UCustomizableObjectInstance* CustomizableObjectInstance = nullptr;

	FMutableImageReference ImageRef;
	TSharedPtr<FMutableUpdateContext> UpdateContext;

	bool bRequestAborted = false;

	TSharedPtr<FMutableImageOperationData> OperationData;

	uint32 MutableTaskId = FMutableTaskGraph::INVALID_ID;
};


UCLASS(hidecategories=Object)
class CUSTOMIZABLEOBJECT_API UMutableTextureMipDataProviderFactory : public UTextureMipDataProviderFactory
{
	GENERATED_UCLASS_BODY()

public:
	virtual FTextureMipDataProvider* AllocateMipDataProvider(UTexture* Asset) override
	{
		check(ImageRef.ImageID > 0);
		FMutableTextureMipDataProvider* Result = new FMutableTextureMipDataProvider(Asset, CustomizableObjectInstance, ImageRef);
		Result->UpdateContext = UpdateContext;		
		return Result;
	}

	virtual bool WillProvideMipDataWithoutDisk() const override
	{ 
		return true;
	}

	// Todo: Simplify by replacing the reference to the Instance with some static parametrization or hash with enough info to reconstruct the texture
	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance = nullptr;

	FMutableImageReference ImageRef;
	TSharedPtr<FMutableUpdateContext> UpdateContext;

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MuR/Parameters.h"
#include "MuR/System.h"
#endif
