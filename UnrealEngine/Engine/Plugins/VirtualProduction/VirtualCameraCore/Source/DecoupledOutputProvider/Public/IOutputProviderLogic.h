// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Output/VCamOutputProviderBase.h"

class FReferenceCollector;
class UDecoupledOutputProvider;
class UVCamOutputProviderBase;
#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

namespace UE::DecoupledOutputProvider
{
	/** Exposes certain data and manages the call to the super function. */
	class DECOUPLEDOUTPUTPROVIDER_API IOutputProviderEvent
	{
	public:
		
		/**
		 * Calls the super UVCamOutputProviderBase function version.
		 * If this is not called, the call to super takes place automatically after the IOutputProviderLogic function call.
		 */
		virtual void ExecuteSuperFunction() = 0;

		/** Gets the associated output provider */
		virtual UDecoupledOutputProvider& GetOutputProvider() const = 0;

		virtual ~IOutputProviderEvent() = default;
	};
	
	/**
	 * Receives events from an output provider's and implements its logic.
	 * 
	 * This pattern decouples the output provider's data from its logic allowing the data to be loaded
	 * on all platforms but only have the logic be loaded on supported platforms.
	 *
	 * A logic's lifetime is bound to that of the output provider and is managed by the DecoupledOutputProvider's module.
	 * You can register a factory function with IDecoupledOutputProviderModule::RegisterLogicFactory.
	 */
	class DECOUPLEDOUTPUTPROVIDER_API IOutputProviderLogic : public TSharedFromThis<IOutputProviderLogic>
	{
	public:

		// UVCamOutputProviderBase events
		virtual void OnInitialize(IOutputProviderEvent& Args) {}
		virtual void OnDeinitialize(IOutputProviderEvent& Args) {}
		virtual void OnActivate(IOutputProviderEvent& Args) {}
		virtual void OnDeactivate(IOutputProviderEvent& Args) {}
		virtual void OnTick(IOutputProviderEvent& Args, const float DeltaTime) {}
		virtual VCamCore::EViewportChangeReply PreReapplyViewport(IOutputProviderEvent& Args) { return VCamCore::EViewportChangeReply::Reinitialize; }
		virtual void PostReapplyViewport(IOutputProviderEvent& Args) {}

		// UObject events
		virtual void OnAddReferencedObjects(IOutputProviderEvent& Args, FReferenceCollector& Collector) {}
		virtual void OnBeginDestroy(IOutputProviderEvent& Args) {}
		virtual void OnSerialize(IOutputProviderEvent& Args, FArchive& Ar) {}
		virtual void OnPostLoad(IOutputProviderEvent& Args) {}
#if WITH_EDITOR
		virtual void OnPostEditChangeProperty(IOutputProviderEvent& Args, FPropertyChangedEvent& PropertyChangedEvent) {}
#endif
		
		virtual ~IOutputProviderLogic() = default;
	};
}