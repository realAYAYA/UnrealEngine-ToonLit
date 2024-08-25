// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/LogInformationUtil.h"

#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "TextureResource.h"
#include "MuCO/CustomizableObject.h"

int LogInformationUtil::CountLOD0 = 0;
int LogInformationUtil::CountLOD1 = 0;
int LogInformationUtil::CountLOD2 = 0;

void LogInformationUtil::PrintGeneratedTextures(const TArray<FGeneratedTexture> GeneratedTextures, FString& Log, bool DoPrintInitialMessage)
{
	if (GeneratedTextures.Num() == 0)
	{
		return;
	}

	if (DoPrintInitialMessage)
	{
		Log += "Generated textures: ";
	}

	const int Max = GeneratedTextures.Num();
	int i;

	bool UseTabulation = (Max > 1);

	if (!DoPrintInitialMessage)
	{
		Log += "\n\t\t";
	}
	

	if (Max > 1)
	{
		UE_LOG(LogMutable, Log, TEXT("Case"));
	}

	for (i = 0; i < Max; ++i)
	{
		Log += FString::Printf(TEXT("Texture name=%s, "), *GeneratedTextures[i].Name);
		Log += FString::Printf(TEXT("id=%d, "), GeneratedTextures[i].Key.Resource);
		Log += "\n\t\t\t";
		PrintTextureData(Cast<UTexture2D>(GeneratedTextures[i].Texture), Log, false);
		if (i < (Max - 1))
		{
			Log += "\n\t\t";
		}
	}
}


void LogInformationUtil::PrintGeneratedMaterial(const TArray<struct FGeneratedMaterial> GeneratedMaterials, FString& Log)
{
	if (GeneratedMaterials.Num() == 0)
	{
		return;
	}

	Log += "\tGenerated materials:\n";

	FString MessageChunk = "\t\t";

	const int Max = GeneratedMaterials.Num();
	int i;
	int entryLength = 25;
	bool AnyHasTextures = false;

	MessageChunk += "Materials with no textures: ";

	// First only no texture materials
	for (i = 0; i < Max; ++i)
	{
		if (GeneratedMaterials[i].Textures.Num() == 0)
		{
			MessageChunk += FString::Printf(TEXT("%d, "), i);
		}
		else
		{
			AnyHasTextures = true;
		}
	}

	// Second texture materials
	for (i = 0; i < Max; ++i)
	{
		if (GeneratedMaterials[i].Textures.Num() > 0)
		{
			MessageChunk += "\n\t\t";
			MessageChunk += FString::Printf(TEXT("%d"), i);
			MessageChunk += FString::Printf(TEXT(" has %d texture(s)"), GeneratedMaterials[i].Textures.Num());
			PrintGeneratedTextures(GeneratedMaterials[i].Textures, MessageChunk, false);
		}
	}

	Log += MessageChunk;
}


void LogInformationUtil::LogShowInstanceData(const UCustomizableObjectInstance* CustomizableObjectInstance, FString& LogData)
{
	LogData += FString::Printf(TEXT("Name=%s "), *CustomizableObjectInstance->GetName());
	LogData += FString::Printf(TEXT("Priority=%f "), FMath::Sqrt(CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer));

	const uint32 InstanceFlags = CustomizableObjectInstance->GetPrivate()->GetCOInstanceFlags();
	LogData += FString::Printf(TEXT("bShouldUpdateLODs=%d "), InstanceFlags & ECOInstanceFlags::PendingLODsUpdate);

	LogData += FString::Printf(TEXT("CurrentMinLOD=%d "), CustomizableObjectInstance->GetCurrentMinLOD());
	LogData += FString::Printf(TEXT("MinLODToLoad=%d "), CustomizableObjectInstance->GetMinLODToLoad());
}


