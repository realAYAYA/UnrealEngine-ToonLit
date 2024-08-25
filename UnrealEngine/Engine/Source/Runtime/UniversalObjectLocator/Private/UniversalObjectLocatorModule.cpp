// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubObjectLocator.h"
#include "Modules/ModuleManager.h"
#include "IUniversalObjectLocatorModule.h"
#include "UniversalObjectLocatorFragment.h"
#include "UniversalObjectLocatorRegistry.h"
#include "UniversalObjectLocatorParameterTypeHandle.h"
//#include "Modules/VisualizerDebuggingState.h"
#include "Containers/Ticker.h"
#include "Misc/DelayedAutoRegister.h"

#include "DirectPathObjectLocator.h"
#include "ILocatorSpawnedCache.h"

namespace UE::UniversalObjectLocator
{

class FUniversalObjectLocatorModule
	: public IUniversalObjectLocatorModule
{
public:
	
	void StartupModule() override
	{
		// Register fragment types as soon as the object system is ready
		FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::ObjectSystemReady,
			[this]
			{
				{
					FFragmentTypeParameters FragmentTypeParams("subobj", NSLOCTEXT("SubObjectLocator", "Object", "Object"));
					FragmentTypeParams.PrimaryEditorType = "SubObject";
					FSubObjectLocator::FragmentType = this->RegisterFragmentType<FSubObjectLocator>(FragmentTypeParams);
				}

				{
					FFragmentTypeParameters FragmentTypeParams("uobj", NSLOCTEXT("DirectPathObjectLocator", "Object", "Object"));
					FDirectPathObjectLocator::FragmentType = this->RegisterFragmentType<FDirectPathObjectLocator>(FragmentTypeParams);
				}

				FLocatorSpawnedCacheResolveParameter::ParameterType = this->RegisterParameterType<FLocatorSpawnedCacheResolveParameter>();
			}
		);

		TickerDelegate = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FUniversalObjectLocatorModule::PurgeVisualizers), 60.f);
	}

	void ShutdownModule() override
	{

	}

	FFragmentTypeHandle RegisterFragmentTypeImpl(const FFragmentType& FragmentType) override
	{
		FRegistry& Registry = FRegistry::Get();

		const int32 Index = Registry.FragmentTypes.Num();
		Registry.FragmentTypes.Add(FragmentType);
		checkf(Index < static_cast<int32>(std::numeric_limits<uint8>::max()), TEXT("Maximum number of UOL FragmentTypes reached"));

		// @todo: enable this code once visualizer debugging state is enabled
		// Re-assign the debugging ptr in case it changed
		// UE::Core::FVisualizerDebuggingState::Assign("UOL", Registry.FragmentTypes.GetData());

		return FFragmentTypeHandle(static_cast<uint8>(Index));
	}

	void UnregisterFragmentTypeImpl(FFragmentTypeHandle FragmentType) override
	{
		FRegistry::Get().FragmentTypes[FragmentType.GetIndex()] = FFragmentType{};
	}

	FParameterTypeHandle RegisterParameterTypeImpl(UScriptStruct* Struct)
	{
		FRegistry& Registry = FRegistry::Get();

		const int32 Index = Registry.ParameterTypes.Num();
		Registry.ParameterTypes.Add(Struct);

		checkf(Index < FResolveParameterBuffer::MaxNumParameters, TEXT("Maximum number of UOL ParameterTypes reached"));

		return FParameterTypeHandle(static_cast<uint8>(Index));
	}

	void UnregisterParameterTypeImpl(FParameterTypeHandle ParameterType)
	{
		FRegistry& Registry = FRegistry::Get();
		check(ParameterType.IsValid());
		Registry.ParameterTypes[ParameterType.GetIndex()] = nullptr;
	}

	bool PurgeVisualizers(float) const
	{
		for (FFragmentType& FragmentType : FRegistry::Get().FragmentTypes)
		{
			if (FragmentType.DebuggingAssistant)
			{
				FragmentType.DebuggingAssistant->Purge();
			}
		}
		return true;
	}

	FTSTicker::FDelegateHandle TickerDelegate;
};

} // namespace UE::UniversalObjectLocator

IMPLEMENT_MODULE(UE::UniversalObjectLocator::FUniversalObjectLocatorModule, UniversalObjectLocator);

