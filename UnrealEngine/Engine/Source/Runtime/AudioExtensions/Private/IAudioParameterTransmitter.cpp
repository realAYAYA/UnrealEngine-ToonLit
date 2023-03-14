// Copyright Epic Games, Inc. All Rights Reserved.
#include "IAudioParameterTransmitter.h"
#include "UObject/Object.h"


namespace Audio
{
	const FName IParameterTransmitter::RouterName = "ParameterTransmitter";

	TArray<UObject*> ILegacyParameterTransmitter::GetReferencedObjects() const
	{
		return { };
	}

	FParameterTransmitterBase::FParameterTransmitterBase(TArray<FAudioParameter>&& InDefaultParams)
		: AudioParameters(MoveTemp(InDefaultParams))
	{
	}

	bool FParameterTransmitterBase::GetParameter(FName InName, FAudioParameter& OutValue) const
	{
		if (const FAudioParameter* Param = FAudioParameter::FindParam(AudioParameters, InName))
		{
			OutValue = *Param;
			return true;
		}

		return false;
	}

	const TArray<FAudioParameter>& FParameterTransmitterBase::GetParameters() const
	{
		return AudioParameters;
	}

	bool FParameterTransmitterBase::Reset()
	{
		AudioParameters.Reset();
		return true;
	}

	bool FParameterTransmitterBase::SetParameters(TArray<FAudioParameter>&& InParameters)
	{
		FAudioParameter::Merge(MoveTemp(InParameters), AudioParameters);
		return true;
	}
} // namespace Audio