// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundGeneratorHandle.h"

#include "MetasoundGenerator.h"
#include "MetasoundParameterPack.h"
#include "MetasoundSource.h"
#include "MetasoundTrace.h"
#include "Analysis/MetasoundFrontendAnalyzerRegistry.h"
#include "Components/AudioComponent.h"

TMap<FName, UMetasoundGeneratorHandle::FPassthroughAnalyzerInfo> UMetasoundGeneratorHandle::PassthroughAnalyzers{};

UMetasoundGeneratorHandle* UMetasoundGeneratorHandle::CreateMetaSoundGeneratorHandle(UAudioComponent* OnComponent)
{
	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundGeneratorHandle::CreateMetaSoundGeneratorHandle);
	
	if (!OnComponent)
	{
		return nullptr;
	}

	if (OnComponent->bCanPlayMultipleInstances)
	{
		UE_LOG(
			LogMetaSound,
			Warning,
			TEXT("Created a UMetaSoundGeneratorHandle for a UAudioComponent that is allowed to play multiple instances. This may not work as expected."))
	}

	UMetasoundGeneratorHandle* Result = NewObject<UMetasoundGeneratorHandle>();
	Result->SetAudioComponent(OnComponent);
	return Result;
}

void UMetasoundGeneratorHandle::BeginDestroy()
{
	Super::BeginDestroy();
	DetachGeneratorDelegates();

	if (TSharedPtr<Metasound::FMetasoundGenerator> PinnedGenerator = CachedGeneratorPtr.Pin())
	{
		PinnedGenerator->OnOutputChanged.Remove(OutputChangedDelegateHandle);
	}
}

bool UMetasoundGeneratorHandle::IsValid() const
{
	return AudioComponent.IsValid();
}

uint64 UMetasoundGeneratorHandle::GetAudioComponentId() const
{
	if (AudioComponent.IsValid())
	{
		return AudioComponent->GetAudioComponentID();
	}

	return INDEX_NONE;
}

void UMetasoundGeneratorHandle::ClearCachedData()
{
	DetachGeneratorDelegates();
	AudioComponent        = nullptr;
	CachedMetasoundSource = nullptr;
	CachedGeneratorPtr    = nullptr;
	CachedParameterPack   = nullptr;
}

void UMetasoundGeneratorHandle::SetAudioComponent(UAudioComponent* InAudioComponent)
{
	if (InAudioComponent != AudioComponent)
	{
		ClearCachedData();
		AudioComponent   = InAudioComponent;
	}
}

void UMetasoundGeneratorHandle::CacheMetasoundSource()
{
	if (!AudioComponent.IsValid())
	{
		return;
	}

	UMetaSoundSource* CurrentMetasoundSource = Cast<UMetaSoundSource>(AudioComponent->GetSound());
	if (CachedMetasoundSource == CurrentMetasoundSource)
	{
		return;
	}

	DetachGeneratorDelegates();
	CachedGeneratorPtr    = nullptr;
	CachedMetasoundSource = CurrentMetasoundSource;

	if (CachedMetasoundSource.IsValid())
	{
		AttachGeneratorDelegates();
	}
}

void UMetasoundGeneratorHandle::AttachGeneratorDelegates()
{
	// These delegates can be called on a separate thread, so we need to take steps
	// to assure this UObject hasn't been garbage collected before trying to dereference. 
	// That is why we capture a TWeakObjectPtr and try to dereference that later!
	GeneratorCreatedDelegateHandle = CachedMetasoundSource->OnGeneratorInstanceCreated.AddLambda(
		[WeakGeneratorHandlePtr = TWeakObjectPtr<UMetasoundGeneratorHandle>(this),
		StatId = this->GetStatID(true)]
		(uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator)
		{
			// We are in the audio render (or control) thread here, so create a "dispatch task" to be
			// executed later on the game thread...
			FFunctionGraphTask::CreateAndDispatchWhenReady(
				[WeakGeneratorHandlePtr, InAudioComponentId, InGenerator]()
				{
					METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundGeneratorHandle::CallingGeneratorAttachedDelegates);
					check(IsInGameThread());
					// Now, since we are in the game thread, try to dereference the pointer to
					// to the UMetasoundGeneratorHandle. This should only succeed if the UObject
					// hasn't been garbage collected.
					if (UMetasoundGeneratorHandle* TheHandle = WeakGeneratorHandlePtr.Get())
					{
						TheHandle->OnSourceCreatedAGenerator(InAudioComponentId, InGenerator);
					}
				}, 
				StatId, nullptr, ENamedThreads::GameThread);
		});
	GeneratorDestroyedDelegateHandle = CachedMetasoundSource->OnGeneratorInstanceDestroyed.AddLambda(
		[WeakGeneratorHandlePtr = TWeakObjectPtr<UMetasoundGeneratorHandle>(this),
		StatId = this->GetStatID(true)]
		(uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator)
		{
			// We are in the audio render (or control) thread here, so create a "dispatch task" to be
			// executed later on the game thread...
			FFunctionGraphTask::CreateAndDispatchWhenReady(
				[WeakGeneratorHandlePtr, InAudioComponentId, InGenerator]()
				{
					METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundGeneratorHandle::CallingGeneratorDetachedDelegates);
					check(IsInGameThread());
					// Now, since we are in the game thread, try to dereference the pointer to
					// to the UMetasoundGeneratorHandle. This should only succeed if the UObject
					// hasn't been garbage collected.
					if (UMetasoundGeneratorHandle* TheHandle = WeakGeneratorHandlePtr.Get())
					{
						TheHandle->OnSourceDestroyedAGenerator(InAudioComponentId, InGenerator);
					}
				},
				StatId, nullptr, ENamedThreads::GameThread);
		});
}

