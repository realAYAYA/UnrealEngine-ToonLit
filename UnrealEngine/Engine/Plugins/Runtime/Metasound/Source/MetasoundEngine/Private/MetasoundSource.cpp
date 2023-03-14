// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundSource.h"

#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioDeviceManager.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "Interfaces/MetasoundFrontendOutputFormatInterfaces.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Internationalization/Text.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "MetasoundAudioFormats.h"
#include "MetasoundEngineArchetypes.h"
#include "MetasoundEngineAsset.h"
#include "MetasoundEngineEnvironment.h"
#include "MetasoundEnvironment.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundGenerator.h"
#include "MetasoundLog.h"
#include "MetasoundOperatorBuilderSettings.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundPrimitives.h"
#include "MetasoundReceiveNode.h"
#include "MetasoundSettings.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundUObjectRegistry.h"
#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundSource)

#if WITH_EDITORONLY_DATA
#include "EdGraph/EdGraph.h"
#endif // WITH_EDITORONLY_DATA

#define LOCTEXT_NAMESPACE "MetaSound"

namespace Metasound
{
	namespace SourcePrivate
	{
		// Contains information on output audio formats
		struct FOutputFormatInfo
		{
			FMetasoundFrontendVersion InterfaceVersion;
			TArray<Metasound::FVertexName> OutputVertexChannelOrder;
		};

		using FFormatInfoMap = TMap<EMetasoundSourceAudioFormat, FOutputFormatInfo>;
		using FFormatInfoPair = TPair<EMetasoundSourceAudioFormat, FOutputFormatInfo>;

		// Return a map containing all the supported audio formats for a MetaSound
		// Source. 
		const FFormatInfoMap& GetFormatInfoMap()
		{
			auto CreateFormatInfoMap = []()
			{
				using namespace Metasound::Frontend;

				return FFormatInfoMap
				{
					{
						EMetasoundSourceAudioFormat::Mono,
						{
							OutputFormatMonoInterface::GetVersion(),
							{
								OutputFormatMonoInterface::Outputs::MonoOut
							}
						}
					},
					{
						EMetasoundSourceAudioFormat::Stereo,
						{
							OutputFormatStereoInterface::GetVersion(),
							{
								OutputFormatStereoInterface::Outputs::LeftOut,
								OutputFormatStereoInterface::Outputs::RightOut
							}
						}
					},
					{
						EMetasoundSourceAudioFormat::Quad,
						{
							OutputFormatQuadInterface::GetVersion(),
							{
								OutputFormatQuadInterface::Outputs::FrontLeftOut,
								OutputFormatQuadInterface::Outputs::FrontRightOut,
								OutputFormatQuadInterface::Outputs::SideLeftOut,
								OutputFormatQuadInterface::Outputs::SideRightOut
							}
						}
					},
					{
						EMetasoundSourceAudioFormat::FiveDotOne,
						{
							OutputFormatFiveDotOneInterface::GetVersion(),
							{
								OutputFormatFiveDotOneInterface::Outputs::FrontLeftOut,
								OutputFormatFiveDotOneInterface::Outputs::FrontRightOut,
								OutputFormatFiveDotOneInterface::Outputs::FrontCenterOut,
								OutputFormatFiveDotOneInterface::Outputs::LowFrequencyOut,
								OutputFormatFiveDotOneInterface::Outputs::SideLeftOut,
								OutputFormatFiveDotOneInterface::Outputs::SideRightOut
							}
						}
					},
					{
						EMetasoundSourceAudioFormat::SevenDotOne,
						{
							OutputFormatSevenDotOneInterface::GetVersion(),
							{
								OutputFormatSevenDotOneInterface::Outputs::FrontLeftOut,
								OutputFormatSevenDotOneInterface::Outputs::FrontRightOut,
								OutputFormatSevenDotOneInterface::Outputs::FrontCenterOut,
								OutputFormatSevenDotOneInterface::Outputs::LowFrequencyOut,
								OutputFormatSevenDotOneInterface::Outputs::SideLeftOut,
								OutputFormatSevenDotOneInterface::Outputs::SideRightOut,
								OutputFormatSevenDotOneInterface::Outputs::BackLeftOut,
								OutputFormatSevenDotOneInterface::Outputs::BackRightOut
							}
						}
					}
				};
			};

			static const FFormatInfoMap Map = CreateFormatInfoMap();
			return Map;
		}
		
