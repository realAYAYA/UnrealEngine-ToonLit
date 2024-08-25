// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundSource.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioDeviceManager.h"
#include "Containers/Ticker.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Internationalization/Text.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "MetasoundAudioFormats.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundDynamicOperatorTransactor.h"
#include "MetasoundEngineAsset.h"
#include "MetasoundEngineEnvironment.h"
#include "MetasoundEnvironment.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocumentIdGenerator.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundGenerator.h"
#include "MetasoundLog.h"
#include "MetasoundOperatorBuilderSettings.h"
#include "MetasoundOperatorCacheSubsystem.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundPrimitives.h"
#include "MetasoundReceiveNode.h"
#include "MetasoundSettings.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundUObjectRegistry.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundSource)

#if WITH_EDITORONLY_DATA
#include "EdGraph/EdGraph.h"
#endif // WITH_EDITORONLY_DATA

#define LOCTEXT_NAMESPACE "MetaSound"

namespace Metasound
{
	namespace ConsoleVariables
	{
		bool bEnableExperimentalRuntimePresetGraphInflation = false;
	}

	namespace SourcePrivate
	{
		static constexpr float DefaultBlockRateConstant = 100.f;
		static constexpr float DefaultSampleRateConstant = 48000.f;

		static bool IsCookedForEditor(const FArchive& InArchive, const UObject* InObj)
		{
#if WITH_EDITORONLY_DATA
			return ((InArchive.GetPortFlags() & PPF_Duplicate) == 0) && InObj->GetPackage()->HasAnyPackageFlags(PKG_Cooked);
#else //WITH_EDITORONLY_DATA
			return false;
#endif //WITH_EDITORONLY_DATA
		}
		
		static const FLazyName TriggerName = "Trigger";

		// Holds onto a global static TSet for tracking which error/warning logs have been
		// trigger in order to avoid log spam.
		bool HasNotBeenLoggedForThisObject(const UMetaSoundSource& InMetaSound, uint32 InLogLineNumber)
		{
			using FObjectAddressAndLineNum = TTuple<const void*, uint32>;

			static TSet<FObjectAddressAndLineNum> LoggedSet;

			bool bIsAlreadyInSet = false;
			LoggedSet.Add(FObjectAddressAndLineNum(&InMetaSound, InLogLineNumber), &bIsAlreadyInSet);

			return !bIsAlreadyInSet;
		}

		Frontend::FMetaSoundAssetRegistrationOptions GetInitRegistrationOptions()
		{
			Frontend::FMetaSoundAssetRegistrationOptions RegOptions;
			RegOptions.bForceReregister = false;
#if !WITH_EDITOR 
			if (Frontend::MetaSoundEnableCookDeterministicIDGeneration != 0)
			{
				// When without editor, don't AutoUpdate or ResolveDocument at runtime. This only happens at cook or save.
				// When with editor, those are needed because sounds are not necessarily saved before previewing.
				RegOptions.bAutoUpdate = false;
			}
#endif // !WITH_EDITOR
			if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
			{
				RegOptions.bAutoUpdateLogWarningOnDroppedConnection = Settings->bAutoUpdateLogWarningOnDroppedConnection;
			}

			return RegOptions;
		}

		class FParameterRouter
		{
			struct FQueueState
			{
				TWeakPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> DataChannel;
				bool bWriterAvailable = true;
			};

		public:

			using FAudioDeviceIDAndInstanceID = TTuple<Audio::DeviceID, uint64>;

			TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> FindOrCreateDataChannelForReader(Audio::DeviceID InDeviceID, uint64 InstanceID)
			{
				constexpr bool bIsForWriter = false;
				return FindOrCreateDataChannel(InDeviceID, InstanceID, bIsForWriter);
			}

			TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> FindOrCreateDataChannelForWriter(Audio::DeviceID InDeviceID, uint64 InstanceID)
			{
				constexpr bool bIsForWriter = true;
				return FindOrCreateDataChannel(InDeviceID, InstanceID, bIsForWriter);
			}

		private:

			TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> FindOrCreateDataChannel(Audio::DeviceID InDeviceID, uint64 InstanceID, bool bIsForWriter)
			{
				FScopeLock Lock(&DataChannelMapCS);

				FAudioDeviceIDAndInstanceID Key = {InDeviceID, InstanceID};
				const bool bIsForReader = !bIsForWriter;

				if (FQueueState* State = DataChannels.Find(Key))
				{
					// Allow multiple readers to be returned because FMetaSoundGenerators are recreated when they come out of virtualization.
					// Only allow a single writer to be returned because FMetaSoundParameterTransmitters are only created once
					const bool bIsAvailable = bIsForReader || (State->bWriterAvailable && bIsForWriter);
					if (bIsAvailable)
					{
						TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> Channel = State->DataChannel.Pin();
						if (Channel.IsValid())
						{
							if (bIsForWriter)
							{
								State->bWriterAvailable = false;
							}
							return Channel;
						}
					}
				}

				TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> NewChannel = MakeShared<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>>();

				FQueueState NewState;
				NewState.DataChannel = NewChannel;
				if (bIsForWriter)
				{
					NewState.bWriterAvailable = false;
				}

				DataChannels.Add(Key, NewState);
				return NewChannel;
			}

			FCriticalSection DataChannelMapCS;
			TSortedMap<FAudioDeviceIDAndInstanceID, FQueueState> DataChannels;
		};

		void CreateUObjectProxies(const Frontend::IDataTypeRegistry& InRegistry, FName InVertexTypeName, bool bClearUObjectPointers, FAudioParameter& InOutParamToInit)
		{
			using namespace Metasound;

			switch (InOutParamToInit.ParamType)
			{
				case EAudioParameterType::Object:
				{
					TSharedPtr<Audio::IProxyData> ProxyPtr = InRegistry.CreateProxyFromUObject(InVertexTypeName, InOutParamToInit.ObjectParam);
					InOutParamToInit.ObjectProxies.Emplace(MoveTemp(ProxyPtr));

					if (bClearUObjectPointers)
					{
						InOutParamToInit.ObjectParam = nullptr;
					}
				}
				break;

				case EAudioParameterType::ObjectArray:
				{
					const FName ElementTypeName = CreateElementTypeNameFromArrayTypeName(InVertexTypeName);
					for (TObjectPtr<UObject>& Object : InOutParamToInit.ArrayObjectParam)
					{
						TSharedPtr<Audio::IProxyData> ProxyPtr = InRegistry.CreateProxyFromUObject(ElementTypeName, Object);
						InOutParamToInit.ObjectProxies.Emplace(MoveTemp(ProxyPtr));
					}

					if (bClearUObjectPointers)
					{
						InOutParamToInit.ArrayObjectParam.Reset();
					}
				}
				break;

				default:
					break;
			}
		}

