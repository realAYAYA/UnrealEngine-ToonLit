// Copyright Epic Games, Inc. All Rights Reserved.

#include "GMEResourceItemViewModel.h"

#include "Engine/CanvasRenderTarget2D.h"
#include "GeometryMaskCanvasResource.h"

TSharedRef<FGMEResourceItemViewModel> FGMEResourceItemViewModel::Create(
	const TWeakObjectPtr<const UGeometryMaskCanvasResource>& InResource)
{
	TSharedRef<FGMEResourceItemViewModel> ViewModel = MakeShared<FGMEResourceItemViewModel>(FPrivateToken{}, InResource);

	return ViewModel;
}

FGMEResourceItemViewModel::FGMEResourceItemViewModel(
	FPrivateToken,
	const TWeakObjectPtr<const UGeometryMaskCanvasResource>& InResource)
	: ResourceWeak(InResource)
{
	if (const UGeometryMaskCanvasResource* Resource = InResource.Get())
	{
		UniqueId = Resource->GetUniqueID();
		ResourceTextureWeak = const_cast<UGeometryMaskCanvasResource*>(Resource)->GetRenderTargetTexture();
		UpdateInfoText();
	}
}

float FGMEResourceItemViewModel::GetMemoryUsage() const
{
	if (const UCanvasRenderTarget2D* Texture = GetResourceTexture())
	{
		return (static_cast<float>(Texture->CalcTextureMemorySizeEnum(TMC_ResidentMips)) / (1024.0f * 1024.0f));
	}
	
	return 0.0f;
}

FIntPoint FGMEResourceItemViewModel::GetDimensions() const
{
	if (const UCanvasRenderTarget2D* Texture = GetResourceTexture())
	{
		return {Texture->SizeX, Texture->SizeY};
	}

	return {0, 0};
}

void FGMEResourceItemViewModel::UpdateInfoText()
{
	if (const UGeometryMaskCanvasResource* Resource = ResourceWeak.Get())
	{
		FString UsedChannelsList = FString::JoinBy(Resource->GetDependentCanvasIds(), TEXT("\n"), [](const FGeometryMaskCanvasId& InCanvasId)
		{
			return TEXT("\t") + InCanvasId.ToString();
		});
		
		InfoText = FText::FromString(
    		FString::Printf(TEXT("%-18s: %s\n%-18s: %s\n%-18s: %u of 3\n%s"),
    			TEXT("Name"), *Resource->GetName(),
    			TEXT("Size"), *GetDimensions().ToString(),
    			TEXT("Num. Used Channels"), Resource->GetNumChannelsUsed(),
    			*UsedChannelsList));
	}
}

bool FGMEResourceItemViewModel::Tick(const float InDeltaSeconds)
{
	UpdateInfoText();
	return true;
}

bool FGMEResourceItemViewModel::GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren)
{
	return false;
}

const FText& FGMEResourceItemViewModel::GetResourceInfo() const
{
	return InfoText;
}
