// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

#include "CoreMinimal.h"
#include "OpenColorIOColorSpace.h"

class FOpenColorIODisplayExtension;
class FViewportClient;

class OPENCOLORIO_API FOpenColorIODisplayManager : public TSharedFromThis<FOpenColorIODisplayManager>
{
public:

	/** Will return the configuration associated to the desired viewport or create one if it's not tracked */
	FOpenColorIODisplayConfiguration& FindOrAddDisplayConfiguration(FViewportClient* InViewportClient);

	/** Returns the configuration for a given viewport if it was found, nullptr otherwise */
	const FOpenColorIODisplayConfiguration* GetDisplayConfiguration(const FViewportClient* InViewportClient) const;

	/** Remove display configuration associated with this viewport */
	bool RemoveDisplayConfiguration(const FViewportClient* InViewportClient);

	/** Whether or not InViewport has a display configuration linked to it */
	bool IsTrackingViewport(const FViewportClient* InViewportClient) const;

protected:

	/** List of DisplayExtension created when a viewport is asked to be tracked. It contains the configuration. */
	TArray<TSharedPtr<FOpenColorIODisplayExtension, ESPMode::ThreadSafe>> DisplayExtensions;
};