		FAudioParameter MakeAudioParameter(const Frontend::IDataTypeRegistry& InRegistry, FName InParamName, FName InTypeName, const FMetasoundFrontendLiteral& InLiteral, bool bCreateUObjectProxies) 
		{
			constexpr bool bClearUObjectPointers = false;

			FAudioParameter Params;
			Params.ParamName = InParamName;
			Params.TypeName = InTypeName;

			switch (InLiteral.GetType())
			{
				case EMetasoundFrontendLiteralType::Boolean:
				{
					if (Params.TypeName == TriggerName)
					{
						Params.ParamType = EAudioParameterType::Trigger;
					}
					else
					{
						Params.ParamType = EAudioParameterType::Boolean;
					}
						
					ensure(InLiteral.TryGet(Params.BoolParam));
				}
				break;

				case EMetasoundFrontendLiteralType::BooleanArray:
				{
					Params.ParamType = EAudioParameterType::BooleanArray;
					ensure(InLiteral.TryGet(Params.ArrayBoolParam));
				}
				break;

				case EMetasoundFrontendLiteralType::Integer:
				{
					Params.ParamType = EAudioParameterType::Integer;
					ensure(InLiteral.TryGet(Params.IntParam));
				}
				break;

				case EMetasoundFrontendLiteralType::IntegerArray:
				{
					Params.ParamType = EAudioParameterType::IntegerArray;
					ensure(InLiteral.TryGet(Params.ArrayIntParam));
				}
				break;

				case EMetasoundFrontendLiteralType::Float:
				{
					Params.ParamType = EAudioParameterType::Float;
					ensure(InLiteral.TryGet(Params.FloatParam));
				}
				break;

				case EMetasoundFrontendLiteralType::FloatArray:
				{
					Params.ParamType = EAudioParameterType::FloatArray;
					ensure(InLiteral.TryGet(Params.ArrayFloatParam));
				}
				break;

				case EMetasoundFrontendLiteralType::String:
				{
					Params.ParamType = EAudioParameterType::String;
					ensure(InLiteral.TryGet(Params.StringParam));
				}
				break;

				case EMetasoundFrontendLiteralType::StringArray:
				{
					Params.ParamType = EAudioParameterType::StringArray;
					ensure(InLiteral.TryGet(Params.ArrayStringParam));
				}
				break;

				case EMetasoundFrontendLiteralType::UObject:
				{
					Params.ParamType = EAudioParameterType::Object;
					UObject* Object = nullptr;
					ensure(InLiteral.TryGet(Object));
					Params.ObjectParam = Object;
					if (bCreateUObjectProxies)
					{
						CreateUObjectProxies(InRegistry, InTypeName, bClearUObjectPointers, Params);
					}
				}
				break;

				case EMetasoundFrontendLiteralType::UObjectArray:
				{
					Params.ParamType = EAudioParameterType::ObjectArray;
					ensure(InLiteral.TryGet(MutableView(Params.ArrayObjectParam)));
					if (bCreateUObjectProxies)
					{
						CreateUObjectProxies(InRegistry, InTypeName, bClearUObjectPointers, Params);
					}
				}
				break;

				default:
				break;
			}

			return Params;
		}

	} // namespace SourcePrivate
} // namespace Metasound

FAutoConsoleVariableRef CVarMetaSoundEnableExperimentalRUntimePresetGraphInflation(
	TEXT("au.MetaSound.Experimental.EnableRuntimePresetGraphInflation"),
	Metasound::ConsoleVariables::bEnableExperimentalRuntimePresetGraphInflation,
	TEXT("Enables experimental feature of MetaSounds which reduces overhead of preset graphs\n")
	TEXT("Default: false"),
	ECVF_Default);


UMetaSoundSource::UMetaSoundSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FMetasoundAssetBase()
{
	bProcedural = true;
	bRequiresStopFade = true;
	NumChannels = 1;
}

const UClass& UMetaSoundSource::GetBaseMetaSoundUClass() const
{
	return *UMetaSoundSource::StaticClass();
}

const FMetasoundFrontendDocument& UMetaSoundSource::GetConstDocument() const
{
	return RootMetasoundDocument;
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
	if (InEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, SampleRateOverride) ||
		InEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, BlockRateOverride) ||
		InEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, QualitySetting) )
	{
		PostEditChangeQualitySettings();
	}
}

bool UMetaSoundSource::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	// Allow changes to quality if we don't have any overrides.
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, QualitySetting))
	{
		const TArray<FName> Platforms = FDataDrivenPlatformInfoRegistry::GetSortedPlatformNames(EPlatformInfoType::AllPlatformInfos);
		const int32 DefaultBlockRate = BlockRateOverride.GetDefault();
		const float DefaultSampleRate = BlockRateOverride.GetDefault();

		if (DefaultBlockRate > 0 && DefaultSampleRate > 0)
		{
			return false;
		}

		for (const FName Platform : Platforms)
		{
			if (BlockRateOverride.GetValueForPlatform(Platform) != DefaultBlockRate )
			{
				return false;
			}
			if (!FMath::IsNearlyEqual(SampleRateOverride.GetValueForPlatform(Platform), DefaultSampleRate))
			{
				return false;
			}
		}
	}
		
	return true;
}

void UMetaSoundSource::PostEditChangeOutputFormat()
{
	using namespace Metasound::Frontend;

	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
	{
		const UMetaSoundBuilderSubsystem& BuilderSubsystem = UMetaSoundBuilderSubsystem::GetConstChecked();

		UMetaSoundSourceBuilder* SourceBuilder = BuilderSubsystem.AttachSourceBuilderToAsset(this);
		check(SourceBuilder);
		SourceBuilder->SetFormat(OutputFormat, Result);

		// TODO: Once builders are notified of controller changes and can be safely persistent, this
		// can be removed so builders can be shared and not have to be created for each change output
		// format mutation transaction.
		BuilderSubsystem.DetachBuilderFromAsset(GetConstDocument().RootGraph.Metadata.GetClassName());
	}

	if (Result == EMetaSoundBuilderResult::Succeeded)
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

void UMetaSoundSource::PostEditChangeQualitySettings()
{
	// Refresh the SampleRate (which is what the engine sees from the operator settings).
	SampleRate = GetOperatorSettings(CachedAudioDeviceSampleRate).GetSampleRate();

	// Always refresh the GUID with the selection.
	if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())	
	{
		auto FindByName = [&Name = QualitySetting](const FMetaSoundQualitySettings& Q) -> bool { return Q.Name == Name; };
		if (const FMetaSoundQualitySettings* Found = Settings->GetQualitySettings().FindByPredicate(FindByName))
		{
			QualitySettingGuid = Found->UniqueId;
		}
	}
}
#endif // WITH_EDITOR

