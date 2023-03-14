// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkMediaTextureFactoryNew.h"

#include "BinkMediaPlayerEditorPrivate.h"

UBinkMediaTextureFactoryNew::UBinkMediaTextureFactoryNew( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SupportedClass = UBinkMediaTexture::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UBinkMediaTextureFactoryNew::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn ) 
{
	UBinkMediaTexture* MediaTexture = NewObject<UBinkMediaTexture>(InParent, InClass, InName, Flags);
	if (MediaTexture && InitialMediaPlayer) 
	{
		MediaTexture->SetMediaPlayer(InitialMediaPlayer);
	}
	return MediaTexture;
}