// TODO: [Acosin] put in a different file and include it
void LogInformationUtil::LogShowInstanceDataFull(const UCustomizableObjectInstance* CustomizableObjectInstance, bool ShowMaterialInfo)
{
	FString LogData = "\n\n";

	LogData += FString::Printf(TEXT("CustomizableObjectInstance %s, "), *CustomizableObjectInstance->GetName());

	LogData += "Descriptor:\n";
	LogData += CustomizableObjectInstance->GetPrivate()->CommittedDescriptor.ToString();
	LogData += "\n";
	
	if (ShowMaterialInfo)
	{
		PrintGeneratedMaterial(CustomizableObjectInstance->GetPrivate()->GeneratedMaterials, LogData);

		if (CustomizableObjectInstance->GetPrivate()->GeneratedTextures.Num() > 0)
		{
			LogData += "\n\t";
		}

		PrintGeneratedTextures(CustomizableObjectInstance->GetPrivate()->GeneratedTextures, LogData, true);
	}

	FString MessageChunk;

	MessageChunk += "\n\t";
	MessageChunk += FString::Printf(TEXT("        bShowOnlyRuntimeParameters = %d\n"), CustomizableObjectInstance->GetPrivate()->bShowOnlyRuntimeParameters);
	MessageChunk += FString::Printf(TEXT("        bShowOnlyRelevantParameters = %d\n"), CustomizableObjectInstance->GetPrivate()->bShowOnlyRelevantParameters);
	MessageChunk += FString::Printf(TEXT("        MinSquareDistFromComponentToPlayer = %.2f\n"), CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
	LogData += MessageChunk;

	const uint32 InstanceFlags = CustomizableObjectInstance->GetPrivate()->GetCOInstanceFlags();
	MessageChunk = FString::Printf(TEXT("        bIsBeingUsedByComponent = %d\n"), InstanceFlags & ECOInstanceFlags::UsedByComponent);
	MessageChunk += FString::Printf(TEXT("        bIsBeingUsedByComponentInPlay = %d\n"), InstanceFlags & ECOInstanceFlags::UsedByComponentInPlay);
	MessageChunk += FString::Printf(TEXT("        bIsPlayerOrNearIt = %d\n"), InstanceFlags & ECOInstanceFlags::UsedByPlayerOrNearIt);
	MessageChunk += FString::Printf(TEXT("        bIsDiscardedByNumGeneratedInstancesLimit = %d\n"), InstanceFlags & ECOInstanceFlags::DiscardedByNumInstancesLimit);
	MessageChunk += FString::Printf(TEXT("        LastMinSquareDistFromComponentToPlayer = %.2f\n"), CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer);
	LogData += MessageChunk;

	MessageChunk = FString::Printf(TEXT("        CurrentMinLOD = %d\n"), CustomizableObjectInstance->GetCurrentMinLOD());
	MessageChunk += FString::Printf(TEXT("        MinLODToLoad = %d\n"), CustomizableObjectInstance->GetMinLODToLoad());
	MessageChunk += FString::Printf(TEXT("        bShouldUpdateLODs = %d\n"), InstanceFlags & ECOInstanceFlags::PendingLODsUpdate);
	LogData += MessageChunk;

	MessageChunk += FString::Printf(TEXT("        bIsDowngradeLODUpdate = %d\n"), InstanceFlags & ECOInstanceFlags::PendingLODsDowngrade);
	LogData += MessageChunk;

	for (int32 ComponentIndex = 0; ComponentIndex < CustomizableObjectInstance->GetNumComponents(); ++ComponentIndex)
	{
		if (CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex))
		{
			if (CustomizableObjectInstance->GetCurrentMinLOD() < 1)
			{
				CountLOD0++;
			}
			else if (CustomizableObjectInstance->GetCurrentMinLOD() < 2)
			{
				CountLOD1++;
			}
			else
			{
				CountLOD2++;
			}
		}
	}

	UE_LOG(LogMutable, Log, TEXT("%s"), *LogData);

	UWorld* World = GWorld;

	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			PlayerController->ClientMessage(LogData);
		}
	}
}