bool UMetaSoundSource::ConformObjectDataToInterfaces()
{
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	bool bDidAlterObjectData = false;

	// Update the OutputFormat and NumChannels to match the audio format interface
	// on the root document.
	const FOutputAudioFormatInfoMap& FormatInfo = GetOutputAudioFormatInfo();
	for (const FOutputAudioFormatInfoPair& Pair : FormatInfo)
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

FTopLevelAssetPath UMetaSoundSource::GetAssetPathChecked() const
{
	return Metasound::FMetaSoundEngineAssetHelper::GetAssetPathChecked(*this);
}

void UMetaSoundSource::BeginDestroy()
{
#if WITH_SERVER_CODE
	Metasound::Frontend::IMetaSoundAssetManager::OnManagerSet.RemoveAll(this);
#endif //WITH_SERVER_CODE

	OnNotifyBeginDestroy();
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
	
	using namespace Metasound::SourcePrivate;	
	
	// Load/Save cooked data.
	if (InArchive.IsCooking() || (FPlatformProperties::RequiresCookedData() && InArchive.IsLoading()) || IsCookedForEditor(InArchive, this))
	{
		const FName PlatformName = InArchive.CookingTarget() ? *InArchive.CookingTarget()->IniPlatformName() : FName(FPlatformProperties::IniPlatformName());
		SerializeCookedQualitySettings(PlatformName, InArchive);
	}
}

bool UMetaSoundSource::GetQualitySettings(
	const FName InPlatformName, Metasound::SourcePrivate::FCookedQualitySettings& OutQualitySettings) const
{
#if WITH_EDITORONLY_DATA
	
	// Query Project settings. 
	if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
	{
		if (const FMetaSoundQualitySettings* Found = Settings->GetQualitySettings().FindByPredicate([&QT = QualitySetting](const FMetaSoundQualitySettings& Q) -> bool { return Q.Name == QT; }))
		{
			// Allow partial applications of settings, if some are non-zero.
			if (const float Value = Found->BlockRate.GetValueForPlatform(InPlatformName); Value > 0.f)
			{
				UE_LOG(LogMetaSound, VeryVerbose, TEXT("Metasound [%s] using Quality '%s', BlockRate=%3.3f" ), *GetName(), *QualitySetting.ToString(), Value);
				OutQualitySettings.BlockRate = Value;
			}
			if (const int32 Value = Found->SampleRate.GetValueForPlatform(InPlatformName); Value > 0)
			{
				UE_LOG(LogMetaSound, VeryVerbose, TEXT("Metasound [%s] using Quality '%s', SampleRate=%d" ), *GetName(), *QualitySetting.ToString(), Value);
				OutQualitySettings.SampleRate = Value;
			}
		}
	}

	// Query overrides defined on this asset.
	if (const float SerializedBlockRate = BlockRateOverride.GetValueForPlatform(InPlatformName); SerializedBlockRate > 0.0f)
	{
		UE_LOG(LogMetaSound, VeryVerbose, TEXT("Metasound [%s] BlockRate Override: %3.3f"), *GetName(), SerializedBlockRate);
		OutQualitySettings.BlockRate = SerializedBlockRate;
	}
	if (const int32 SerializedSampleRate = SampleRateOverride.GetValueForPlatform(InPlatformName); SerializedSampleRate > 0)
	{
		UE_LOG(LogMetaSound, VeryVerbose, TEXT("Metasound [%s] SampleRate Override: %d"), *GetName(), SerializedSampleRate);
		OutQualitySettings.SampleRate = SerializedSampleRate;
	}
	
	// Success.
	UE_LOG(LogMetaSound, Verbose, TEXT("Metasound [%s] using SampleRate=%d, BlockRate=%2.3f (not-cooked)"),
	 	*GetName(), OutQualitySettings.SampleRate.GetValue(), OutQualitySettings.BlockRate.GetValue()) ;
	
	return true;

#else //WITH_EDITORONLY_DATA	

	// If we've been cooked, this should contain the quality settings.
	if (CookedQualitySettings)
	{
		if (const float SerializedBlockRate = CookedQualitySettings->BlockRate.GetValue(); SerializedBlockRate > 0.0f)
		{
			UE_LOG(LogMetaSound, VeryVerbose, TEXT("Metasound [%s] BlockRate: %3.3f"), *GetName(), SerializedBlockRate);
			OutQualitySettings.BlockRate = SerializedBlockRate;
		}
		if (const int32 SerializedSampleRate = CookedQualitySettings->SampleRate.GetValue(); SerializedSampleRate > 0)
		{
			UE_LOG(LogMetaSound, VeryVerbose, TEXT("Metasound [%s] SampleRate: %d"), *GetName(), SerializedSampleRate);
			OutQualitySettings.SampleRate = SerializedSampleRate;
		}
	
		UE_LOG(LogMetaSound, Verbose, TEXT("Metasound [%s] using SampleRate=%d, BlockRate=%2.3f (cooked)"),
			*GetName(), OutQualitySettings.SampleRate.GetValue(), OutQualitySettings.BlockRate.GetValue()) ;
		return true;
	}
	
	// Fail.
	return false;
	
#endif //WITH_EDITORONLY_DATA
}
void UMetaSoundSource::SerializeCookedQualitySettings(const FName PlatformName, FArchive& Ar)
{
	Metasound::SourcePrivate::FCookedQualitySettings Settings;
	if (Ar.IsSaving())
	{
		GetQualitySettings(PlatformName, Settings);
	}
	// Use Struct Serializer.
	FMetaSoundQualitySettings::StaticStruct()->SerializeItem(Ar,&Settings,nullptr);
		
	if (Ar.IsLoading())
	{
		CookedQualitySettings = MakePimpl<Metasound::SourcePrivate::FCookedQualitySettings>(Settings);
	}
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

	PostLoadQualitySettings();
}

void UMetaSoundSource::PostLoadQualitySettings()
{
#if WITH_EDITORONLY_DATA
	
	// Ensure that our Quality settings resolve. 
	if (UMetaSoundSettings* Settings = GetMutableDefault<UMetaSoundSettings>())
	{
		ResolveQualitySettings(Settings);
		
		// Register for any changes to the settings while we're open in the editor.
		Settings->OnSettingChanged().AddWeakLambda(this, [WeakSource = MakeWeakObjectPtr(this)](UObject* InObj, struct FPropertyChangedEvent& InEvent)
		{
			if (
				WeakSource.IsValid() &&
				InEvent.GetMemberPropertyName() == UMetaSoundSettings::GetQualitySettingPropertyName()
			)
			{
				WeakSource->ResolveQualitySettings(CastChecked<UMetaSoundSettings>(InObj));
			}
		});

		// Register for changes from the CVars that control overrides.
		// We cache the OperatorSettings, so reset when these change.
		auto ResetOperatorSettings = [WeakSource = MakeWeakObjectPtr(this)](IConsoleVariable* Var)
		{
			if (WeakSource.IsValid())
			{
				WeakSource->ResolveQualitySettings(GetMutableDefault<UMetaSoundSettings>());
			
				// Override SampleRate with the Operator settings version which uses our Quality settings.
				WeakSource->SampleRate = WeakSource->GetOperatorSettings(WeakSource->CachedAudioDeviceSampleRate).GetSampleRate();
			}
		};
		Metasound::Frontend::GetBlockRateOverrideChangedDelegate().AddWeakLambda(this, ResetOperatorSettings);
		Metasound::Frontend::GetSampleRateOverrideChangedDelegate().AddWeakLambda(this, ResetOperatorSettings);
	}
#endif //WITH_EDITORONLY_DATA
}

void UMetaSoundSource::ResolveQualitySettings(const UMetaSoundSettings* Settings)
{
#if WITH_EDITORONLY_DATA

	const FMetaSoundQualitySettings* Resolved = nullptr;

	// 1. Try and resolve by name. (most should resolve unless its been renamed, deleted).
	auto FindByName = [&Name = QualitySetting](const FMetaSoundQualitySettings& Q) -> bool { return Q.Name == Name; };
	Resolved = Settings->GetQualitySettings().FindByPredicate(FindByName);


	// 2. If that failed, try by guid (if its been renamed in the settings, we can still find it).
	if (!Resolved && QualitySettingGuid.IsValid())
	{
		auto FindByGuid = [&Guid = QualitySettingGuid](const FMetaSoundQualitySettings& Q) -> bool { return Q.UniqueId == Guid; };
		Resolved = Settings->GetQualitySettings().FindByPredicate(FindByName);
	}

	// 3. If still failed to resolve, use defaults and warn.
	if (!Resolved)
	{
		// Disable the warning for now.
		//UE_LOG(LogMetaSound, Warning, TEXT("Failed to resolve Quality '%s', resetting to the default."), *QualitySetting.ToString());

		// Reset to defaults. (and make sure they are sane)
		QualitySetting = GetDefault<UMetaSoundSource>()->QualitySetting;
		QualitySettingGuid = GetDefault<UMetaSoundSource>()->QualitySettingGuid;
		if (!Settings->GetQualitySettings().FindByPredicate(FindByName) && !Settings->GetQualitySettings().IsEmpty())
		{
			// Default doesn't point to anything, use first one in the list.
			QualitySetting = Settings->GetQualitySettings()[0].Name;
			QualitySettingGuid = Settings->GetQualitySettings()[0].UniqueId;
		}				
	}

	// Refresh the guid/name now we've resolved to correctly reflect.
	if (Resolved)
	{
		QualitySetting = Resolved->Name;
		QualitySettingGuid = Resolved->UniqueId;
	}

#endif //WITH_EDITORONLY_DATA
}

void UMetaSoundSource::InitParameters(TArray<FAudioParameter>& ParametersToInit, FName InFeatureName)
{
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::InitParameters);


	if (bIsBuilderActive)	
	{
		// Do not create UObject proxies in the runtime input map because they proxies 
		// stored there will not be used. The necessary proxies in the ParametersToInit 
		// will be created and used instead. 
		constexpr bool bCreateUObjectProxiesInRuntimeInputMap = false; 
		InitParametersInternal(CreateRuntimeInputMap(bCreateUObjectProxiesInRuntimeInputMap), ParametersToInit, InFeatureName);
	}
	else
	{
		const bool bIsRuntimeInputDataValid = RuntimeInputData.bIsValid.load();
		if (bIsRuntimeInputDataValid)
		{
			InitParametersInternal(RuntimeInputData.InputMap, ParametersToInit, InFeatureName);
		}
		else
		{
			// The runtime input data should have been cached, but is not so we use
			// a fallback method. If this is occurring, then callers need to ensure
			// that InitResources has been called before this method executes or else
			// suffer the consequences of incurring significant performance losses 
			// each time a parameter is set on the MetaSound. 
			UE_CLOG(HasNotBeenLoggedForThisObject(*this, __LINE__), LogMetaSound, Warning, TEXT("Initializing parameters on uninitialized UMetaSoundSource %s will result in slower performance. UMetaSoundSource::InitResources should finish executing on the game thread before attempting to call UMetaSoundSource::InitParameters(...)"), *GetOwningAssetName());

			// Do not create UObject proxies in the runtime input map because they proxies 
			// stored there will not be used. The necessary proxies in the ParametersToInit 
			// will be created and used instead. 
			constexpr bool bCreateUObjectProxiesInRuntimeInputMap = false; 
			InitParametersInternal(CreateRuntimeInputMap(bCreateUObjectProxiesInRuntimeInputMap), ParametersToInit, InFeatureName);
		}
	}
}

