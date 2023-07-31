// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AppleARKitAvailability.h"

#if SUPPORTS_ARKIT_3_0
#include <ARKit/ARKit.h>

@interface FARKitCoachingOverlay : NSObject<ARCoachingOverlayViewDelegate>
{
}

- (id)initWithAppleARKitSystem:(class FAppleARKitSystem*)InAppleARKitSystem;

- (void)setARSession:(ARSession*)InARSession;

- (void)addToRootView;
@end

#endif
