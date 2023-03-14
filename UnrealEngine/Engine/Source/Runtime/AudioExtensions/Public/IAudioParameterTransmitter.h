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
	/** Data passed to CreateParameterTransmitter. */
	struct AUDIOEXTENSIONS_API FParameterTransmitterInitParams
	{
		// Unique ID for this audio instance.
		uint64 InstanceID = INDEX_NONE;

		// Audio sample rate.
		float SampleRate = 0.0f;

		TArray<FAudioParameter> DefaultParams;
	};

	// Reference collector functionality for legacy parameter system
	// (i.e. backwards compatibility with the SoundCue system). None of this
	// should be used by future assets supporting parameters (ex. MetaSounds)
	// as object pointers within parameters should NOT be cached on threads
	// other than the GameThread, utilizing a proxy methodology like MetaSounds
	// that copies UObject data when and where necessary.
	class AUDIOEXTENSIONS_API ILegacyParameterTransmitter
	{
		public:
			virtual ~ILegacyParameterTransmitter() = default;

			virtual TArray<UObject*> GetReferencedObjects() const;
	};

	/** Interface for a audio instance transmitter.
	 *
	 * An audio instance transmitter ushers control parameters to a single audio object instance.
	 */
	class AUDIOEXTENSIONS_API IParameterTransmitter : public ILegacyParameterTransmitter
	{
		public:
			static const FName RouterName;

			virtual ~IParameterTransmitter() = default;

			virtual bool Reset() = 0;

			// Return the cached parameter with the given name if it exists
			// @return False if param not found, true if found.
			virtual bool GetParameter(FName InName, FAudioParameter& OutParam) const = 0;

			// Return reference to the cached parameter array.
			virtual const TArray<FAudioParameter>& GetParameters() const = 0;

			// Parameter Setters
			virtual bool SetParameters(TArray<FAudioParameter>&& InParameters) = 0;
	};

	/** Base implementation for the parameter transmitter, which caches parameters
	  * and provides implementer to add additional logic to route parameter data accordingly.
	  */
	class AUDIOEXTENSIONS_API FParameterTransmitterBase : public IParameterTransmitter
	{
	public:
		FParameterTransmitterBase(TArray<FAudioParameter>&& InDefaultParams);
		virtual ~FParameterTransmitterBase() = default;

		virtual bool GetParameter(FName InName, FAudioParameter& OutParam) const override;
		virtual const TArray<FAudioParameter>& GetParameters() const override;
		virtual bool Reset() override;
		virtual bool SetParameters(TArray<FAudioParameter>&& InParameters) override;

	protected:
		TArray<FAudioParameter> AudioParameters;
	};
} // namespace Audio
