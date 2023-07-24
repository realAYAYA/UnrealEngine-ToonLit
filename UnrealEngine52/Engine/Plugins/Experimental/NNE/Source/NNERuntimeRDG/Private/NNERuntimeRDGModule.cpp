// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGModule.h"
#include "NNECore.h"
#include "NNERuntimeRDGDml.h"
#include "NNERuntimeRDGHlsl.h"
#include "UObject/WeakInterfacePtr.h"


void FNNERuntimeRDGModule::StartupModule()
{
	// NNE runtime ORT Cpu startup
	NNERuntimeRDGHlsl = NewObject<UNNERuntimeRDGHlslImpl>();
	if (NNERuntimeRDGHlsl.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeRDGHlsl.Get());

		NNERuntimeRDGHlsl->Init();
		NNERuntimeRDGHlsl->AddToRoot();
		UE::NNECore::RegisterRuntime(RuntimeCPUInterface);
	}

#ifdef NNE_USE_DIRECTML
	// NNE runtime ORT Dml startup
	NNERuntimeRDGDml = NewObject<UNNERuntimeRDGDmlImpl>();
	if (NNERuntimeRDGDml.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeRDGDml.Get());
		
		bool bRegisterOnlyOperators = UE::NNERuntimeRDG::Private::Dml::FRuntimeDmlStartup();
		NNERuntimeRDGDml->Init(bRegisterOnlyOperators);
		NNERuntimeRDGDml->AddToRoot();
		UE::NNECore::RegisterRuntime(RuntimeDmlInterface);
	}
#endif
}

void FNNERuntimeRDGModule::ShutdownModule()
{
	// NNE runtime ORT Cpu shutdown
	if (NNERuntimeRDGHlsl.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeRDGHlsl.Get());

		UE::NNECore::UnregisterRuntime(RuntimeCPUInterface);
		NNERuntimeRDGHlsl->RemoveFromRoot();
		NNERuntimeRDGHlsl = TWeakObjectPtr<UNNERuntimeRDGHlslImpl>(nullptr);
	}

#ifdef NNE_USE_DIRECTML
	// NNE runtime ORT Dml shutdown
	if (NNERuntimeRDGDml.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeRDGDml.Get());

		UE::NNECore::UnregisterRuntime(RuntimeDmlInterface);
		NNERuntimeRDGDml->RemoveFromRoot();
		NNERuntimeRDGDml = TWeakObjectPtr<UNNERuntimeRDGDmlImpl>(nullptr);
	}
#endif

}

IMPLEMENT_MODULE(FNNERuntimeRDGModule, NNERuntimeRDG);
