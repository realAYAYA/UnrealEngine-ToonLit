// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioParameter.h"
#include "AudioParameterControllerInterface.h"
#include "Containers/Array.h"
#include "CoreTypes.h"
#include "IAudioProxyInitializer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

// Forward Declarations
class UObject;

namespace Audio
{
	using DeviceID = uint32;

	/** Data passed to CreateParameterTransmitter. */
	struct FParameterTransmitterInitParams
	{
		// Unique ID for this audio instance.
		uint64 InstanceID = INDEX_NONE;

		// Audio sample rate.
		float SampleRate = 0.0f;

		TArray<FAudioParameter> DefaultParams;

		// Audio device ID
		DeviceID AudioDeviceID = INDEX_NONE;

	};

	// Reference collector functionality for legacy parameter system
	// (i.e. backwards compatibility with the SoundCue system). None of this
	// should be used by future assets supporting parameters (ex. MetaSounds)
	// as object pointers within parameters should NOT be cached on threads
	// other than the GameThread, utilizing a proxy methodology like MetaSounds
	// that copies UObject data when and where necessary.
	class ILegacyParameterTransmitter
	{
		public:
			virtual ~ILegacyParameterTransmitter() = default;

			AUDIOEXTENSIONS_API virtual TArray<const TObjectPtr<UObject>*> GetReferencedObjects() const;
	};

	/** Interface for a audio instance transmitter.
	 *
	 * An audio instance transmitter ushers control parameters to a single audio object instance.
	 */
	class IParameterTransmitter : public ILegacyParameterTransmitter
	{
		public:
			static AUDIOEXTENSIONS_API const FName RouterName;

			virtual ~IParameterTransmitter() = default;

			UE_DEPRECATED(5.2, "Use ResetParameters() or OnDeleteActiveSound() instead depending on use case.")
			virtual bool Reset() { ResetParameters(); return true; }

			// Reset parameters which stored on the transmitter.
			virtual void ResetParameters() {}

			// Called when the active sound is deleted due to the sound finishing,
			// being stopped, or being virtualized. 
			virtual void OnDeleteActiveSound() {}

			// Return the cached parameter with the given name if it exists
			// @return False if param not found, true if found.
			virtual bool GetParameter(FName InName, FAudioParameter& OutParam) const = 0;

			// Return reference to the cached parameter array.
			virtual const TArray<FAudioParameter>& GetParameters() const = 0;

			// Parameter Setters
			virtual bool SetParameters(TArray<FAudioParameter>&& InParameters) = 0;

			// Called when the active sound is virtualized  
			virtual void OnVirtualizeActiveSound() {}

			// Called when the virtualized active sound is realized 
			// @param Parameters to set 
			virtual void OnRealizeVirtualizedActiveSound(TArray<FAudioParameter>&& InParameters) {}
	};

	/** Base implementation for the parameter transmitter, which caches parameters
	  * and provides implementer to add additional logic to route parameter data accordingly.
	  */
	class FParameterTransmitterBase : public IParameterTransmitter
	{
	public:
		AUDIOEXTENSIONS_API FParameterTransmitterBase(TArray<FAudioParameter>&& InDefaultParams);
		virtual ~FParameterTransmitterBase() = default;

		AUDIOEXTENSIONS_API virtual bool GetParameter(FName InName, FAudioParameter& OutParam) const override;
		AUDIOEXTENSIONS_API virtual void ResetParameters() override;
		AUDIOEXTENSIONS_API virtual const TArray<FAudioParameter>& GetParameters() const override;
		UE_DEPRECATED(5.2, "Use ResetParameters() or OnDeleteActiveSound() instead depending on use case.")
		AUDIOEXTENSIONS_API virtual bool Reset() override;
		AUDIOEXTENSIONS_API virtual bool SetParameters(TArray<FAudioParameter>&& InParameters) override;
		AUDIOEXTENSIONS_API virtual void OnVirtualizeActiveSound() override;
		AUDIOEXTENSIONS_API virtual void OnRealizeVirtualizedActiveSound(TArray<FAudioParameter>&& InParameters);

	protected:
		TArray<FAudioParameter> AudioParameters;
		bool bIsVirtualized;
	};
} // namespace Audio