void UMetaSoundSource::InitResources()
{
	using namespace Metasound::Frontend;
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::InitResources); 
	ensureMsgf(!IsRunningCookCommandlet(), TEXT("UMetaSoundSource::InitResources should not be called during cook."));

	if (IsInGameThread())
	{
#if WITH_SERVER_CODE
		
		if (IMetaSoundAssetManager::Get())
		{
			RegisterGraphWithFrontend(GetInitRegistrationOptions());
		}
		else
		{
			//to allow the server to preload UMetaSoundSource objects before the manager has been set up
			IMetaSoundAssetManager::OnManagerSet.AddUObject(this, &UMetaSoundSource::InitResources);
		}
#else
		RegisterGraphWithFrontend(GetInitRegistrationOptions());
#endif //WITH_SERVER_CODE
	}
	else
	{
		const bool bIsInGCSafeThread = IsInAudioThread() || IsInAsyncLoadingThread(); // Audio Thread is safe from GC, so we can safely construct the TWeakObjectPtr<> to this.
		if (!bIsInGCSafeThread)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Attempt to call UMetaSoundSource::InitResources() on %s from thread which may not provide garbage collection safety of the UMetaSoundSource"), *GetOwningAssetName());
		}

		ExecuteOnGameThread(
			UE_SOURCE_LOCATION, 
			[MetaSoundSourcePtr=TWeakObjectPtr<UMetaSoundSource>(this)]()
			{
				if (UMetaSoundSource* Source = MetaSoundSourcePtr.Get())
				{
					Source->InitResources();
				}
			}
		);
	}
}

void UMetaSoundSource::RegisterGraphWithFrontend(Metasound::Frontend::FMetaSoundAssetRegistrationOptions InRegistrationOptions)
{
	check(IsInGameThread());

	FMetasoundAssetBase::RegisterGraphWithFrontend(InRegistrationOptions);
	const bool bIsRuntimeInputDataValid = RuntimeInputData.bIsValid.load();
	// Runtime data does not need to and should not be created at cook
	if (!bIsRuntimeInputDataValid && !IsRunningCookCommandlet())
	{
		CacheRuntimeInputData();
	}
}

