// Copyright Epic Games, Inc. All Rights Reserved.
#include "IAudioModulation.h"

#include "Containers/Map.h"
#include "UObject/Object.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IAudioModulation)


namespace Audio
{
	namespace ModulationInterfacePrivate
	{
		class FModulationParameterRegistry
		{
			TMap<FName, FModulationParameter> Values;

			mutable FCriticalSection ThreadSafeValueAccessor;

		public:
			bool IsRegistered(FName InName) const
			{
				FScopeLock Lock(&ThreadSafeValueAccessor);
				return Values.Contains(InName);
			}

			void Register(FName InName, FModulationParameter&& InParameter)
			{
				FScopeLock Lock(&ThreadSafeValueAccessor);
				if (FModulationParameter* Value = Values.Find(InName))
				{
					*Value = MoveTemp(InParameter);
				}
				else
				{
					Values.Add(InName, MoveTemp(InParameter));
				}
			}

			bool Unregister(FName InName)
			{
				FScopeLock Lock(&ThreadSafeValueAccessor);
				return Values.Remove(InName) > 0;
			}

			void UnregisterAll()
			{
				FScopeLock Lock(&ThreadSafeValueAccessor);
				Values.Reset();
			}

			const FModulationParameter& Get(FName InName) const
			{
				FScopeLock Lock(&ThreadSafeValueAccessor);

				static const FModulationParameter DefaultParameter { };
				if (const FModulationParameter* Param = Values.Find(InName))
				{
					return *Param;
				}

				return DefaultParameter;
			}
		} ParameterRegistry;
	}

	FModulatorHandleId CreateModulatorHandleId()
	{
		static FModulatorHandleId NextHandleId = INDEX_NONE;
		return ++NextHandleId;
	}

	FModulationParameter::FModulationParameter()
		: MixFunction(GetDefaultMixFunction())
		, UnitFunction(GetDefaultUnitConversionFunction())
		, NormalizedFunction(GetDefaultNormalizedConversionFunction())
	{
	}

	FModulationParameter::FModulationParameter(FModulationParameter&& InParam)
		: ParameterName(MoveTemp(InParam.ParameterName))
		, DefaultValue(InParam.DefaultValue)
		, MinValue(InParam.MinValue)
		, MaxValue(InParam.MaxValue)
		, bRequiresConversion(InParam.bRequiresConversion)
	#if WITH_EDITORONLY_DATA
		, UnitDisplayName(MoveTemp(InParam.UnitDisplayName))
		, ClassName(MoveTemp(InParam.ClassName))
	#endif // WITH_EDITORONLY_DATA
		, MixFunction(MoveTemp(InParam.MixFunction))
		, UnitFunction(MoveTemp(InParam.UnitFunction))
		, NormalizedFunction(MoveTemp(InParam.NormalizedFunction))
	{
	}

	FModulationParameter::FModulationParameter(const FModulationParameter& InParam)
		: ParameterName(InParam.ParameterName)
		, DefaultValue(InParam.DefaultValue)
		, MinValue(InParam.MinValue)
		, MaxValue(InParam.MaxValue)
		, bRequiresConversion(InParam.bRequiresConversion)
#if WITH_EDITORONLY_DATA
		, UnitDisplayName(InParam.UnitDisplayName)
		, ClassName(InParam.ClassName)
#endif // WITH_EDITORONLY_DATA
		, MixFunction(InParam.MixFunction)
		, UnitFunction(InParam.UnitFunction)
		, NormalizedFunction(InParam.NormalizedFunction)
	{
	}

	FModulationParameter& FModulationParameter::operator=(const FModulationParameter& InParam)
	{
		ParameterName = InParam.ParameterName;
		DefaultValue = InParam.DefaultValue;
		MinValue = InParam.MinValue;
		MaxValue = InParam.MaxValue;
		bRequiresConversion = InParam.bRequiresConversion;

#if WITH_EDITORONLY_DATA
		UnitDisplayName = InParam.UnitDisplayName;
		ClassName = InParam.ClassName;
#endif // WITH_EDITORONLY_DATA

		MixFunction = InParam.MixFunction;
		UnitFunction = InParam.UnitFunction;
		NormalizedFunction = InParam.NormalizedFunction;

		return *this;
	}

