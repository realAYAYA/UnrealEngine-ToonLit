// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaDelegates.h"
#include "MediaUtilsPrivate.h"

#if UE_MEDIAUTILS_DEVELOPMENT_DELEGATE

FMediaDelegates::FOnSampleDiscardedDelegate FMediaDelegates::OnSampleDiscarded_RenderThread;
FMediaDelegates::FOnPreSampleRenderDelegate FMediaDelegates::OnPreSampleRender_RenderThread;

#endif //UE_MEDIAUTILS_DEVELOPMENT_DELEGATE