		Frontend::FMetaSoundAssetRegistrationOptions GetInitRegistrationOptions()
		{
			Frontend::FMetaSoundAssetRegistrationOptions RegOptions;
			RegOptions.bForceReregister = false;
			if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
			{
				RegOptions.bAutoUpdateLogWarningOnDroppedConnection = Settings->bAutoUpdateLogWarningOnDroppedConnection;
			}

			return RegOptions;
		}

		// Return an array of all the audio format versions.
		const TArray<FMetasoundFrontendVersion>& GetFormatInterfaceVersions()
		{
			auto CreateFormatInterfaceVersions = []() -> TArray<FMetasoundFrontendVersion>
			{
				TArray<FMetasoundFrontendVersion> FormatVersions;
				const FFormatInfoMap& FormatMap = GetFormatInfoMap();
				for (const auto& Pair : FormatMap)
				{
					FormatVersions.Add(Pair.Value.InterfaceVersion);
				}
				return FormatVersions;
			};

			static const TArray<FMetasoundFrontendVersion> Versions = CreateFormatInterfaceVersions();
			return Versions;
		}

	} // namespace SourcePrivate
} // namespace Metasound


UMetaSoundSource::UMetaSoundSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FMetasoundAssetBase()
{
	bRequiresStopFade = true;
	NumChannels = 1;

	// todo: ensure that we have a method so that the audio engine can be authoritative over the sample rate the UMetaSoundSource runs at.
	SampleRate = 48000.f;
}

#if WITH_EDITOR
void UMetaSoundSource::PostEditUndo()
{
	Super::PostEditUndo();
	Metasound::FMetaSoundEngineAssetHelper::PostEditUndo(*this);
}

void UMetaSoundSource::PostDuplicate(EDuplicateMode::Type InDuplicateMode)
{
	Super::PostDuplicate(InDuplicateMode);

	// Guid is reset as asset may share implementation from
	// asset duplicated from but should not be registered as such.
	if (InDuplicateMode == EDuplicateMode::Normal)
	{
		AssetClassID = FGuid::NewGuid();
		Metasound::Frontend::FRenameRootGraphClass::Generate(GetDocumentHandle(), AssetClassID);
	}
}

void UMetaSoundSource::PostEditChangeProperty(FPropertyChangedEvent& InEvent)
{
	Super::PostEditChangeProperty(InEvent);

	if (InEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, OutputFormat))
	{
		PostEditChangeOutputFormat();
	}
}

void UMetaSoundSource::PostEditChangeOutputFormat()
{
	using namespace Metasound::SourcePrivate;
	using namespace Metasound::Frontend;

	// If this is a preset, convert to normal metasound source since it is being
	// altered. 
	ConvertFromPreset();

	// Determine which interfaces to add and remove from the document due to the
	// output format being changed.
	TArray<FMetasoundFrontendVersion> OutputFormatsToAdd;
	if (const FOutputFormatInfo* FormatInfo = GetFormatInfoMap().Find(OutputFormat))
	{
		OutputFormatsToAdd.Add(FormatInfo->InterfaceVersion);
	}

	TArray<FMetasoundFrontendVersion> OutputFormatsToRemove;
	for (const FMetasoundFrontendVersion& FormatVersion : GetFormatInterfaceVersions())
	{
		if (RootMetasoundDocument.Interfaces.Contains(FormatVersion))
		{
			if (!OutputFormatsToAdd.Contains(FormatVersion))
			{
				OutputFormatsToRemove.Add(FormatVersion);
			}
		}
	}

	// Add and/or remove interfaces from the root document.
	const bool bDidModifyDocument = FModifyRootGraphInterfaces(OutputFormatsToRemove, OutputFormatsToAdd).Transform(GetDocumentHandle());

	if (bDidModifyDocument)
	{
		// Update the data in this UMetaSoundSource to reflect what is in the metasound document.
		ConformObjectDataToInterfaces();

		// Use the editor form of register to ensure other editors'
		// MetaSounds are auto-updated if they are referencing this graph.
		if (Graph)
		{
			Graph->RegisterGraphWithFrontend();
		}
		MarkMetasoundDocumentDirty();
	}
}
#endif // WITH_EDITOR

