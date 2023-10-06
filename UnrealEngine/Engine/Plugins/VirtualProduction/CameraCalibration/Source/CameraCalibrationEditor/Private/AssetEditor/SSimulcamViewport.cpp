// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimulcamViewport.h"
#include "Engine/Texture.h"
#include "Viewport/SSimulcamEditorViewport.h"


void SSimulcamViewport::Construct(const FArguments& InArgs, UTexture* InTexture)
{
	OnSimulcamViewportClicked = InArgs._OnSimulcamViewportClicked;
	OnSimulcamViewportInputKey = InArgs._OnSimulcamViewportInputKey;

	Texture = TStrongObjectPtr<UTexture>(InTexture);

	TextureViewport = SNew(SSimulcamEditorViewport, SharedThis(this), InArgs._WithZoom.Get(), InArgs._WithPan.Get());

	ChildSlot
		[
			TextureViewport.ToSharedRef()
		];
}

UTexture* SSimulcamViewport::GetTexture() const
{
	return Texture.Get();
}

bool SSimulcamViewport::HasValidTextureResource() const
{
	UTexture* CurrentTexture = GetTexture();
	return CurrentTexture != nullptr && CurrentTexture->GetResource() != nullptr;
}

bool SSimulcamViewport::OnViewportInputKey(const FKey& Key, const EInputEvent& Event)
{ 
	if (OnSimulcamViewportInputKey.IsBound())
	{
		return OnSimulcamViewportInputKey.Execute(Key, Event);
	}
	return false;
}