bool UMetaSoundSource::IsPlayable() const
{
	return true;
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

Metasound::Frontend::FDocumentAccessPtr UMetaSoundSource::GetDocumentAccessPtr()
{
	using namespace Metasound::Frontend;

	// Mutation of a document via the soft deprecated access ptr/controller system is not tracked by
	// the builder registry, so the document cache is invalidated here. It is discouraged to mutate
	// documents using both systems at the same time as it can corrupt a builder document's cache.
	if (UMetaSoundBuilderSubsystem* BuilderSubsystem = UMetaSoundBuilderSubsystem::Get())
	{
		const FMetasoundFrontendClassName& Name = RootMetasoundDocument.RootGraph.Metadata.GetClassName();
		BuilderSubsystem->InvalidateDocumentCache(Name);
	}

	// Return document using FAccessPoint to inform the TAccessPtr when the 
	// object is no longer valid.
	return MakeAccessPtr<FDocumentAccessPtr>(RootMetasoundDocument.AccessPoint, RootMetasoundDocument);
}

Metasound::Frontend::FConstDocumentAccessPtr UMetaSoundSource::GetDocumentConstAccessPtr() const
{
	using namespace Metasound::Frontend;

	// Return document using FAccessPoint to inform the TAccessPtr when the 
	// object is no longer valid.
	return MakeAccessPtr<FConstDocumentAccessPtr>(RootMetasoundDocument.AccessPoint, RootMetasoundDocument);
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
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::CreateSoundGenerator);

	FOperatorSettings InSettings = GetOperatorSettings(static_cast<FSampleRate>(InParams.SampleRate));

	SampleRate = InSettings.GetSampleRate();

	FMetasoundEnvironment Environment = CreateEnvironment(InParams);
	FParameterRouter& Router = UMetaSoundSource::GetParameterRouter();
	TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> DataChannel = Router.FindOrCreateDataChannelForReader(InParams.AudioDeviceID, InParams.InstanceID);

	FOperatorBuilderSettings BuilderSettings = FOperatorBuilderSettings::GetDefaultSettings();
	// Graph analyzer currently only enabled for preview sounds (but can theoretically be supported for all sounds)
	BuilderSettings.bPopulateInternalDataReferences = InParams.bIsPreviewSound;

	constexpr bool bBuildSynchronous = false;

	const bool bIsDynamic = DynamicTransactor.IsValid();
	TSharedPtr<FMetasoundGenerator> Generator;

	if (bIsDynamic)
	{
		// In order to ensure synchronization and avoid race conditions the current state
		// of the graph is copied and transform queue created here. This ensures that:
		//
		// 1. Modifications to the underlying FGraph in the FDynamicOperatorTransactor can continue 
		// while the generator is being constructed on an async task.  If this were not ensured, 
		// a race condition would be introduced wherein the FGraph could be manipulated while the
		// graph is being read while building the generator.
		//
		// 2. The state of the FGraph and TransformQueue are synchronized so that any additional
		// changes applied to the FDynamicOperatorTransactor will be placed in the TransformQueue.
		// The dynamic operator & generator will then consume these transforms after it has finished 
		// being built.

		BuilderSettings.bEnableOperatorRebind = true;

		FMetasoundDynamicGraphGeneratorInitParams InitParams
		{
			{
				InSettings,
				MoveTemp(BuilderSettings),
				MakeShared<FGraph>(DynamicTransactor->GetGraph()), // Make a copy of the graph.
				Environment,
				GetName(),
				GetOutputAudioChannelOrder(),
				MoveTemp(InDefaultParameters),
				bBuildSynchronous,
				DataChannel
			},
			DynamicTransactor->CreateTransformQueue(InSettings, Environment) // Create transaction queue
		};
		TSharedPtr<FMetasoundDynamicGraphGenerator> DynamicGenerator = MakeShared<FMetasoundDynamicGraphGenerator>(InSettings);
		DynamicGenerator->Init(MoveTemp(InitParams));

		Generator = MoveTemp(DynamicGenerator);
	}
	else 
	{
		// By default, the sound generator for a metasound preset uses a graph specifically
		// associated with the UMetaSoundSource_Preset. The overridden defaults for that
		// preset are baked into the IGraph. Unfortunately, this makes the MetaSound
		// operator pool less efficient because it associates the operator with the IGraph.
		// The way the presets use the IGraph mean that there is less sharing of cached
		// operators.
		//
		// To improve the efficiency of the operator pool, we have presets use their
		// base IGraphs so that more MetaSounds utilize the same IGraph. This requires
		// us to retrieve that specific graph. We also supply the parameters that were overridden
		// in the preset to the FMetaSoundGenerator, because they are not backed into
		// the base IGraph. 
		if (ConsoleVariables::bEnableExperimentalRuntimePresetGraphInflation && bIsPresetGraphInflationSupported)
		{
			// Get the graph associated with base graph which this preset wraps.
			TSharedPtr<const IGraph> MetasoundGraph = TryGetMetaSoundPresetBaseGraph();

			if (MetasoundGraph.IsValid())
			{
				// Combine the supplied parameters with the overridden parameters set on the preset.
				TArray<FAudioParameter> MergedParameters;
				MergePresetOverridesAndSuppliedDefaults(InDefaultParameters, MergedParameters);

				// Create generator.
				FMetasoundGeneratorInitParams InitParams
				{
					InSettings,
					MoveTemp(BuilderSettings),
					MetasoundGraph,
					Environment,
					GetName(),
					GetOutputAudioChannelOrder(),
					MoveTemp(MergedParameters),
					bBuildSynchronous,
					DataChannel
				};

				Generator = MakeShared<FMetasoundConstGraphGenerator>(MoveTemp(InitParams));
			}
		}

		if (!Generator.IsValid())
		{
			TSharedPtr<const FGraph> MetasoundGraph = FMetasoundFrontendRegistryContainer::Get()->GetGraph(GetGraphRegistryKey());
			if (!MetasoundGraph.IsValid())
			{
				return ISoundGeneratorPtr(nullptr);
			}

			FMetasoundGeneratorInitParams InitParams
			{
				InSettings,
				MoveTemp(BuilderSettings),
				MetasoundGraph,
				Environment,
				GetName(),
				GetOutputAudioChannelOrder(),
				MoveTemp(InDefaultParameters),
				bBuildSynchronous,
				DataChannel
			};

			Generator = MakeShared<FMetasoundConstGraphGenerator>(MoveTemp(InitParams));
		}
	}

	if (Generator.IsValid())
	{
		TrackGenerator(InParams.AudioComponentId, Generator);
	}

	return ISoundGeneratorPtr(Generator);
}

void UMetaSoundSource::OnEndGenerate(ISoundGeneratorPtr Generator)
{
	using namespace Metasound;
	ForgetGenerator(Generator);
}

bool UMetaSoundSource::GetAllDefaultParameters(TArray<FAudioParameter>& OutParameters) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace Metasound::Engine;

	
	if(!RuntimeInputData.bIsValid.load())
	{
		UE_CLOG(SourcePrivate::HasNotBeenLoggedForThisObject(*this, __LINE__), LogMetaSound, Warning, TEXT("Default parameters will be ommitted. Accessing invalid runtime data on MetaSound %s. Ensure that UMetaSoundSource::InitResources() is executed on the game thread before calling UMetaSoundSource::GetAllDefaultParameters(...)"), *GetOwningAssetName());
		return false;
	}

	for(const TPair<FVertexName, FRuntimeInput>& Pair : RuntimeInputData.InputMap)
	{
		OutParameters.Add(Pair.Value.DefaultParameter);
	}
	return true;
}

void UMetaSoundSource::InitParametersInternal(const Metasound::TSortedVertexNameMap<FRuntimeInput>& InInputMap, TArray<FAudioParameter>& ParametersToInit, FName InFeatureName) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::InitParametersInternal);

	checkf(IsInGameThread() || IsInAudioThread(), TEXT("Parameter initialization must happen on the GameThread or AudioThread to allow for safe creation of UObject proxies"));

	IDataTypeRegistry& DataTypeRegistry = IDataTypeRegistry::Get();

	// Removes values that are not explicitly defined by the ParamType
	auto Sanitize = [](FAudioParameter& Parameter)
	{
		switch (Parameter.ParamType)
		{
			case EAudioParameterType::Trigger:
			{
				Parameter = FAudioParameter(Parameter.ParamName, EAudioParameterType::Trigger);
			}
			break;
			
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
	};

	for (int32 i = ParametersToInit.Num() - 1; i >= 0; --i)
	{
		bool bIsParameterValid = false;

		FAudioParameter& Parameter = ParametersToInit[i];
		if (const FRuntimeInput* Input = InInputMap.Find(Parameter.ParamName))
		{
			if (IsParameterValidInternal(Parameter, Input->TypeName, DataTypeRegistry))
			{
				Sanitize(Parameter);
				constexpr bool bClearUObjectPointers = true; // protect against leaking UObject ptrs to the audio thread 
				SourcePrivate::CreateUObjectProxies(DataTypeRegistry, Input->TypeName, bClearUObjectPointers, Parameter);
				bIsParameterValid = true;
			}
		}

		if (!bIsParameterValid)
		{
			ParametersToInit.RemoveAtSwap(i, 1, EAllowShrinking::No);

#if !NO_LOGGING
			if (::Metasound::MetaSoundParameterEnableWarningOnIgnoredParameterCVar)
			{
				const FString AssetName = GetName();
				UE_LOG(LogMetaSound, Warning, TEXT("Failed to set parameter '%s' in asset '%s': No name specified, no transmittable input found, or type mismatch."), *Parameter.ParamName.ToString(), *AssetName);
			}
#endif // !NO_LOGGING
		}
	}
}