bool UMetaSoundSource::ConformObjectDataToInterfaces()
{
	using namespace Metasound::SourcePrivate;

	bool bDidAlterObjectData = false;

	// Update the OutputFormat and NumChannels to match the audio format interface
	// on the root document.
	const FFormatInfoMap& FormatInfo = GetFormatInfoMap();
	for (const FFormatInfoPair& Pair : FormatInfo)
	{
		if (RootMetasoundDocument.Interfaces.Contains(Pair.Value.InterfaceVersion))
		{
			if ((OutputFormat != Pair.Key) || (NumChannels != Pair.Value.OutputVertexChannelOrder.Num()))
			{
				OutputFormat = Pair.Key;
				NumChannels = Pair.Value.OutputVertexChannelOrder.Num();
				bDidAlterObjectData = true;
			}

			break;
		}
	}

	return bDidAlterObjectData;
}


void UMetaSoundSource::BeginDestroy()
{
	UnregisterGraphWithFrontend();
	Super::BeginDestroy();
}

void UMetaSoundSource::PreSave(FObjectPreSaveContext InSaveContext)
{
	Super::PreSave(InSaveContext);
	Metasound::FMetaSoundEngineAssetHelper::PreSaveAsset(*this, InSaveContext);
}

void UMetaSoundSource::Serialize(FArchive& InArchive)
{
	Super::Serialize(InArchive);
	Metasound::FMetaSoundEngineAssetHelper::SerializeToArchive(*this, InArchive);
}

#if WITH_EDITOR
void UMetaSoundSource::SetReferencedAssetClasses(TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo>&& InAssetClasses)
{
	Metasound::FMetaSoundEngineAssetHelper::SetReferencedAssetClasses(*this, MoveTemp(InAssetClasses));
}
#endif // WITH_EDITOR

TArray<FMetasoundAssetBase*> UMetaSoundSource::GetReferencedAssets()
{
	return Metasound::FMetaSoundEngineAssetHelper::GetReferencedAssets(*this);
}

const TSet<FSoftObjectPath>& UMetaSoundSource::GetAsyncReferencedAssetClassPaths() const 
{
	return ReferenceAssetClassCache;
}

void UMetaSoundSource::OnAsyncReferencedAssetsLoaded(const TArray<FMetasoundAssetBase*>& InAsyncReferences)
{
	Metasound::FMetaSoundEngineAssetHelper::OnAsyncReferencedAssetsLoaded(*this, InAsyncReferences);
}

#if WITH_EDITORONLY_DATA

UEdGraph* UMetaSoundSource::GetGraph()
{
	return Graph;
}

const UEdGraph* UMetaSoundSource::GetGraph() const
{
	return Graph;
}

UEdGraph& UMetaSoundSource::GetGraphChecked()
{
	check(Graph);
	return *Graph;
}

const UEdGraph& UMetaSoundSource::GetGraphChecked() const
{
	check(Graph);
	return *Graph;
}

FText UMetaSoundSource::GetDisplayName() const
{
	FString TypeName = UMetaSoundSource::StaticClass()->GetName();
	return FMetasoundAssetBase::GetDisplayName(MoveTemp(TypeName));
}


void UMetaSoundSource::SetRegistryAssetClassInfo(const Metasound::Frontend::FNodeClassInfo& InNodeInfo)
{
	Metasound::FMetaSoundEngineAssetHelper::SetMetaSoundRegistryAssetClassInfo(*this, InNodeInfo);
}
#endif // WITH_EDITORONLY_DATA

void UMetaSoundSource::PostLoad()
{
	Super::PostLoad();
	Metasound::FMetaSoundEngineAssetHelper::PostLoad(*this);

	Duration = GetDuration();
	bLooping = IsLooping();
}

