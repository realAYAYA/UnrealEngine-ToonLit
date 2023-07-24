// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkMediaPlayerFactoryNew.h"


UBinkMediaPlayerFactoryNew::UBinkMediaPlayerFactoryNew( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SupportedClass = UBinkMediaPlayer::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}