bool UMetaSoundSource::IsParameterValid(const FAudioParameter& InParameter) const
{
	const TArray<FMetasoundFrontendClassInput>& Inputs = GetDocumentChecked().RootGraph.Interface.Inputs;
	const FMetasoundFrontendVertex* Vertex = Algo::FindByPredicate(Inputs, [&InParameter] (const FMetasoundFrontendClassInput& Input)
	{
		return Input.Name == InParameter.ParamName;
	});
	
	if (Vertex)
	{
		return IsParameterValidInternal(InParameter, Vertex->TypeName, Metasound::Frontend::IDataTypeRegistry::Get());
	}
	else
	{
		return false;
	}
}

bool UMetaSoundSource::IsParameterValidInternal(const FAudioParameter& InParameter, const FName& InTypeName, Metasound::Frontend::IDataTypeRegistry& InDataTypeRegistry) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	if (InParameter.ParamName.IsNone())
	{
		// Invalid parameter name
		return false;
	}

	if (!InParameter.TypeName.IsNone() && InParameter.TypeName != InTypeName)
	{
		// Mismatched parameter type and vertex data type
		return false;
	}

	// Special handling for UObject proxies
	if (InParameter.ParamType == EAudioParameterType::Object)
	{
		return InDataTypeRegistry.IsValidUObjectForDataType(InTypeName, InParameter.ObjectParam);
	}
	else if (InParameter.ParamType == EAudioParameterType::ObjectArray)
	{
		bool bIsValid = true;

		const FName ElementTypeName = CreateElementTypeNameFromArrayTypeName(InTypeName);
		for (const UObject* Object : InParameter.ArrayObjectParam)
		{
			bIsValid = InDataTypeRegistry.IsValidUObjectForDataType(ElementTypeName, Object);
			if (!bIsValid)
			{
				break;
			}
		}
		return bIsValid;
	}

	const IDataTypeRegistryEntry* RegistryEntry = InDataTypeRegistry.FindDataTypeRegistryEntry(InTypeName);
	if (!RegistryEntry)
	{
		// Unregistered MetaSound data type
		return false;
	}

	switch (InParameter.ParamType)
	{
		case EAudioParameterType::Trigger:
		case EAudioParameterType::Boolean:
		{
			return RegistryEntry->GetDataTypeInfo().bIsBoolParsable;
		}
		break;

		case EAudioParameterType::BooleanArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsBoolArrayParsable;
		}
		break;

		case EAudioParameterType::Float:
		{
			return RegistryEntry->GetDataTypeInfo().bIsFloatParsable;
		}
		break;

		case EAudioParameterType::FloatArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsFloatArrayParsable;
		}
		break;

		case EAudioParameterType::Integer:
		{
			return RegistryEntry->GetDataTypeInfo().bIsIntParsable;
		}
		break;

		case EAudioParameterType::IntegerArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsIntArrayParsable;
		}
		break;

		case EAudioParameterType::String:
		{
			return RegistryEntry->GetDataTypeInfo().bIsStringParsable;
		}
		break;

		case EAudioParameterType::StringArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsStringArrayParsable;
		}
		break;

		case EAudioParameterType::NoneArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsDefaultArrayParsable;
		}

		case EAudioParameterType::None:
		{
			return RegistryEntry->GetDataTypeInfo().bIsDefaultParsable;
		}
		break;

		default:
		{
			// All parameter types should be covered.
			static_assert(static_cast<uint8>(EAudioParameterType::COUNT) == 13, "Possible unhandled EAudioParameterType");
			checkNoEntry();
			// Unhandled parameter type
			return false;
		}
	}

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
	using namespace Metasound;
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;

	auto CreateParameterTransmitterInternal = [this](const TSortedVertexNameMap<FRuntimeInput>& InInputMap, Audio::FParameterTransmitterInitParams& InParams)
	{
		// Build list of parameters that can be set at runtime.
		TArray<FName> ValidParameters;
		for (const TPair<FVertexName, FRuntimeInput>& Pair : InInputMap)
		{
			if (Pair.Value.bIsTransmittable && (Pair.Value.AccessType == EMetasoundFrontendVertexAccessType::Reference))
			{
				ValidParameters.Add(Pair.Value.Name);
			}
		}

		FParameterRouter& Router = UMetaSoundSource::GetParameterRouter();
		TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> DataChannel = Router.FindOrCreateDataChannelForWriter(InParams.AudioDeviceID, InParams.InstanceID);

		Metasound::FMetaSoundParameterTransmitter::FInitParams InitParams
		(
			GetOperatorSettings(InParams.SampleRate), 
			InParams.InstanceID, 
			MoveTemp(InParams.DefaultParams), 
			MoveTemp(ValidParameters), 
			DataChannel
		);

		InitParams.DebugMetaSoundName = this->GetFName();

		return MakeShared<Metasound::FMetaSoundParameterTransmitter>(MoveTemp(InitParams));
	};

	const bool bIsRuntimeInputDataValid = RuntimeInputData.bIsValid.load();
	const bool bCreateInputMapOnTheFly = bIsBuilderActive || !bIsRuntimeInputDataValid;

	if (bCreateInputMapOnTheFly)
	{
		if (!bIsBuilderActive)
		{
			// If we're not using a builder, that means the metasound cannot change and that the runtime input data should have been cached. 
			UE_LOG(LogMetaSound, Warning, TEXT("Creating a Parameter Transmiiter on uninitialized UMetaSoundSource %s will result in slower performance. UMetaSoundSource::InitResources should finish executing on the game thread before attempting to call UMetaSoundSource::CreateParameterTransmitter(...)"), *GetOwningAssetName());
		}

		// Do not create UObject proxies in the runtime input map because they proxies 
		// stored there will not be used. The necessary proxies in the ParametersToInit 
		// will be created and used instead. 
		constexpr bool bCreateUObjectProxiesInRuntimeInputMap = false; 
		return CreateParameterTransmitterInternal(CreateRuntimeInputMap(bCreateUObjectProxiesInRuntimeInputMap), InParams);
	}
	else
	{
		return CreateParameterTransmitterInternal(RuntimeInputData.InputMap, InParams);
	}
}