void UMetaSoundSource::InitParameters(TArray<FAudioParameter>& ParametersToInit, FName InFeatureName)
{
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;

	// Have to call register vs a simple get as the source may have yet to start playing/has not been registered
	// via InitResources. If it has, this call is fast and returns the already cached RuntimeData.
	RegisterGraphWithFrontend(GetInitRegistrationOptions());
	const FRuntimeData& RuntimeData = GetRuntimeData();
	const TArray<FMetasoundFrontendClassInput>& PublicInputs = RuntimeData.PublicInputs;

	TMap<FName, FMetasoundFrontendVertex> PublicInputMap;
	Algo::Transform(PublicInputs, PublicInputMap, [](const FMetasoundFrontendClassInput& Input)
	{
		return TPair<FName, FMetasoundFrontendVertex>(Input.Name, Input);
	});

	// Removes values that are not explicitly defined by the ParamType and returns
	// whether or not the parameter is a valid input and should be included.
	auto Sanitize = [&PublicInputMap](FAudioParameter& Parameter) -> bool
	{
		const FMetasoundFrontendVertex* Input = PublicInputMap.Find(Parameter.ParamName);
		if (!Input)
		{
			return false;
		}

		const bool bIsMatchingType = Parameter.TypeName.IsNone() || (Parameter.TypeName == Input->TypeName);
		if (!bIsMatchingType)
		{
			return false;
		}

		switch (Parameter.ParamType)
		{
			case EAudioParameterType::Boolean:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.BoolParam);
			}
			break;

			case EAudioParameterType::BooleanArray:
			{
				TArray<bool> TempArray = Parameter.ArrayBoolParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;

			case EAudioParameterType::Float:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.FloatParam);
			}
			break;

			case EAudioParameterType::FloatArray:
			{
				TArray<float> TempArray = Parameter.ArrayFloatParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;

			case EAudioParameterType::Integer:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.IntParam);
			}
			break;

			case EAudioParameterType::IntegerArray:
			{
				TArray<int32> TempArray = Parameter.ArrayIntParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;

			case EAudioParameterType::Object:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.ObjectParam);
			}
			break;

			case EAudioParameterType::ObjectArray:
			{
				TArray<UObject*> TempArray = Parameter.ArrayObjectParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;


			case EAudioParameterType::String:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.StringParam);
			}
			break;

			case EAudioParameterType::StringArray:
			{
				TArray<FString> TempArray = Parameter.ArrayStringParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;

			case EAudioParameterType::None:
			default:
			break;
		}

		return true;
	};

	auto ConstructProxies = [this, FeatureName = InFeatureName](FAudioParameter& OutParamToInit)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;

		const Audio::FProxyDataInitParams ProxyInitParams { FeatureName };

		switch (OutParamToInit.ParamType)
		{
			case EAudioParameterType::Object:
			{
				FDataTypeRegistryInfo DataTypeInfo;
				if (IDataTypeRegistry::Get().GetDataTypeInfo(OutParamToInit.ObjectParam, DataTypeInfo))
				{
					Audio::IProxyDataPtr ProxyPtr = IDataTypeRegistry::Get().CreateProxyFromUObject(DataTypeInfo.DataTypeName, OutParamToInit.ObjectParam);
					OutParamToInit.ObjectProxies.Emplace(MoveTemp(ProxyPtr));

					// Null out param as it is no longer needed (nor desired to be accessed once passed to the Audio Thread)
					OutParamToInit.ObjectParam = nullptr;
				}
			}
			break;

			case EAudioParameterType::ObjectArray:
			{
				for (TObjectPtr<UObject>& Object : OutParamToInit.ArrayObjectParam)
				{
					FDataTypeRegistryInfo DataTypeInfo;
					if (IDataTypeRegistry::Get().GetDataTypeInfo(Object, DataTypeInfo))
					{
						Audio::IProxyDataPtr ProxyPtr = IDataTypeRegistry::Get().CreateProxyFromUObject(DataTypeInfo.DataTypeName, Object);
						OutParamToInit.ObjectProxies.Emplace(MoveTemp(ProxyPtr));
					}
				}
				// Reset param array as it is no longer needed (nor desired to be accessed once passed to the Audio Thread).
				// All object manipulation hereafter should be done via proxies
				OutParamToInit.ArrayObjectParam.Reset();
			}
			break;

			default:
				break;
		}
	};


	for (int32 i = ParametersToInit.Num() - 1; i >= 0; --i)
	{
		FAudioParameter& Parameter = ParametersToInit[i];
		
#if !NO_LOGGING
		// For logging in case of failure
		const FString AssetName = GetName();
#endif // !NO_LOGGING
		
		if (Sanitize(Parameter))
		{
			if (IsParameterValid(Parameter, PublicInputMap))
			{
				ConstructProxies(Parameter);
			}
			else
			{
#if !NO_LOGGING
				if (::Metasound::MetaSoundParameterEnableWarningOnIgnoredParameterCVar)
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Failed to set invalid parameter '%s' in asset '%s': Either does not exist or is unsupported type"), *Parameter.ParamName.ToString(), *AssetName);
				}
#endif // !NO_LOGGING
				constexpr bool bAllowShrinking = false;
				ParametersToInit.RemoveAtSwap(i, 1, bAllowShrinking);
			}
		}
		else
		{
			constexpr bool bAllowShrinking = false;
			ParametersToInit.RemoveAtSwap(i, 1, bAllowShrinking);

#if !NO_LOGGING
			if (::Metasound::MetaSoundParameterEnableWarningOnIgnoredParameterCVar)
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed to set parameter '%s' in asset '%s': No name specified, no transmittable input found, or type mismatch."), *Parameter.ParamName.ToString(), *AssetName);
			}