void UMetasoundGeneratorHandle::AttachGraphChangedDelegate()
{
	if (!CachedGeneratorPtr.IsValid() || !OnGeneratorsGraphChanged.IsBound())
		return;

	TSharedPtr<Metasound::FMetasoundGenerator> Generator = CachedGeneratorPtr.Pin();
	if (Generator)
	{
		// We're about to add a delegate to the generator. This delegate will be called on 
		// the audio render thread so we need to take steps to assure this UMetasoundGeneratorHandle
		// hasn't been garbage collected before trying to dereference it later when the delegate "fires".
		// That is why we capture a TWeakObjectPtr and try to dereference that later!
		Metasound::FOnSetGraph::FDelegate NewSetGraphDelegate;
		NewSetGraphDelegate.BindLambda([WeakGeneratorHandlePtr = TWeakObjectPtr<UMetasoundGeneratorHandle>(this), StatId = this->GetStatID(true)]
			()
			{
				// We are in the audio render thread here, so create a "dispatch task" to be
				// executed later on the game thread...
				FFunctionGraphTask::CreateAndDispatchWhenReady(
					[WeakGeneratorHandlePtr]()
					{
						METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundGeneratorHandle::CallingGraphChangedDelegates);
						check(IsInGameThread());
						// Now, since we are in the game thread, try to dereference the pointer to
						// to the UMetasoundGeneratorHandle. This should only succeed if the UObject
						// hasn't been garbage collected.
						if (UMetasoundGeneratorHandle* TheHandle = WeakGeneratorHandlePtr.Get())
						{
							TheHandle->OnGeneratorsGraphChanged.Broadcast();
						}
					},
					StatId, nullptr, ENamedThreads::GameThread);
			});

		GeneratorGraphChangedDelegateHandle = Generator->AddGraphSetCallback(MoveTemp(NewSetGraphDelegate));
	}
}

void UMetasoundGeneratorHandle::DetachGeneratorDelegates()
{
	// First detach any callbacks that tell us when a generator is created or destroyed
	// for the UMetasoundSource of interest...
	if (CachedMetasoundSource.IsValid())
	{
	CachedMetasoundSource->OnGeneratorInstanceCreated.Remove(GeneratorCreatedDelegateHandle);
	GeneratorCreatedDelegateHandle.Reset();
	CachedMetasoundSource->OnGeneratorInstanceDestroyed.Remove(GeneratorDestroyedDelegateHandle);
	GeneratorDestroyedDelegateHandle.Reset();
}
	// Now detach any callback we may have registered to get callbacks when the generator's
	// graph has been changed...
	TSharedPtr<Metasound::FMetasoundGenerator> PinnedGenerator = CachedGeneratorPtr.Pin();
	if (PinnedGenerator)
	{
		PinnedGenerator->RemoveGraphSetCallback(GeneratorGraphChangedDelegateHandle);
		GeneratorGraphChangedDelegateHandle.Reset();
	}
}

TSharedPtr<Metasound::FMetasoundGenerator> UMetasoundGeneratorHandle::PinGenerator()
{
	TSharedPtr<Metasound::FMetasoundGenerator> PinnedGenerator = CachedGeneratorPtr.Pin();
	if (PinnedGenerator.IsValid() || !CachedMetasoundSource.IsValid())
	{
		return PinnedGenerator;
	}

	// The first attempt to pin failed, so reach out to the MetaSoundSource and see if it has a 
	// generator for our AudioComponent...
	check(AudioComponent.IsValid()); // expect the audio component to still be valid if the generator is.
	CachedGeneratorPtr = CachedMetasoundSource->GetGeneratorForAudioComponent(AudioComponent->GetAudioComponentID());
	PinnedGenerator    = CachedGeneratorPtr.Pin();
	return PinnedGenerator;
}