void LogInformationUtil::PrintTextureData(const UTexture2D* Texture, FString& Log, bool UseTabulation)
{
	if (!Texture)
	{
		return;
	}

	FString MessageChunk;
	MessageChunk += FString::Printf(TEXT("        Name=%s\n"), *Texture->GetName());
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 37);
	}
	MessageChunk += FString::Printf(TEXT("                w=%d\n"), int32(Texture->GetSurfaceWidth()));
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 65);
	}
	MessageChunk += FString::Printf(TEXT("                h=%d\n"), int32(Texture->GetSurfaceHeight()));
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 95);
	}
	MessageChunk += FString::Printf(TEXT("                bForceMiplevelsToBeResident=%d\n"), Texture->bForceMiplevelsToBeResident);
	if (UseTabulation)
	{
		MessageChunk += "\n\t\t\t";
	}
	MessageChunk += FString::Printf(TEXT("                bGlobalForceMipLevelsToBeResident=%d\n"), Texture->bGlobalForceMipLevelsToBeResident);
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 37);
	}
	MessageChunk += FString::Printf(TEXT("                bHasStreamingUpdatePending=%d\n"), Texture->bHasStreamingUpdatePending);
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 65);
	}
	MessageChunk += FString::Printf(TEXT("                bIgnoreStreamingMipBias=%d\n"), Texture->bIgnoreStreamingMipBias);
	if (UseTabulation)
	{
		MessageChunk += "\n\t\t\t";
	}
	MessageChunk += FString::Printf(TEXT("                FirstResourceMemMip=%d\n"), Texture->FirstResourceMemMip);
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 37);
	}
	MessageChunk += FString::Printf(TEXT("                LevelIndex=%d\n"), Texture->LevelIndex);
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 65);
	}
	MessageChunk += FString::Printf(TEXT("                CompressionSettings=%s\n"), *UEnum::GetValueAsString(Texture->CompressionSettings));
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 95);
	}
	MessageChunk += FString::Printf(TEXT("                Filter=%s\n"), *UEnum::GetValueAsString(Texture->Filter));
	if (UseTabulation)
	{
		FillToLength(MessageChunk, 128);
	}
	MessageChunk += FString::Printf(TEXT("                LODGroup=%s\n"), *UEnum::GetValueAsString(Texture->LODGroup));

	Log += MessageChunk;
}


void LogInformationUtil::FillToLength(FString& Data, int32 NewLength)
{
	int32 i;
	const int32 Max = NewLength - Data.Len();

	for (i = 0; i < Max; ++i)
	{
		Data += " ";
	}
}


void LogInformationUtil::ResetCounters()
{
	CountLOD0 = 0;
	CountLOD1 = 0;
	CountLOD2 = 0;
}

void LogInformationUtil::PrintImageToPlatformDataMap(const TMap<uint32, FTexturePlatformData*>& ImageToPlatformDataMap, FString& Log)
{
	if (ImageToPlatformDataMap.Num() == 0)
	{
		return;
	}

	Log += "\n\tImageToPlatformDataMap:";
	for (const TPair<uint32, FTexturePlatformData*>& Elem : ImageToPlatformDataMap)
	{
		Log += "\n\t\t";
		Log = FString::Printf(TEXT("key=%d, "), Elem.Key);
		Log = FString::Printf(TEXT("value: SizeX=%d, "), Elem.Value->SizeX);
		Log = FString::Printf(TEXT("SizeY=%d, "), Elem.Value->SizeY);
		Log = FString::Printf(TEXT("PixelFormat=%d, "), *UEnum::GetValueAsString(Elem.Value->PixelFormat));
		int i;
		const int Max = Elem.Value->Mips.Num();
		for (i = 0; i < Max; ++i)
		{
			Log += "\n\t\t\t";
			Log = FString::Printf(TEXT("Mips[%d] "), i);
			Log = FString::Printf(TEXT("SizeX=%d, "), Elem.Value->Mips[i].SizeX);
			Log = FString::Printf(TEXT("SizeY=%d"), Elem.Value->Mips[i].SizeY);
		}
	}
}
