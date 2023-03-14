// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkMediaPlayerFactory.h"

#include "BinkMediaPlayerEditorPrivate.h"

UBinkMediaPlayerFactory::UBinkMediaPlayerFactory( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SupportedClass = UBinkMediaPlayer::StaticClass();

	bCreateNew = false;
	bEditorImport = true;

	Formats.Add(TEXT("bk2;Bink 2 Movie File"));
}

UObject* UBinkMediaPlayerFactory::FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn ) 
{
	UBinkMediaPlayer* MediaPlayer = NewObject<UBinkMediaPlayer>(InParent, Class, Name, Flags);
	MediaPlayer->OpenUrl(CurrentFilename);
	return MediaPlayer;
}
