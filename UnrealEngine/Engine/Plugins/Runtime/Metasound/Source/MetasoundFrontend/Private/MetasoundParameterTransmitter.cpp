// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundParameterTransmitter.h"

#include "IAudioParameterInterfaceRegistry.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundLog.h"


namespace Metasound
{
	int32 MetaSoundParameterEnableWarningOnIgnoredParameterCVar = 0;

	FAutoConsoleVariableRef CVarMetaSoundParameterEnableWarningOnIgnoredParameter(
		TEXT("au.MetaSound.Parameter.EnableWarningOnIgnoredParameter"),
		MetaSoundParameterEnableWarningOnIgnoredParameterCVar,
		TEXT("If enabled, a warning will be logged when a parameters sent to a MetaSound is ignored.\n")
		TEXT("0: Disabled (default), !0: Enabled"),
		ECVF_Default);

	namespace Frontend
	{
		FLiteral ConvertParameterToLiteral(FAudioParameter&& InValue)
		{
			switch (InValue.ParamType)
			{
				case EAudioParameterType::Boolean:
				{
					return FLiteral(InValue.BoolParam);
				}

				case EAudioParameterType::BooleanArray:
				{
					return FLiteral(MoveTemp(InValue.ArrayBoolParam));
				}

				case EAudioParameterType::Float:
				{
					return FLiteral(InValue.FloatParam);
				}

				case EAudioParameterType::FloatArray:
				{
					return FLiteral(MoveTemp(InValue.ArrayFloatParam));
				}

				case EAudioParameterType::Integer:
				{
					return FLiteral(InValue.IntParam);
				}

				case EAudioParameterType::IntegerArray:
				{
					return FLiteral(MoveTemp(InValue.ArrayIntParam));
				}

				case EAudioParameterType::None:
				{
					return FLiteral();
				}

				case EAudioParameterType::NoneArray:
				{
					TArray<FLiteral::FNone> InitArray;
					InitArray.Init(FLiteral::FNone(), InValue.IntParam);
					return FLiteral(MoveTemp(InitArray));
				}

				case EAudioParameterType::Object:
				{
					if (InValue.ObjectProxies.IsEmpty())
					{
						return FLiteral();
					}

					return FLiteral(MoveTemp(InValue.ObjectProxies[0]));
				}

				case EAudioParameterType::ObjectArray:
				{
					return FLiteral(MoveTemp(InValue.ObjectProxies));
				}

				case EAudioParameterType::String:
				{
					return FLiteral(MoveTemp(InValue.StringParam));
				}

				case EAudioParameterType::StringArray:
				{
					return FLiteral(MoveTemp(InValue.ArrayStringParam));
				}

				default:
				{
					static_assert(static_cast<int32>(EAudioParameterType::COUNT) == 12, "Possible missing switch case coverage");
					checkNoEntry();
				}
			}

			return FLiteral();
		}

