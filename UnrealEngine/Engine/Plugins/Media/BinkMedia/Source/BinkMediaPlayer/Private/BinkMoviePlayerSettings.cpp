// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkMoviePlayerSettings.h"
#include "BinkMediaPlayerPrivate.h"

UBinkMoviePlayerSettings::UBinkMoviePlayerSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, BinkBufferMode(MP_Bink_Stream)
	, BinkSoundTrack(MP_Bink_Sound_Simple)
	, BinkSoundTrackStart(0)
	, BinkDestinationUpperLeft(0,0)
	, BinkDestinationLowerRight(1,1)
	, BinkPixelFormat(PF_B8G8R8A8)
{
}
