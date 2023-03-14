// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLMediaSourceFactory.h"

#include "HLMediaSource.h"

#include "AssetTypeCategories.h"

/* UHLMediaSourceFactory 
 *****************************************************************************/
UHLMediaSourceFactory::UHLMediaSourceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("3g2;3G2 Multimedia Stream"));
	Formats.Add(TEXT("3gp;3GP Video Stream"));
	Formats.Add(TEXT("3gp2;3GPP2 Multimedia File"));
	Formats.Add(TEXT("3gpp;3GPP Multimedia File"));
	Formats.Add(TEXT("aac;MPEG-2 Advanced Audio Coding File"));
	Formats.Add(TEXT("adts;Audio Data Transport Stream"));
	Formats.Add(TEXT("asf;ASF Media File"));
	Formats.Add(TEXT("avi;Audio Video Interleave File"));
	Formats.Add(TEXT("m4a;Apple MPEG-4 Audio"));
	Formats.Add(TEXT("m4v;Apple MPEG-4 Video"));
	Formats.Add(TEXT("mov;Apple QuickTime Movie"));
	Formats.Add(TEXT("mp3;MPEG-2 Audio"));
	Formats.Add(TEXT("mp4;MPEG-4 Movie"));
	Formats.Add(TEXT("sami;Synchronized Accessible Media Interchange (SAMI) File"));
	Formats.Add(TEXT("smi;Synchronized Multimedia Integration (SMIL) File"));
	Formats.Add(TEXT("wav;Wave Audio File"));
	Formats.Add(TEXT("wma;Windows Media Audio"));
	Formats.Add(TEXT("wmv;Windows Media Video"));

	SupportedClass = UHLMediaSource::StaticClass();
	bEditorImport = true;
}

/* UFactory overrides
 *****************************************************************************/
UObject* UHLMediaSourceFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UHLMediaSource>(InParent, InClass, InName, Flags);
}

uint32 UHLMediaSourceFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}

bool UHLMediaSourceFactory::ShouldShowInNewMenu() const
{
	return true;
}