		FLiteral ConvertParameterToLiteral(const FAudioParameter& InValue)
		{
			switch (InValue.ParamType)
			{
				case EAudioParameterType::Boolean:
				{
					return FLiteral(InValue.BoolParam);
				}

				case EAudioParameterType::BooleanArray:
				{
					return FLiteral(InValue.ArrayBoolParam);
				}

				case EAudioParameterType::Float:
				{
					return FLiteral(InValue.FloatParam);
				}

				case EAudioParameterType::FloatArray:
				{
					return FLiteral(InValue.ArrayFloatParam);
				}

				case EAudioParameterType::Integer:
				{
					return FLiteral(InValue.IntParam);
				}

				case EAudioParameterType::IntegerArray:
				{
					return FLiteral(InValue.ArrayIntParam);
				}

				case EAudioParameterType::None:
				{
					return FLiteral();
				}

				case EAudioParameterType::NoneArray:
				{
					TArray<FLiteral::FNone> InitArray;
					InitArray.Init(FLiteral::FNone(), InValue.IntParam);
					return FLiteral(InitArray);
				}

				case EAudioParameterType::Object:
				{
					if (InValue.ObjectProxies.IsEmpty())
					{
						return FLiteral();
					}

					Audio::IProxyDataPtr ObjectProxyClone = InValue.ObjectProxies.Last()->Clone();
					return FLiteral(MoveTemp(ObjectProxyClone));
				}

				case EAudioParameterType::ObjectArray:
				{
					TArray<Audio::IProxyDataPtr> ObjectProxiesClone;
					Algo::Transform(InValue.ObjectProxies, ObjectProxiesClone, [](const Audio::IProxyDataPtr& DataPtr)
					{
						return DataPtr->Clone();
					});
					return FLiteral(MoveTemp(ObjectProxiesClone));
				}

				case EAudioParameterType::String:
				{
					return FLiteral(InValue.StringParam);
				}

				case EAudioParameterType::StringArray:
				{
					return FLiteral(InValue.ArrayStringParam);
				}

				default:
				{
					static_assert(static_cast<int32>(EAudioParameterType::COUNT) == 12, "Possible missing switch case coverage");
					checkNoEntry();
				}
			}

			return FLiteral();
		}

		FName ConvertParameterToDataType(EAudioParameterType InParameterType)
		{
			switch (InParameterType)
			{
				case EAudioParameterType::Boolean:
					return GetMetasoundDataTypeName<bool>();
				case EAudioParameterType::BooleanArray:
					return GetMetasoundDataTypeName<TArray<bool>>();
				case EAudioParameterType::Float:
					return GetMetasoundDataTypeName<float>();
				case EAudioParameterType::FloatArray:
					return GetMetasoundDataTypeName<TArray<float>>();
				case EAudioParameterType::Integer:
					return GetMetasoundDataTypeName<int32>();
				case EAudioParameterType::IntegerArray:
					return GetMetasoundDataTypeName<TArray<int32>>();
				case EAudioParameterType::String:
					return GetMetasoundDataTypeName<FString>();
				case EAudioParameterType::StringArray:
					return GetMetasoundDataTypeName<TArray<FString>>();

				case EAudioParameterType::Object:
				case EAudioParameterType::ObjectArray:
					// TODO: Add support for objects

				case EAudioParameterType::None:
				case EAudioParameterType::NoneArray:
				default:
					ensureAlwaysMsgf(false, TEXT("Failed to convert AudioParameterType to POD MetaSound DataType"));
					static_assert(static_cast<int32>(EAudioParameterType::COUNT) == 12, "Possible missing case coverage");
					return FName();
			}
		}
	} // namespace Frontend

	FSendAddress FMetaSoundParameterTransmitter::CreateSendAddressFromEnvironment(const FMetasoundEnvironment& InEnvironment, const FVertexName& InVertexName, const FName& InTypeName)
	{
		using namespace Frontend;

		uint64 InstanceID = -1;
		if (ensure(InEnvironment.Contains<uint64>(SourceInterface::Environment::TransmitterID)))
		{
			InstanceID = InEnvironment.GetValue<uint64>(SourceInterface::Environment::TransmitterID);
		}

		return CreateSendAddressFromInstanceID(InstanceID, InVertexName, InTypeName);
	}

	FSendAddress FMetaSoundParameterTransmitter::CreateSendAddressFromInstanceID(uint64 InInstanceID, const FVertexName& InVertexName, const FName& InTypeName)
	{
		return FSendAddress(InVertexName, InTypeName, InInstanceID);
	}

	FMetaSoundParameterTransmitter::FMetaSoundParameterTransmitter(FMetaSoundParameterTransmitter::FInitParams&& InInitParams)
		: Audio::FParameterTransmitterBase(MoveTemp(InInitParams.DefaultParams))
		, OperatorSettings(MoveTemp(InInitParams.OperatorSettings))
		, InstanceID(InInitParams.InstanceID)
		, DebugMetaSoundName(InInitParams.DebugMetaSoundName)
		, SendInfos(MoveTemp(InInitParams.Infos))
	{
	}

