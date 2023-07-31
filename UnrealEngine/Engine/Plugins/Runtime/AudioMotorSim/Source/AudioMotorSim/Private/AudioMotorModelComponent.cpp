// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMotorModelComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioMotorModelComponent)

void UAudioMotorModelComponent::Update(FAudioMotorSimInputContext Input)
{
	for(FMotorSimEntry& Entry : SimComponents)
	{
		if(Entry.Sim)
		{
			Entry.Sim->Update(Input, CachedRuntimeContext);
		}
	}

	for(TScriptInterface<IAudioMotorSimOutput> Component : AudioComponents)
	{
		if(Component)
		{
			Component->Update(Input, CachedRuntimeContext);
		}
	}
	
	CachedInputContext = Input;
}
	
void UAudioMotorModelComponent::Reset()
{
	for(FMotorSimEntry& Entry : SimComponents)
	{
		if(Entry.Sim)
		{
			Entry.Sim->Reset();
		}
	}
}

void UAudioMotorModelComponent::StartOutput()
{
	for(TScriptInterface<IAudioMotorSimOutput> Component : AudioComponents)
	{
		if(Component)
		{
			Component->StartOutput();
		}
	}
}
	
void UAudioMotorModelComponent::StopOutput()
{
	for(TScriptInterface<IAudioMotorSimOutput> Component : AudioComponents)
	{
		if(Component)
		{
			Component->StopOutput();
		}
	}
}

void UAudioMotorModelComponent::AddMotorAudioComponent(TScriptInterface<IAudioMotorSimOutput> InComponent)
{
	if(InComponent)
	{
		AudioComponents.Add(InComponent);
	}
}
	
void UAudioMotorModelComponent::RemoveMotorAudioComponent(TScriptInterface<IAudioMotorSimOutput> InComponent)
{
	if(InComponent)
	{
		AudioComponents.Remove(InComponent);
	}
}
	
void UAudioMotorModelComponent::AddMotorSimComponent(TScriptInterface<IAudioMotorSim> InComponent, const int32 SortOrder)
{
	const FMotorSimEntry NewEntry = {InComponent, SortOrder};
	
	for(auto SimIt = SimComponents.CreateIterator(); SimIt; ++SimIt)
	{
		if(SimIt->SortOrder > SortOrder)
		{
			const int32 Index = SimIt.GetIndex();
			if(SimIt.GetIndex() == 0)
			{
				SimComponents.Insert(NewEntry, 0);
				return;
			}
			
			SimComponents.Insert(NewEntry, Index);
			return;
		}
	}
	
	SimComponents.Add(NewEntry);
}
	
void UAudioMotorModelComponent::RemoveMotorSimComponent(TScriptInterface<IAudioMotorSim> InComponent)
{
	for(auto SimIt = SimComponents.CreateIterator(); SimIt; ++SimIt)
	{
		if(SimIt->Sim == InComponent)
		{
			SimIt.RemoveCurrent();
			return;
		}
	}
}
