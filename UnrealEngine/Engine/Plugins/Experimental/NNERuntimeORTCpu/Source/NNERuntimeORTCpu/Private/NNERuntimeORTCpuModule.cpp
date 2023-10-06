// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTCpuModule.h"
#include "NNE.h"
#include "NNERuntimeORTCpu.h"
#include "UObject/WeakInterfacePtr.h"

void FNNERuntimeORTCpuModule::StartupModule()
{
	// NNE runtime startup
	NNERuntimeORTCpu = NewObject<UNNERuntimeORTCpuImpl>();
	if (NNERuntimeORTCpu.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeORTCpu.Get());
		
		NNERuntimeORTCpu->AddToRoot();
		UE::NNE::RegisterRuntime(RuntimeCPUInterface);
	}
}

void FNNERuntimeORTCpuModule::ShutdownModule()
{
	// NNE runtime shutdown
	if (NNERuntimeORTCpu.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeORTCpu.Get());
		
		UE::NNE::UnregisterRuntime(RuntimeCPUInterface);
		NNERuntimeORTCpu->RemoveFromRoot();
		NNERuntimeORTCpu = TWeakObjectPtr<UNNERuntimeORTCpuImpl>(nullptr);
	}
}

IMPLEMENT_MODULE(FNNERuntimeORTCpuModule, NNERuntimeORTCpu);