	bool FMetaSoundParameterTransmitter::Reset()
	{
		bool bSuccess = true;

		for (const FSendInfo& SendInfo : SendInfos)
		{
			if (InputSends.Remove(SendInfo.ParameterName))
			{
				// Only unregister the data channel if we had a sender using that 
				// data channel. This protects against removing the data channel 
				// multiple times. Multiple removals of data channels has caused
				// race conditions between newly created transmitters and transmitters
				// being cleaned up.
				bSuccess &= FDataTransmissionCenter::Get().UnregisterDataChannel(SendInfo.Address);
			}
		}

		bSuccess &= Audio::FParameterTransmitterBase::Reset();

		return bSuccess;
	}

	bool FMetaSoundParameterTransmitter::SetParameters(TArray<FAudioParameter>&& InParameters)
	{
		bool bSuccess = true;

		TArray<FAudioParameter> NewParams;
		for (FAudioParameter& Param : InParameters)
		{
			const FName ParamName = Param.ParamName;
			if (SetParameterWithLiteral(ParamName, Frontend::ConvertParameterToLiteral(Param)))
			{
				NewParams.Add(MoveTemp(Param));
			}
			else
			{
				bSuccess = false;
			}
		}

		if (!NewParams.IsEmpty())
		{
			bSuccess &= FParameterTransmitterBase::SetParameters(MoveTemp(NewParams));
		}

		InParameters.Reset();
		return bSuccess;
	}

	bool FMetaSoundParameterTransmitter::SetParameterWithLiteral(FName InParameterName, const FLiteral& InLiteral)
	{
		if (ISender* Sender = FindSender(InParameterName))
		{
			return Sender->PushLiteral(InLiteral);
		}

		// If no sender exists for parameter name, attempt to add one.
		if (const FSendInfo* SendInfo = FindSendInfo(InParameterName))
		{
			if (ISender* Sender = AddSender(*SendInfo))
			{
				return Sender->PushLiteral(InLiteral);
			}
		}

		// Enable / disable via CVAR to avoid log spam.
		if (MetaSoundParameterEnableWarningOnIgnoredParameterCVar)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Failed to set parameter %s in asset %s on instance %d with value %s. No runtime modifiable input with that name exists on instance."), *InParameterName.ToString(), *DebugMetaSoundName.ToString(), InstanceID, *LexToString(InLiteral));
		}

		return false;
	}

	const FMetaSoundParameterTransmitter::FSendInfo* FMetaSoundParameterTransmitter::FindSendInfo(const FName& InParameterName) const
	{
		return SendInfos.FindByPredicate([&](const FSendInfo& Info) { return Info.ParameterName == InParameterName; });
	}

	ISender* FMetaSoundParameterTransmitter::FindSender(const FName& InParameterName)
	{
		if (TUniquePtr<ISender>* SenderPtrPtr = InputSends.Find(InParameterName))
		{
			return SenderPtrPtr->Get();
		}
		return nullptr;
	}

	ISender* FMetaSoundParameterTransmitter::AddSender(const FSendInfo& InInfo)
	{
		// TODO: likely want to remove this and opt for different protocols having different behaviors.
		const float DelayTimeInSeconds = 0.1f; // This not used for non-audio routing.
		const FSenderInitParams InitParams = { OperatorSettings, DelayTimeInSeconds };

		TUniquePtr<ISender> Sender = FDataTransmissionCenter::Get().RegisterNewSender(InInfo.Address, InitParams);
		if (ensureMsgf(Sender.IsValid(), TEXT("Failed to create sender [Address:%s]"), *InInfo.Address.ToString()))
		{
			ISender* Ptr = Sender.Get();
			InputSends.Add(InInfo.ParameterName, MoveTemp(Sender));
			return Ptr;
		}

		return nullptr;
	}
}
