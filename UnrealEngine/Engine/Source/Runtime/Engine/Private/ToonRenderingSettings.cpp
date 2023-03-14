// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/ToonRenderingSettings.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"

/** The editor object. */
extern UNREALED_API class UEditorEngine* GEditor;
#endif // #if WITH_EDITOR


FRuntimeAtlasTexture::FRuntimeAtlasTexture()
	: CurveLinearColorAtlas(nullptr)
{
	
}

UTexture2D* FRuntimeAtlasTexture::GetAtlasTexture2D() const 
{
	if(CurveLinearColorAtlas != nullptr)
		return CurveLinearColorAtlas;
	else
		return nullptr;
}

const UTexture2D* FRuntimeAtlasTexture::GetAtlasTexture2DConst() const
{
	if(CurveLinearColorAtlas != nullptr)
	{
		return CurveLinearColorAtlas;
	}
	else
	{
		return nullptr;
	}
}

UToonRenderingSettings::UToonRenderingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitToonData();
}

void UToonRenderingSettings::InitToonData() const
{
	if(RampTexture.GetAtlasTexture2DConst())
		GEngine->ToonRampTexture = RampTexture.CurveLinearColorAtlas;

	UE_LOG(LogTexture, Error, TEXT("UToonRenderingSettings::InitToonData()"));
}

void UToonRenderingSettings::PostInitProperties()
{
	Super::PostInitProperties();


#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
	
}

#if WITH_EDITOR
void UToonRenderingSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	
}

void UToonRenderingSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UToonRenderingSettings, RampTexture))
		{
			GEngine->ToonRampTexture = RampTexture.CurveLinearColorAtlas;
			UE_LOG(LogTexture, Warning, TEXT("UToonRenderingSettings::RampTexture changed."));
		}

		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UToonRenderingSettings, OutlineZOffsetCurve))
		{
			UE_LOG(LogTexture, Warning, TEXT("UToonRenderingSettings::OutlineZOffsetCurve changed."));
		}
	}
}

bool UToonRenderingSettings::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	return true;
}
#endif // #if WITH_EDITOR
