// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEngineModule.h"

#include "Metasound.h"
#include "MetasoundAudioBus.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundOutputSubsystem.h"
#include "MetasoundSource.h"
#include "MetasoundTrace.h"
#include "MetasoundUObjectRegistry.h"
#include "MetasoundWave.h"
#include "MetasoundWaveTable.h"
#include "Analysis/MetasoundFrontendAnalyzerRegistry.h"
#include "Analysis/MetasoundFrontendVertexAnalyzerAudioBuffer.h"
#include "Analysis/MetasoundFrontendVertexAnalyzerEnvelopeFollower.h"
#include "Analysis/MetasoundFrontendVertexAnalyzerForwardValue.h"
#include "Analysis/MetasoundFrontendVertexAnalyzerTriggerDensity.h"
#include "Analysis/MetasoundFrontendVertexAnalyzerTriggerToTime.h"
#include "Interfaces/MetasoundDeprecatedInterfaces.h"
#include "Interfaces/MetasoundInterface.h"
#include "Interfaces/MetasoundInterfaceBindingsPrivate.h"
#include "Modules/ModuleManager.h"
#include "Sound/AudioSettings.h"

DEFINE_LOG_CATEGORY(LogMetasoundEngine);

namespace Metasound
{
	// Enable send/receive node registration for data types which existed before
	// send/receive were deprecated in order to support old UMetaSound assets. 
	template<>
	struct TEnableTransmissionNodeRegistration<FWaveAsset>
	{
		static constexpr bool Value = true;
	};
}

REGISTER_METASOUND_DATATYPE(Metasound::FAudioBusAsset, "AudioBusAsset", Metasound::ELiteralType::UObjectProxy, UAudioBus);
REGISTER_METASOUND_DATATYPE(Metasound::FWaveAsset, "WaveAsset", Metasound::ELiteralType::UObjectProxy, USoundWave);
REGISTER_METASOUND_DATATYPE(WaveTable::FWaveTable, "WaveTable", Metasound::ELiteralType::FloatArray)
REGISTER_METASOUND_DATATYPE(Metasound::FWaveTableBankAsset, "WaveTableBankAsset", Metasound::ELiteralType::UObjectProxy, UWaveTableBank);


class FMetasoundEngineModule : public IMetasoundEngineModule
{
	// Supplies GC referencing in the MetaSound Frontend node registry for doing
	// async work on UObjets
	class FObjectReferencer 
		: public FMetasoundFrontendRegistryContainer::IObjectReferencer
		, public FGCObject
	{
	public:
		virtual void AddObject(UObject* InObject) override
		{
			FScopeLock LockObjectArray(&ObjectArrayCriticalSection);
			ObjectArray.Add(InObject);
		}

		virtual void RemoveObject(UObject* InObject) override
		{
			FScopeLock LockObjectArray(&ObjectArrayCriticalSection);
			ObjectArray.Remove(InObject);
		}

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			FScopeLock LockObjectArray(&ObjectArrayCriticalSection);
			Collector.AddReferencedObjects(ObjectArray);
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("FMetasoundEngineModule::FObjectReferencer");
		}

	private:
		mutable FCriticalSection ObjectArrayCriticalSection;
		TArray<TObjectPtr<UObject>> ObjectArray;
	};

public:

	virtual void StartupModule() override
	{
		using namespace Metasound;
		using namespace Metasound::Engine;

		METASOUND_LLM_SCOPE;
		FModuleManager::Get().LoadModuleChecked("MetasoundGraphCore");
		FModuleManager::Get().LoadModuleChecked("MetasoundFrontend");
		FModuleManager::Get().LoadModuleChecked("MetasoundStandardNodes");
		FModuleManager::Get().LoadModuleChecked("MetasoundGenerator");
		FModuleManager::Get().LoadModuleChecked("WaveTable");
		
		// Set GCObject referencer for metasound frontend node registry. The MetaSound
		// frontend does not have access to Engine GC tools and must have them 
		// supplied externally.
		FMetasoundFrontendRegistryContainer::Get()->SetObjectReferencer(MakeUnique<FObjectReferencer>());

		// Register engine-level parameter interfaces if not done already.
		// (Potentially not already called if plugin is loaded while cooking.)
		UAudioSettings* AudioSettings = GetMutableDefault<UAudioSettings>();
		check(AudioSettings);
		AudioSettings->RegisterParameterInterfaces();

		IMetasoundUObjectRegistry::Get().RegisterUClass(MakeUnique<TMetasoundUObjectRegistryEntry<UMetaSoundBuilderDocument>>());
		IMetasoundUObjectRegistry::Get().RegisterUClass(MakeUnique<TMetasoundUObjectRegistryEntry<UMetaSoundPatch>>());
		IMetasoundUObjectRegistry::Get().RegisterUClass(MakeUnique<TMetasoundUObjectRegistryEntry<UMetaSoundSource>>());

		Engine::RegisterDeprecatedInterfaces();
		Engine::RegisterInterfaces();
		Engine::RegisterInternalInterfaceBindings();

		// Flush node registration queue
		FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();

		// Register Analyzers
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerAudioBuffer)
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerEnvelopeFollower)
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerForwardBool)
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerForwardFloat)
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerForwardInt)
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerForwardTime)
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerForwardString)
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerTriggerDensity)
		METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(Frontend::FVertexAnalyzerTriggerToTime)

		// Register passthrough output analyzers
		UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
			GetMetasoundDataTypeName<float>(),
			Frontend::FVertexAnalyzerForwardFloat::GetAnalyzerName(),
			Frontend::FVertexAnalyzerForwardFloat::FOutputs::GetValue().Name);
		UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
			GetMetasoundDataTypeName<int32>(),
			Frontend::FVertexAnalyzerForwardInt::GetAnalyzerName(),
			Frontend::FVertexAnalyzerForwardInt::FOutputs::GetValue().Name);
		UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
			GetMetasoundDataTypeName<bool>(),
			Frontend::FVertexAnalyzerForwardBool::GetAnalyzerName(),
			Frontend::FVertexAnalyzerForwardBool::FOutputs::GetValue().Name);
		UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
			GetMetasoundDataTypeName<FString>(),
			Frontend::FVertexAnalyzerForwardString::GetAnalyzerName(),
			Frontend::FVertexAnalyzerForwardString::FOutputs::GetValue().Name);
		UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
			GetMetasoundDataTypeName<FTime>(),
			Frontend::FVertexAnalyzerForwardTime::GetAnalyzerName(),
			Frontend::FVertexAnalyzerForwardTime::FOutputs::GetValue().Name);
		UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
			GetMetasoundDataTypeName<FTrigger>(),
			Frontend::FVertexAnalyzerTriggerToTime::GetAnalyzerName(),
			Frontend::FVertexAnalyzerTriggerToTime::FOutputs::GetValue().Name);

		UE_LOG(LogMetasoundEngine, Log, TEXT("MetaSound Engine Initialized"));
	}
};

IMPLEMENT_MODULE(FMetasoundEngineModule, MetasoundEngine);