#endif // !NO_LOGGING
		}
	}
}

void UMetaSoundSource::InitResources()
{
	using namespace Metasound::Frontend;
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::InitResources);

	RegisterGraphWithFrontend(GetInitRegistrationOptions());
}

Metasound::Frontend::FNodeClassInfo UMetaSoundSource::GetAssetClassInfo() const
{
	return { GetDocumentChecked().RootGraph, FSoftObjectPath(this) };
}

bool UMetaSoundSource::IsPlayable() const
{
	// todo: cache off whether this metasound is buildable to an operator.
	return true;
}

bool UMetaSoundSource::SupportsSubtitles() const
{
	return Super::SupportsSubtitles();
}

float UMetaSoundSource::GetDuration() const
{
	// This is an unfortunate function required by logic in determining what sounds can be potentially
	// culled (in this case prematurally). MetaSound OneShots are stopped either by internally logic that
	// triggers OnFinished, or if an external system requests the sound to be stopped. Setting the duration
	// as a "close to" maximum length without being considered looping avoids the MetaSound from being
	// culled inappropriately.
	return IsOneShot() ? INDEFINITELY_LOOPING_DURATION - 1.0f : INDEFINITELY_LOOPING_DURATION;
}

bool UMetaSoundSource::ImplementsParameterInterface(Audio::FParameterInterfacePtr InInterface) const
{
	const FMetasoundFrontendVersion Version { InInterface->GetName(), { InInterface->GetVersion().Major, InInterface->GetVersion().Minor } };
	return GetDocumentChecked().Interfaces.Contains(Version);
}

ISoundGeneratorPtr UMetaSoundSource::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams, TArray<FAudioParameter>&& InDefaultParameters)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace Metasound::Engine;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::CreateSoundGenerator);

	SampleRate = InParams.SampleRate;
	FOperatorSettings InSettings = GetOperatorSettings(static_cast<FSampleRate>(SampleRate));
	FMetasoundEnvironment Environment = CreateEnvironment(InParams);

	TSharedPtr<const IGraph, ESPMode::ThreadSafe> MetasoundGraph = GetRuntimeData().Graph;
	if (!MetasoundGraph.IsValid())
	{
		return ISoundGeneratorPtr(nullptr);
	}

	FOperatorBuilderSettings BuilderSettings = FOperatorBuilderSettings::GetDefaultSettings();

	// Graph analyzer currently only enabled for preview sounds (but can theoretically be supported for all sounds)
	BuilderSettings.bPopulateInternalDataReferences = InParams.bIsPreviewSound;

	FMetasoundGeneratorInitParams InitParams =
	{
		InSettings,
		MoveTemp(BuilderSettings),
		MetasoundGraph,
		Environment,
		GetName(),
		GetOutputAudioChannelOrder(),
		MoveTemp(InDefaultParameters)
	};

	return ISoundGeneratorPtr(new FMetasoundGenerator(MoveTemp(InitParams)));
}