bool UMetasoundGeneratorHandle::ApplyParameterPack(UMetasoundParameterPack* Pack)
{
	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundGeneratorHandle::ApplyParameterPack);
	
	if (!Pack)
	{
		return false;
	}

	// Create a copy of the parameter pack and cache it.
	CachedParameterPack = Pack->GetCopyOfParameterStorage();

	// No point in continuing if the parameter pack is not valid for any reason.
	if (!CachedParameterPack.IsValid())
	{
		return false;
	}

	// Assure that our MetaSoundSource is up to date. It is possible that this has been 
	// changed via script since we were first created.
	CacheMetasoundSource();

	// Now we can try to pin the generator.
	TSharedPtr<Metasound::FMetasoundGenerator> PinnedGenerator = PinGenerator();

	if (!PinnedGenerator.IsValid())
	{
		// Failed to pin the generator, but we have cached the parameter pack,
		// so if our delegate gets called when a new generator is created we can 
		// apply the cached parameters then.
		return false;
	}

	// Finally... send down the parameter pack.
	PinnedGenerator->QueueParameterPack(CachedParameterPack);
	return true;
}

TSharedPtr<Metasound::FMetasoundGenerator> UMetasoundGeneratorHandle::GetGenerator()
{
	// Attach if we aren't attached, check for changes, etc...
	CacheMetasoundSource();

	return PinGenerator();
}

FDelegateHandle UMetasoundGeneratorHandle::AddGraphSetCallback(UMetasoundGeneratorHandle::FOnSetGraph::FDelegate&& Delegate)
{
	
	FDelegateHandle Handle = OnGeneratorsGraphChanged.Add(Delegate);
	CacheMetasoundSource();
	AttachGraphChangedDelegate();
	return Handle;
}

bool UMetasoundGeneratorHandle::RemoveGraphSetCallback(const FDelegateHandle& Handle)
{
	return OnGeneratorsGraphChanged.Remove(Handle);
}

bool AnalyzerAddressesReferToSameGeneratorOutput(const Metasound::Frontend::FAnalyzerAddress& Lhs, const Metasound::Frontend::FAnalyzerAddress& Rhs)
{
	return Lhs.OutputName == Rhs.OutputName && Lhs.AnalyzerName == Rhs.AnalyzerName && Lhs.AnalyzerMemberName == Rhs.AnalyzerMemberName;
}

bool UMetasoundGeneratorHandle::WatchOutput(
	FName OutputName,
	const FOnMetasoundOutputValueChanged& OnOutputValueChanged,
	FName AnalyzerName,
	FName AnalyzerOutputName)
{
	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundGeneratorHandle::WatchOutput);

	if (!IsValid())
	{
		return false;
	}

	CacheMetasoundSource();

	if (!CachedMetasoundSource.IsValid())
	{
		return false;
	}

	// Find the node id and type name
	const Metasound::Frontend::FNodeHandle Node = CachedMetasoundSource->GetRootGraphHandle()->GetOutputNodeWithName(OutputName);

	if (!Node->IsValid())
	{
		return false;
	}

	const FGuid NodeId = Node->GetID();

	// We expect an output node to have exactly one output
	if (!ensure(Node->GetNumOutputs() == 1))
	{
		return false;
	}

	const FName TypeName = Node->GetOutputs()[0]->GetDataType();

	// Make the analyzer address
	Metasound::Frontend::FAnalyzerAddress AnalyzerAddress;
	AnalyzerAddress.DataType = TypeName;
	AnalyzerAddress.InstanceID = AudioComponent->GetAudioComponentID();
	AnalyzerAddress.OutputName = OutputName;

	// If no analyzer name was provided, try to find a passthrough analyzer
	if (AnalyzerName.IsNone())
	{
		if (!PassthroughAnalyzers.Contains(TypeName))
		{
			return false;
		}

		AnalyzerName = PassthroughAnalyzers[TypeName].AnalyzerName;
		AnalyzerOutputName = PassthroughAnalyzers[TypeName].OutputName;
	}
	AnalyzerAddress.AnalyzerName = AnalyzerName;
	AnalyzerAddress.AnalyzerMemberName = AnalyzerOutputName;
	
	AnalyzerAddress.AnalyzerInstanceID = FGuid::NewGuid();
	AnalyzerAddress.NodeID = NodeId;

	// Check to see if the analyzer exists
	{
		using namespace Metasound::Frontend;
		const IVertexAnalyzerFactory* Factory =
			IVertexAnalyzerRegistry::Get().FindAnalyzerFactory(AnalyzerAddress.AnalyzerName);
		if (nullptr == Factory)
		{
			return false;
		}
	}

	// if we already have a generator, go ahead and make the analyzer
	if (const TSharedPtr<Metasound::FMetasoundGenerator> PinnedGenerator = PinGenerator())
	{
		CreateAnalyzer(AnalyzerAddress, PinnedGenerator);
	}

	// Create the listener
	CreateListener(AnalyzerAddress, OnOutputValueChanged);

	return true;
}

void UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(FName TypeName, FName AnalyzerName, FName OutputName)
{
	check(!PassthroughAnalyzers.Contains(TypeName));
	PassthroughAnalyzers.Add(TypeName, { AnalyzerName, OutputName });
}

void UMetasoundGeneratorHandle::UpdateWatchers()
{
	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundGeneratorHandle::UpdateWatchers);

	while (TOptional<FOutputPayload> ChangedOutput = ChangedOutputs->Dequeue())
	{
		if (const FOutputListener* OutputListener = OutputListeners.FindByPredicate(
			[&ChangedOutput](const FOutputListener& ExistingListener)
			{
				return (*ChangedOutput).AnalyzerName == ExistingListener.AnalyzerAddress.AnalyzerName
				&& (*ChangedOutput).OutputName == ExistingListener.AnalyzerAddress.OutputName
				&& (*ChangedOutput).OutputValue.Name == ExistingListener.AnalyzerAddress.AnalyzerMemberName;
			}))
		{
			OutputListener->OnOutputValueChanged.Broadcast((*ChangedOutput).OutputName, (*ChangedOutput).OutputValue);
		}
	}
}

void UMetasoundGeneratorHandle::OnSourceCreatedAGenerator(uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator)
{
	check(IsInGameThread());

	if (!AudioComponent.IsValid())
	{
		return;
	}
	
	if (InAudioComponentId == AudioComponent->GetAudioComponentID())
	{
		CachedGeneratorPtr = InGenerator;
		if (InGenerator)
		{
			// If there is a parameter pack to apply, apply it
			if (CachedParameterPack)
			{
				InGenerator->QueueParameterPack(CachedParameterPack);
			}

			// If we have listeners, add the analyzers for them
			{
				for (const FOutputListener& Listener : OutputListeners)
				{
					CreateAnalyzer(Listener.AnalyzerAddress, InGenerator);
				}
			}
			
			OnGeneratorHandleAttached.Broadcast();
			// If anyone has told us they are interested in being notified when a generator's 
			// graph has changed go ahead and set that up now...
			AttachGraphChangedDelegate();
		}
	}
}

void UMetasoundGeneratorHandle::OnSourceDestroyedAGenerator(uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator)
{
	check(IsInGameThread());

	if (!AudioComponent.IsValid())
	{
		return;
	}
	
	if (InAudioComponentId == AudioComponent->GetAudioComponentID())
	{
		if (CachedGeneratorPtr.IsValid())
		{
			OnGeneratorHandleDetached.Broadcast();
		}
		CachedGeneratorPtr = nullptr;

		OutputChangedDelegateHandle.Reset();
	}
}

void UMetasoundGeneratorHandle::CreateAnalyzer(
	const Metasound::Frontend::FAnalyzerAddress& AnalyzerAddress,
	const TSharedPtr<Metasound::FMetasoundGenerator> Generator)
{
	if (!IsValid())
	{
		return;
	}

	// Create the analyzer (will skip if there's already one)
	Generator->AddOutputVertexAnalyzer(AnalyzerAddress);

	// Subscribe for output updates if we haven't already
	if (!OutputChangedDelegateHandle.IsValid())
	{
		TWeakPtr<TSpscQueue<FOutputPayload>> ChangedOutputQueue{ ChangedOutputs };
		OutputChangedDelegateHandle = Generator->OnOutputChanged.AddLambda([ChangedOutputQueue](
			FName AnalyzerName,
			FName OutputName,
			FName AnalyzerOutputName,
			TSharedPtr<Metasound::IOutputStorage> OutputData)
		{
			if (const TSharedPtr<TSpscQueue<FOutputPayload>> PinnedQueue = ChangedOutputQueue.Pin())
			{
				PinnedQueue->Enqueue(AnalyzerName, OutputName, AnalyzerOutputName, OutputData);
			}
		});
	}
}

void UMetasoundGeneratorHandle::CreateListener(
	const Metasound::Frontend::FAnalyzerAddress& AnalyzerAddress,
	const FOnMetasoundOutputValueChanged& OnOutputValueChanged)
{
	if (!IsValid())
	{
		return;
	}
	
	// If we already have a listener for this output, just add the delegate to that one
	if (FOutputListener* Listener = OutputListeners.FindByPredicate(
		[&AnalyzerAddress](const FOutputListener& ExistingListener)
		{
			return AnalyzerAddressesReferToSameGeneratorOutput(AnalyzerAddress, ExistingListener.AnalyzerAddress);
		}))
	{
		Listener->OnOutputValueChanged.AddUnique(OnOutputValueChanged);
	}
	// Otherwise add a new listener
	else
	{
		OutputListeners.Emplace(AnalyzerAddress, OnOutputValueChanged);
	}
}