Metasound::FOperatorSettings UMetaSoundSource::GetOperatorSettings(Metasound::FSampleRate InDeviceSampleRate) const
{	
	using namespace Metasound;
	using namespace Metasound::SourcePrivate;
	
	// Default sensibly.
	FCookedQualitySettings Settings;
	Settings.BlockRate = DefaultBlockRateConstant;
	Settings.SampleRate = InDeviceSampleRate > 0 ? InDeviceSampleRate : DefaultSampleRateConstant;

	// Fetch our quality settings.
	// If we are cooked these are baked, if we are editor these are queried from the project settings and this assets overrides.
	const bool bFoundQualitySettings = GetQualitySettings(FPlatformProperties::IniPlatformName(), Settings);
	UE_CLOG(!bFoundQualitySettings && IsAsset(), LogMetaSound, Error, TEXT("Could not retrieve quality settings for asset %s"), *GetOwningAssetName());
	
	// Query CVars. (Override with CVars if they are > 0)
	using namespace Metasound::Frontend;
	const float BlockRateCVar = GetBlockRateOverride();
	const int32 SampleRateCvar = GetSampleRateOverride();

	if (SampleRateCvar > 0)
	{
		Settings.SampleRate = SampleRateCvar;
	}
	if (BlockRateCVar > 0)
	{
		Settings.BlockRate = BlockRateCVar;
	}

	// Sanity clamps.
	const TRange<float> BlockRange = GetBlockRateClampRange();
	const TRange<int32> RateRange = GetSampleRateClampRange();
	Settings.BlockRate = FMath::Clamp(Settings.BlockRate.GetValue(), BlockRange.GetLowerBoundValue(), BlockRange.GetUpperBoundValue());
	Settings.SampleRate = FMath::Clamp(Settings.SampleRate.GetValue(), RateRange.GetLowerBoundValue(), RateRange.GetUpperBoundValue());

	UE_LOG(LogMetaSound, Verbose, TEXT("Metasound [%s] GetOperatorSettings: SampleRate: %d, BlockRate: %3.3f"),
		*GetName(), Settings.SampleRate.GetValue(), Settings.BlockRate.GetValue());
	
	return Metasound::FOperatorSettings(
		/* SampleRate */ Settings.SampleRate.GetValue(),
		/* BlockRate */ Settings.BlockRate.GetValue());
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
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	if (const FOutputAudioFormatInfo* FormatInfo = GetOutputAudioFormatInfo().Find(OutputFormat))
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

void UMetaSoundSource::TrackGenerator(uint64 Id, TSharedPtr<Metasound::FMetasoundGenerator> Generator)
{
	FScopeLock GeneratorMapLock(&GeneratorMapCriticalSection);
	Generators.Add(Id, Generator);
	OnGeneratorInstanceCreated.Broadcast(Id, Generator);
}

void UMetaSoundSource::ForgetGenerator(ISoundGeneratorPtr Generator)
{
	using namespace Metasound;
	FMetasoundGenerator* AsMetasoundGenerator = static_cast<FMetasoundGenerator*>(Generator.Get());
	FScopeLock GeneratorMapLock(&GeneratorMapCriticalSection);
	for (auto It = Generators.begin(); It != Generators.end(); ++It)
	{
		if ((*It).Value.HasSameObject(AsMetasoundGenerator))
		{
			OnGeneratorInstanceDestroyed.Broadcast((*It).Key, StaticCastSharedPtr<Metasound::FMetasoundGenerator>(Generator));
			Generators.Remove((*It).Key);
			return;
		}
	}
}

TWeakPtr<Metasound::FMetasoundGenerator> UMetaSoundSource::GetGeneratorForAudioComponent(uint64 ComponentId) const
{
	using namespace Metasound;
	FScopeLock GeneratorMapLock(&GeneratorMapCriticalSection);
	const TWeakPtr<FMetasoundGenerator>* Result = Generators.Find(ComponentId);
	if (!Result)
	{
		return TWeakPtr<FMetasoundGenerator>(nullptr);
	}
	return *Result;
}


bool UMetaSoundSource::IsDynamic() const
{
	return DynamicTransactor.IsValid();
}

Metasound::SourcePrivate::FParameterRouter& UMetaSoundSource::GetParameterRouter()
{
	using namespace Metasound::SourcePrivate;

	static FParameterRouter Router;
	return Router;
}

bool UMetaSoundSource::IsBuilderActive() const
{
	return bIsBuilderActive;
}

void UMetaSoundSource::OnBeginActiveBuilder()
{
	using namespace Metasound::Frontend;

	if (bIsBuilderActive)
	{
		UE_LOG(LogMetaSound, Error, TEXT("OnBeginActiveBuilder() call while prior builder is still active. This may indicate that multiple builders are attempting to modify the MetaSound %s concurrently."), *GetOwningAssetName())
	}

	// If a builder is activating, make sure any in-flight registration
	// tasks have completed. Async registration tasks use the FMetasoundFrontendDocument
	// that lives on this object. We need to make sure that registration task
	// completes so that the FMetasoundFrontendDocument does not get modified
	// by a builder while it is also being read by async registration.
	const FGraphRegistryKey GraphKey = GetGraphRegistryKey();
	if (GraphKey.IsValid())
	{
		FMetasoundFrontendRegistryContainer::Get()->WaitForAsyncGraphRegistration(GraphKey);
	}

	bIsBuilderActive = true;

	// Currently we do not have information on whether inputs were added or removed
	// from the document. We invalidate the cached runtime inputs just in case.
	// MetaSounds which have an active builder should not be using cached runtime
	// input data until the builder is no longer active. 
	InvalidateCachedRuntimeInputData();
}

void UMetaSoundSource::OnFinishActiveBuilder()
{
	bIsBuilderActive = false;
}

TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> UMetaSoundSource::SetDynamicGeneratorEnabled(const FTopLevelAssetPath& InAssetPath, bool bInIsEnabled)
{
	using namespace Metasound;
	using namespace Metasound::DynamicGraph;

	if (bInIsEnabled)
	{
		if (!DynamicTransactor.IsValid())
		{
			// If a FGraph exists for this UMetaSoundSource, then we need to initialize
			// the DynamicTransactor with the existing FGraph so it has the correct
			// initial state. 
			//
			// Currently, any existing FGraph will be stored in the node registry,
			// hence we check if the graph is registered and retrieve the current 
			// graph to see if any FGraph already exists. 
			if (IsRegistered())
			{
				TSharedPtr<const FGraph> CurrentGraph = FMetasoundFrontendRegistryContainer::Get()->GetGraph(GetGraphRegistryKey());

				if (CurrentGraph.IsValid())
				{
					DynamicTransactor = MakeShared<FDynamicOperatorTransactor>(*CurrentGraph);
				}
				else
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Failed to get existing graph for dynamic metasound %s. Initializing to empty graph."), *GetOwningAssetName());
					DynamicTransactor = MakeShared<FDynamicOperatorTransactor>();
				}
			}
			else
			{
				DynamicTransactor = MakeShared<FDynamicOperatorTransactor>();
			}
		}
	}
	else
	{
		DynamicTransactor.Reset();
	}

	return DynamicTransactor;
}

TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> UMetaSoundSource::GetDynamicGeneratorTransactor() const
{
	return DynamicTransactor;
}

UMetaSoundSource::FRuntimeInput UMetaSoundSource::CreateRuntimeInput(const Metasound::Frontend::IDataTypeRegistry& Registry, const FMetasoundFrontendClassInput& Input, bool bCreateUObjectProxies)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	bool bIsTransmittable = false;
	if (const IDataTypeRegistryEntry* RegistryEntry = Registry.FindDataTypeRegistryEntry(Input.TypeName))
	{
		bIsTransmittable = RegistryEntry->GetDataTypeInfo().bIsTransmittable;
	}
	else
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Failed to find data type '%s' in registry. Assuming data type is not transmittable"), *Input.TypeName.ToString());
	}

	FAudioParameter DefaultParameter = SourcePrivate::MakeAudioParameter(Registry, Input.Name, Input.TypeName, Input.DefaultLiteral, bCreateUObjectProxies);

	return FRuntimeInput { Input.Name, Input.TypeName, Input.AccessType, DefaultParameter, bIsTransmittable };
}