bool UMetaSoundSource::GetAllDefaultParameters(TArray<FAudioParameter>& OutParameters) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace Metasound::Engine;

	// TODO: Make this use the cached runtime data's input copy. This call can become expensive if called repeatedly.
	TArray<FMetasoundFrontendClassInput> PublicInputs = GetPublicClassInputs();
	for(const FMetasoundFrontendClassInput& Input : PublicInputs)
	{
		FAudioParameter Params;
		Params.ParamName = Input.Name;
		Params.TypeName = Input.TypeName;

		switch (Input.DefaultLiteral.GetType())
		{
			case EMetasoundFrontendLiteralType::Boolean:
			{
				Params.ParamType = EAudioParameterType::Boolean;
				ensure(Input.DefaultLiteral.TryGet(Params.BoolParam));
			}
			break;

			case EMetasoundFrontendLiteralType::BooleanArray:
			{
				Params.ParamType = EAudioParameterType::BooleanArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayBoolParam));
			}
			break;

			case EMetasoundFrontendLiteralType::Integer:
			{
				Params.ParamType = EAudioParameterType::Integer;
				ensure(Input.DefaultLiteral.TryGet(Params.IntParam));
			}
			break;

			case EMetasoundFrontendLiteralType::IntegerArray:
			{
				Params.ParamType = EAudioParameterType::IntegerArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayIntParam));
			}
			break;

			case EMetasoundFrontendLiteralType::Float:
			{
				Params.ParamType = EAudioParameterType::Float;
				ensure(Input.DefaultLiteral.TryGet(Params.FloatParam));
			}
			break;

			case EMetasoundFrontendLiteralType::FloatArray:
			{
				Params.ParamType = EAudioParameterType::FloatArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayFloatParam));
			}
			break;

			case EMetasoundFrontendLiteralType::String:
			{
				Params.ParamType = EAudioParameterType::String;
				ensure(Input.DefaultLiteral.TryGet(Params.StringParam));
			}
			break;

			case EMetasoundFrontendLiteralType::StringArray:
			{
				Params.ParamType = EAudioParameterType::StringArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayStringParam));
			}
			break;

			case EMetasoundFrontendLiteralType::UObject:
			{
				Params.ParamType = EAudioParameterType::Object;
				UObject* Object = nullptr;
				ensure(Input.DefaultLiteral.TryGet(Object));
				Params.ObjectParam = Object;
			}
			break;

			case EMetasoundFrontendLiteralType::UObjectArray:
			{
				Params.ParamType = EAudioParameterType::ObjectArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayObjectParam));
			}
			break;

			default:
			break;
		}

		if (Params.ParamType != EAudioParameterType::None)
		{
			OutParameters.Add(Params);
		}
	}
	return true;
}

bool UMetaSoundSource::IsParameterValid(const FAudioParameter& InParameter) const
{
	TMap<FName, FMetasoundFrontendVertex> InputNameTypeMap;
	Algo::Transform(GetDocumentChecked().RootGraph.Interface.Inputs, InputNameTypeMap, [] (const FMetasoundFrontendClassInput& Input)
	{
		return TPair<FName, FMetasoundFrontendVertex>(Input.Name, Input);
	});
	return IsParameterValid(InParameter, InputNameTypeMap);
}

bool UMetaSoundSource::IsParameterValid(const FAudioParameter& InParameter, const TMap<FName, FMetasoundFrontendVertex>& InInputNameVertexMap) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	if (InParameter.ParamName.IsNone())
	{
		return false;
	}

	const FMetasoundFrontendVertex* Vertex = InInputNameVertexMap.Find(InParameter.ParamName);
	if (!Vertex)
	{
		return false;
	}
	const FName& TypeName = Vertex->TypeName;

	bool bIsValid = false;
	switch (InParameter.ParamType)
	{
		case EAudioParameterType::Boolean:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsBoolParsable;
		}
		break;

		case EAudioParameterType::BooleanArray:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsBoolArrayParsable;
		}
		break;

		case EAudioParameterType::Float:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsFloatParsable;
		}
		break;

		case EAudioParameterType::FloatArray:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsFloatArrayParsable;
		}
		break;

		case EAudioParameterType::Integer:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsIntParsable;
		}
		break;

		case EAudioParameterType::IntegerArray:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsIntArrayParsable;

		}
		break;

		case EAudioParameterType::Object:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(InParameter.ObjectParam, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsProxyParsable;
			bIsValid &= DataTypeInfo.DataTypeName == TypeName;
		}
		break;

		case EAudioParameterType::ObjectArray:
		{
			bIsValid = true;

			const FName ElementTypeName = CreateElementTypeNameFromArrayTypeName(TypeName);
			for (UObject* Object : InParameter.ArrayObjectParam)
			{
				FDataTypeRegistryInfo DataTypeInfo;
				bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(Object, DataTypeInfo);
				bIsValid &= DataTypeInfo.bIsProxyParsable;
				bIsValid &= DataTypeInfo.DataTypeName == ElementTypeName;

				if (!bIsValid)
				{
					break;
				}
			}
		}
		break;

		case EAudioParameterType::String:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsStringParsable;
		}
		break;

		case EAudioParameterType::StringArray:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsStringArrayParsable;
		}
		break;

		case EAudioParameterType::NoneArray:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsDefaultArrayParsable;
		}
		case EAudioParameterType::None:
		default:
		{
			FDataTypeRegistryInfo DataTypeInfo;
			bIsValid = IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			bIsValid &= DataTypeInfo.bIsDefaultParsable;
		}
		break;
	}

	return bIsValid;
}