	FModulationParameter& FModulationParameter::operator=(FModulationParameter&& InParam)
	{
		ParameterName = MoveTemp(InParam.ParameterName);
		DefaultValue = InParam.DefaultValue;
		MinValue = InParam.MinValue;
		MaxValue = InParam.MaxValue;
		bRequiresConversion = InParam.bRequiresConversion;

	#if WITH_EDITORONLY_DATA
		UnitDisplayName = MoveTemp(InParam.UnitDisplayName);
		ClassName = MoveTemp(InParam.ClassName);
	#endif // WITH_EDITORONLY_DATA

		MixFunction = MoveTemp(InParam.MixFunction);
		UnitFunction = MoveTemp(InParam.UnitFunction);
		NormalizedFunction = MoveTemp(InParam.NormalizedFunction);

		return *this;
	}

	const FModulationMixFunction& FModulationParameter::GetDefaultMixFunction()
	{
		static const FModulationMixFunction DefaultMixFunction = [](float& InOutValueA, float InValueB)
		{
			InOutValueA *= InValueB;
		};

		return DefaultMixFunction;
	}

	const FModulationUnitConversionFunction& FModulationParameter::GetDefaultUnitConversionFunction()
	{
		static const FModulationUnitConversionFunction ConversionFunction = [](float& InOutValue)
		{
		};

		return ConversionFunction;
	};

	const FModulationNormalizedConversionFunction& FModulationParameter::GetDefaultNormalizedConversionFunction()
	{
		static const FModulationNormalizedConversionFunction ConversionFunction = [](float& InOutValue)
		{
		};

		return ConversionFunction;
	};

	void RegisterModulationParameter(FName InName, FModulationParameter&& InParameter)
	{
		using namespace ModulationInterfacePrivate;
		ParameterRegistry.Register(InName, MoveTemp(InParameter));
	}

	bool UnregisterModulationParameter(FName InName)
	{
		using namespace ModulationInterfacePrivate;
		return ParameterRegistry.Unregister(InName);
	}

	void UnregisterAllModulationParameters()
	{
		using namespace ModulationInterfacePrivate;
		ParameterRegistry.UnregisterAll();
	}

	bool IsModulationParameterRegistered(FName InName)
	{
		using namespace ModulationInterfacePrivate;
		return ParameterRegistry.IsRegistered(InName);
	}

	const FModulationParameter& GetModulationParameter(FName InName)
	{
		using namespace ModulationInterfacePrivate;
		return ParameterRegistry.Get(InName);
	}

	FModulatorHandle::FModulatorHandle(Audio::FModulationParameter&& InParameter)
		: Parameter(InParameter)
		, HandleId(CreateModulatorHandleId())
	{
	}

	FModulatorHandle::FModulatorHandle(IAudioModulationManager& InModulation, const Audio::IModulatorSettings& InModulatorSettings, Audio::FModulationParameter&& InParameter)
		: Parameter(MoveTemp(InParameter))
		, HandleId(CreateModulatorHandleId())
		, Modulation(InModulation.AsShared())
	{
		ModulatorTypeId = InModulatorSettings.Register(HandleId, InModulation);
		if (ModulatorTypeId != INDEX_NONE)
		{
			ModulatorId = InModulatorSettings.GetModulatorId();
		}
	}

	FModulatorHandle::FModulatorHandle(const FModulatorHandle& InOther)
	{
		HandleId = CreateModulatorHandleId();

		if (TSharedPtr<IAudioModulationManager> ModPtr = InOther.Modulation.Pin())
		{
			ModPtr->RegisterModulator(HandleId, InOther.ModulatorId);
			Parameter = InOther.Parameter;
			ModulatorId = InOther.ModulatorId;
			ModulatorTypeId = InOther.ModulatorTypeId;
			Modulation = InOther.Modulation;
		}
	}

	FModulatorHandle::FModulatorHandle(FModulatorHandle&& InOther)
		: Parameter(MoveTemp(InOther.Parameter))
		, HandleId(InOther.HandleId)
		, ModulatorTypeId(InOther.ModulatorTypeId)
		, ModulatorId(InOther.ModulatorId)
		, Modulation(InOther.Modulation)
	{
		// Move does not register as presumed already activated or
		// copying default handle, which is invalid. Removes data
		// from handle being moved to avoid double deactivation on
		// destruction.
		InOther.Parameter = FModulationParameter();
		InOther.HandleId = INDEX_NONE;
		InOther.ModulatorTypeId = INDEX_NONE;
		InOther.ModulatorId = INDEX_NONE;
		InOther.Modulation.Reset();
	}