Metasound::TSortedVertexNameMap<UMetaSoundSource::FRuntimeInput> UMetaSoundSource::CreateRuntimeInputMap(bool bCreateUObjectProxies) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::CreateRuntimeInputMap);

	auto GetInputName = [](const FMetasoundFrontendClassInput& InInput) { return InInput.Name; };

	const IDataTypeRegistry& Registry = IDataTypeRegistry::Get();
	const FMetasoundFrontendDocument& Doc = GetConstDocument();

	TArray<const IInterfaceRegistryEntry*> Interfaces;
	FMetaSoundFrontendDocumentBuilder::FindDeclaredInterfaces(Doc, Interfaces);

	// Inputs which are controlled by an interface are private unless
	// their router name is `Audio::IParameterTransmitter::RouterName`
	TSet<FVertexName> PrivateInputs;
	for (const IInterfaceRegistryEntry* InterfaceEntry : Interfaces)
	{
		if (InterfaceEntry)
		{
			if (InterfaceEntry->GetRouterName() != Audio::IParameterTransmitter::RouterName)
			{
				const FMetasoundFrontendInterface& Interface = InterfaceEntry->GetInterface();
				Algo::Transform(Interface.Inputs, PrivateInputs, GetInputName);
			}
		}
	}

	// Cache all inputs which are not private inputs.
	TSortedVertexNameMap<FRuntimeInput> PublicInputs;
	for (const FMetasoundFrontendClassInput& Input : Doc.RootGraph.Interface.Inputs)
	{
		if (!PrivateInputs.Contains(Input.Name))
		{
			PublicInputs.Add(Input.Name, CreateRuntimeInput(Registry, Input, bCreateUObjectProxies));
		}
	}

	// Add the parameter pack input that ALL Metasounds have
	FMetasoundFrontendClassInput ParameterPackInput = UMetasoundParameterPack::GetClassInput();
	FAudioParameter ParameterPackDefaultParameter = SourcePrivate::MakeAudioParameter(Registry, ParameterPackInput.Name, ParameterPackInput.TypeName, ParameterPackInput.DefaultLiteral, bCreateUObjectProxies) ;
	PublicInputs.Add(ParameterPackInput.Name, FRuntimeInput{ParameterPackInput.Name, ParameterPackInput.TypeName, ParameterPackInput.AccessType, ParameterPackDefaultParameter, true /* bIsTransmittable */});
	
	return PublicInputs;
}

void UMetaSoundSource::CacheRuntimeInputData()
{
	using namespace Metasound;

	if (bIsBuilderActive)
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Skipping caching of runtime inputs for UMetaSoundSource %s because there is an active builder"), *GetOwningAssetName());
	}

	constexpr bool bCreateUObjectProxies = true; 
	RuntimeInputData.InputMap = CreateRuntimeInputMap(bCreateUObjectProxies);

	// Determine if preset graph inflation is possible 
	//
	// Constructor inputs conflict with `Preset Graph Inflation` and `Operator Caching`. 
	// This logic protects against attempting to use preset graph inflation when the preset 
	// graph has overridden constructor pins. 
	// 
	// Operator caching of base preset graphs fail when there are constructor inputs because
	// constructor inputs set on the preset cannot be updated after the base operator is 
	// cached. 
	auto IsOverriddenConstructorInput = [&InputsInheritingDefault=RootMetasoundDocument.RootGraph.PresetOptions.InputsInheritingDefault](const TPair<FVertexName, FRuntimeInput>& Pair) 
	{
		if (Pair.Value.AccessType == EMetasoundFrontendVertexAccessType::Value)
		{
			return !InputsInheritingDefault.Contains(Pair.Key);
		}
		return false;
	};

	bIsPresetGraphInflationSupported = RootMetasoundDocument.RootGraph.PresetOptions.bIsPreset && !Algo::AnyOf(RuntimeInputData.InputMap, IsOverriddenConstructorInput);

	RuntimeInputData.bIsValid.store(true);
}

void UMetaSoundSource::InvalidateCachedRuntimeInputData()
{
	bIsPresetGraphInflationSupported = false; 
	RuntimeInputData.bIsValid.store(false);
}

TSharedPtr<const Metasound::IGraph> UMetaSoundSource::TryGetMetaSoundPresetBaseGraph() const
{
	using namespace Metasound;

	TSharedPtr<const IGraph> MetasoundGraph;
	if (ReferencedAssetClassObjects.Num() == 1)
	{
		// Get first element from TSet<>
		TObjectPtr<const UObject> BaseGraph = *ReferencedAssetClassObjects.CreateConstIterator();

		// Get the reference graph as a UMetaSoundSource
		TObjectPtr<const UMetaSoundSource> BaseMetaSoundSource = Cast<const UMetaSoundSource>(BaseGraph);
		if (BaseMetaSoundSource)
		{
			// Get registered graph of base metasound source.
			MetasoundGraph = FMetasoundFrontendRegistryContainer::Get()->GetGraph(BaseMetaSoundSource->GetGraphRegistryKey());
		}
	}
	else
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Attempt to reference parent of metasound preset failed due to unexpected number of reference asses (%d) from MetaSound Preset %s"), ReferencedAssetClassObjects.Num(), *GetOwningAssetName());
	}

	return MetasoundGraph;
}

void UMetaSoundSource::MergePresetOverridesAndSuppliedDefaults(const TArray<FAudioParameter>& InSuppliedDefaults, TArray<FAudioParameter>& OutMerged)
{
	using namespace Metasound;

	if(!RuntimeInputData.bIsValid.load())
	{
		UE_CLOG(SourcePrivate::HasNotBeenLoggedForThisObject(*this, __LINE__), LogMetaSound, Warning, TEXT("Failed to use experimental preset inflation due to invalid runtime data on MetaSound %s. Ensure that UMetaSoundSource::InitResources() is executed on the game thread before CreateSoundGenerator"), *GetOwningAssetName());
	}
	else
	{
		// Get parameters that are overridden in the Preset.
		for (const TPair<FVertexName, FRuntimeInput>& Pair : RuntimeInputData.InputMap)
		{
			if (!RootMetasoundDocument.RootGraph.PresetOptions.InputsInheritingDefault.Contains(Pair.Key))
			{
				OutMerged.Add(Pair.Value.DefaultParameter);
			}
		}

		// Instead of using `FAudioParameter::Merge(...)` we roll custom merge logic because
		// FAudioParameter::Merge(...) requires us to move the InSuppliedDefaults into the
		// OutMerged array. This unwanted in this case because we want to maintain InSuppliedDefaults
		// for the scenario that the experimental preset override fails to create a generator
		// in which case we are forced to use the prior codepath which does not merge these
		// two parameter arrays. 
		for (const FAudioParameter& SuppliedParameter : InSuppliedDefaults)
		{
			auto HasSameName = [&Name=SuppliedParameter.ParamName](const FAudioParameter& InOtherParam) -> bool
			{
				return InOtherParam.ParamName == Name;
			};

			// The supplied defaults generally come from BP and should override any other parameters.
			if (FAudioParameter* Param = OutMerged.FindByPredicate(HasSameName))
			{
				*Param = SuppliedParameter;
			}
			else
			{
				OutMerged.Add(SuppliedParameter);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE // MetaSound
