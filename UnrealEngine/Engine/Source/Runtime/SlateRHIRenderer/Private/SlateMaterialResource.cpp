// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateMaterialResource.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"

namespace SlateMaterialResource
{

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	void CheckInvalidUMaterial(const UMaterialInterface& InMaterialResource, const FName& InDebugName)
	{
		if (GSlateCheckUObjectRenderResources)
		{
			bool bIsValidLowLevel = InMaterialResource.IsValidLowLevelFast(false);
			if (!bIsValidLowLevel || !IsValid(&InMaterialResource) || InMaterialResource.GetClass() == UMaterialInterface::StaticClass())
			{
				UE_LOG(LogSlate, Error, TEXT("Material '%s' is not valid. PendingKill:'%d'. ValidLowLevelFast:'%d'. InvalidClass:'%d'")
					, *InDebugName.ToString()
					, (bIsValidLowLevel ? !IsValid(&InMaterialResource) : false)
					, bIsValidLowLevel
					, (bIsValidLowLevel ? InMaterialResource.GetClass() == UMaterialInterface::StaticClass() : false));

				const TCHAR* Message = TEXT("We detected an invalid resource in FSlateMaterialResource. Check the log for more detail.");
				if (GSlateCheckUObjectRenderResourcesShouldLogFatal)
				{
					UE_LOG(LogSlate, Fatal, TEXT("%s"), Message);
				}
				else
				{
					ensureAlwaysMsgf(false, TEXT("%s"), Message);
				}
			}
		}
	}

	void CheckInvalidMaterialProxy(const FMaterialRenderProxy* MaterialProxy, const FName& InDebugName)
	{
		if (GSlateCheckUObjectRenderResources)
		{
			if (MaterialProxy == nullptr || MaterialProxy->IsDeleted() || MaterialProxy->IsMarkedForGarbageCollection())
			{
				UE_LOG(LogSlate, Error, TEXT("Material '%s' Render Proxy is: nullptr:'%d'. Deleted:'%d'. Marked for GC:'%d'")
					, *InDebugName.ToString()
					, (MaterialProxy == nullptr)
					, (MaterialProxy ? MaterialProxy->IsDeleted() : false)
					, (MaterialProxy ? MaterialProxy->IsMarkedForGarbageCollection() : false));

				const TCHAR* Message = TEXT("We detected an invalid resource render proxy in FSlateMaterialResource. Check the log for more detail.");
				if (GSlateCheckUObjectRenderResourcesShouldLogFatal)
				{
					UE_LOG(LogSlate, Fatal, TEXT("%s"), Message);
				}
				else
				{
					ensureAlwaysMsgf(false, TEXT("%s"), Message);
				}
			}
		}
	}
#endif

}

FSlateMaterialResource::FSlateMaterialResource(const UMaterialInterface& InMaterialResource, const FVector2f InImageSize, FSlateShaderResource* InTextureMask )
	: MaterialObject( &InMaterialResource)
	, SlateProxy( new FSlateShaderResourceProxy )
	, TextureMaskResource( InTextureMask )
	, Width(FMath::RoundToInt(InImageSize.X))
	, Height(FMath::RoundToInt(InImageSize.Y))
{
#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	SlateMaterialResource::CheckInvalidUMaterial(InMaterialResource, NAME_None);

	MaterialProxy = InMaterialResource.GetRenderProxy();

	MaterialObjectWeakPtr = MaterialObject;
	UpdateMaterialName();

	SlateMaterialResource::CheckInvalidMaterialProxy(MaterialProxy, DebugName);
#else
	MaterialProxy = InMaterialResource.GetRenderProxy();
#endif

	SlateProxy->ActualSize = InImageSize.IntPoint();
	SlateProxy->Resource = this;

	if (MaterialProxy && (MaterialProxy->IsDeleted() || MaterialProxy->IsMarkedForGarbageCollection()))
	{
		MaterialProxy = nullptr;
	}
}

FSlateMaterialResource::~FSlateMaterialResource()
{
	if (SlateProxy)
	{
		delete SlateProxy;
	}
}

void FSlateMaterialResource::UpdateMaterial(const UMaterialInterface& InMaterialResource, const FVector2f InImageSize, FSlateShaderResource* InTextureMask)
{
#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	SlateMaterialResource::CheckInvalidUMaterial(InMaterialResource, DebugName);

	MaterialObject = &InMaterialResource;
	MaterialProxy = InMaterialResource.GetRenderProxy();

	MaterialObjectWeakPtr = MaterialObject;
	UpdateMaterialName();

	SlateMaterialResource::CheckInvalidMaterialProxy(MaterialProxy, DebugName);

#else

	MaterialObject = &InMaterialResource;
	MaterialProxy = InMaterialResource.GetRenderProxy();
#endif

	if (MaterialProxy && (MaterialProxy->IsDeleted() || MaterialProxy->IsMarkedForGarbageCollection()))
	{
		MaterialProxy = nullptr;
	}

	if (!SlateProxy)
	{
		SlateProxy = new FSlateShaderResourceProxy;
	}

	TextureMaskResource = InTextureMask;

	SlateProxy->ActualSize = InImageSize.IntPoint();
	SlateProxy->Resource = this;

	Width = FMath::RoundToInt(InImageSize.X);
	Height = FMath::RoundToInt(InImageSize.Y);
}

void FSlateMaterialResource::ResetMaterial()
{
	MaterialObject = nullptr;
	MaterialProxy = nullptr;

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	MaterialObjectWeakPtr = nullptr;
	UpdateMaterialName();
#endif

	TextureMaskResource = nullptr;
	if (SlateProxy)
	{
		delete SlateProxy;
	}
	SlateProxy = nullptr;
	Width = 0;
	Height = 0;
}

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
void FSlateMaterialResource::UpdateMaterialName()
{
	const UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialObject);
	if(MID && MID->Parent)
	{
		// MID's don't have nice names. Get the name of the parent instead for tracking
		DebugName = MID->Parent->GetFName();
	}
	else if(MaterialObject)
	{
		DebugName = MaterialObject->GetFName();
	}
	else
	{
		DebugName = NAME_None;
	}
}

void FSlateMaterialResource::CheckForStaleResources() const
{
	if (DebugName != NAME_None)
	{
		// pending kill objects may still be rendered for a frame so it is valid for the check to pass
		const bool bEvenIfPendingKill = true;
		// This test needs to be thread safe.  It doesn't give us as many chances to trap bugs here but it is still useful
		const bool bThreadSafe = true;
		checkf(MaterialObjectWeakPtr.IsValid(bEvenIfPendingKill, bThreadSafe), TEXT("Material %s has become invalid.  This means the resource was garbage collected while slate was using it"), *DebugName.ToString());
	}
}

#endif