	FModulatorHandle::~FModulatorHandle()
	{
		if (TSharedPtr<IAudioModulationManager> ModPtr = Modulation.Pin())
		{
			ModPtr->UnregisterModulator(*this);
		}
	}

	FModulatorHandle& FModulatorHandle::operator=(const FModulatorHandle& InOther)
	{
		Parameter = InOther.Parameter;

		if (TSharedPtr<IAudioModulationManager> ModPtr = InOther.Modulation.Pin())
		{
			HandleId = CreateModulatorHandleId();
			ModulatorId = InOther.ModulatorId;
			ModulatorTypeId = InOther.ModulatorTypeId;
			Modulation = InOther.Modulation;

			if (ModulatorId != INDEX_NONE)
			{
				ModPtr->RegisterModulator(HandleId, ModulatorId);
			}
		}
		else
		{
			HandleId = INDEX_NONE;
			ModulatorId = INDEX_NONE;
			ModulatorTypeId = INDEX_NONE;
			Modulation.Reset();
		}

		return *this;
	}

	FModulatorHandle& FModulatorHandle::operator=(FModulatorHandle&& InOther)
	{
		if (HandleId != INDEX_NONE)
		{
			if (TSharedPtr<IAudioModulationManager> ModPtr = Modulation.Pin())
			{
				ModPtr->UnregisterModulator(*this);
			}
		}

		// Move does not activate as presumed already activated or
		// copying default handle, which is invalid. Removes data
		// from handle being moved to avoid double deactivation on
		// destruction.
		Parameter = MoveTemp(InOther.Parameter);
		HandleId = InOther.HandleId;
		ModulatorId = InOther.ModulatorId;
		ModulatorTypeId = InOther.ModulatorTypeId;
		Modulation = InOther.Modulation;

		InOther.Parameter = FModulationParameter();
		InOther.HandleId = INDEX_NONE;
		InOther.ModulatorId = INDEX_NONE;
		InOther.ModulatorTypeId = INDEX_NONE;
		InOther.Modulation.Reset();

		return *this;
	}

	FModulatorId FModulatorHandle::GetModulatorId() const
	{
		return ModulatorId;
	}

	const FModulationParameter& FModulatorHandle::GetParameter() const
	{
		return Parameter;
	}

	FModulatorTypeId FModulatorHandle::GetTypeId() const
	{
		return ModulatorTypeId;
	}

	uint32 FModulatorHandle::GetHandleId() const
	{
		return HandleId;
	}

	bool FModulatorHandle::GetValue(float& OutValue) const
	{
		check(IsValid());

		OutValue = 1.0f;

		if (TSharedPtr<IAudioModulationManager> ModPtr = Modulation.Pin())
		{
			return ModPtr->GetModulatorValue(*this, OutValue);
		}

		return false;
	}

	bool FModulatorHandle::GetValueThreadSafe(float& OutValue) const
	{
		check(IsValid());

		OutValue = 1.0f;
		if (TSharedPtr<IAudioModulationManager> ModPtr = Modulation.Pin())
		{
			return ModPtr->GetModulatorValueThreadSafe(*this, OutValue);
		}

		return false;
	}

	bool FModulatorHandle::IsValid() const
	{
		return ModulatorId != INDEX_NONE;
	}
} // namespace Audio

const Audio::FModulationParameter& USoundModulatorBase::GetOutputParameter() const
{
	return Audio::GetModulationParameter({ });
}

TSharedPtr<Audio::IProxyData> USoundModulatorBase::CreateProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	// This should never be hit as all instances of modulators should implement their own version of the proxy data interface.
	checkNoEntry();
	return TSharedPtr<Audio::IProxyData>();
}

TUniquePtr<Audio::IModulatorSettings> USoundModulatorBase::CreateProxySettings() const
{
	checkNoEntry();
	return TUniquePtr<Audio::IModulatorSettings>();
}

