// Copyright Epic Games, Inc. All Rights Reserved.

#include "Action/Bind/RCCustomBindActionUtilities.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Texture2D.h"
#include "IRemoteControlPropertyHandle.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/Paths.h"
#include "RemoteControlField.h"

UTexture2D* UE::RCCustomBindActionUtilities::LoadTextureFromPath(const FString& InPath)
{
	if (UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *InPath)))
	{
		return Texture;
	}

	if (FPaths::FileExists(InPath))
	{	
		return UKismetRenderingLibrary::ImportFileAsTexture2D(nullptr, InPath);
	}

	return nullptr;
}

void UE::RCCustomBindActionUtilities::SetTexturePropertyFromPath(const TSharedRef<FRemoteControlProperty>& InRemoteControlEntityAsProperty, const FString& InPath)
{
	if (UTexture2D* LoadedTexture = UE::RCCustomBindActionUtilities::LoadTextureFromPath(InPath))
	{
		const FProperty* RemoteControlProperty = InRemoteControlEntityAsProperty->GetProperty();
		if (RemoteControlProperty == nullptr)
		{
			return;
		}

		const TSharedPtr<IRemoteControlPropertyHandle>& RemoteControlHandle = InRemoteControlEntityAsProperty->GetPropertyHandle();
		if (!ensure(RemoteControlHandle))
		{
			return;
		}
			
		RemoteControlHandle->SetValue(LoadedTexture);
	}
}