bool UMetaSoundSource::IsLooping() const
{
	return !IsOneShot();
}

bool UMetaSoundSource::IsOneShot() const
{
	using namespace Metasound::Frontend;

	// If the metasound source implements the one-shot interface, then it's a one-shot metasound
	return IsInterfaceDeclared(SourceOneShotInterface::GetVersion());
}

TSharedPtr<Audio::IParameterTransmitter> UMetaSoundSource::CreateParameterTransmitter(Audio::FParameterTransmitterInitParams&& InParams) const
{
	METASOUND_LLM_SCOPE;

	Metasound::FMetaSoundParameterTransmitter::FInitParams InitParams(GetOperatorSettings(InParams.SampleRate), InParams.InstanceID, MoveTemp(InParams.DefaultParams));
	InitParams.DebugMetaSoundName = GetFName();

	for (const FSendInfoAndVertexName& InfoAndName : FMetasoundAssetBase::GetSendInfos(InParams.InstanceID))
	{
		InitParams.Infos.Add(InfoAndName.SendInfo);
	}

	return MakeShared<Metasound::FMetaSoundParameterTransmitter>(MoveTemp(InitParams));
}

Metasound::FOperatorSettings UMetaSoundSource::GetOperatorSettings(Metasound::FSampleRate InSampleRate) const
{
	const float BlockRate = Metasound::Frontend::GetDefaultBlockRate();
	return Metasound::FOperatorSettings(InSampleRate, BlockRate);
}

Metasound::FMetasoundEnvironment UMetaSoundSource::CreateEnvironment() const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FMetasoundEnvironment Environment;
	Environment.SetValue<uint32>(SourceInterface::Environment::SoundUniqueID, GetUniqueID());

	return Environment;
}

Metasound::FMetasoundEnvironment UMetaSoundSource::CreateEnvironment(const FSoundGeneratorInitParams& InParams) const
{
	using namespace Metasound;
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	FMetasoundEnvironment Environment = CreateEnvironment();
	Environment.SetValue<bool>(SourceInterface::Environment::IsPreview, InParams.bIsPreviewSound);
	Environment.SetValue<uint64>(SourceInterface::Environment::TransmitterID, InParams.InstanceID);
	Environment.SetValue<Audio::FDeviceId>(SourceInterface::Environment::DeviceID, InParams.AudioDeviceID);
	Environment.SetValue<int32>(SourceInterface::Environment::AudioMixerNumOutputFrames, InParams.AudioMixerNumOutputFrames);

#if WITH_METASOUND_DEBUG_ENVIRONMENT
	Environment.SetValue<FString>(SourceInterface::Environment::GraphName, GetFullName());
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

	return Environment;
}

Metasound::FMetasoundEnvironment UMetaSoundSource::CreateEnvironment(const Audio::FParameterTransmitterInitParams& InParams) const
{
	using namespace Metasound;
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	FMetasoundEnvironment Environment = CreateEnvironment();
	Environment.SetValue<uint64>(SourceInterface::Environment::TransmitterID, InParams.InstanceID);

	return Environment;
}

const TArray<Metasound::FVertexName>& UMetaSoundSource::GetOutputAudioChannelOrder() const
{
	using namespace Metasound::SourcePrivate;

	if (const FOutputFormatInfo* FormatInfo = GetFormatInfoMap().Find(OutputFormat))
	{
		return FormatInfo->OutputVertexChannelOrder;
	}
	else
	{
		// Unhandled audio format. Need to update audio output format vertex key map.
		checkNoEntry();
		static const TArray<Metasound::FVertexName> Empty;
		return Empty;
	}
}
#undef LOCTEXT_NAMESPACE // MetaSound

