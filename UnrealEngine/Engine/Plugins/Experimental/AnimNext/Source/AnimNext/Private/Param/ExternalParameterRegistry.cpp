// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ExternalParameterRegistry.h"
#include "Param/ObjectAccessor.h"
#include "Param/ObjectProxy.h"
#include "Logging/StructuredLog.h"
#include "Param/IParameterSource.h"
#include "Param/IParameterSourceFactory.h"
#include "UObject/ObjectKey.h"
#include "UObject/Package.h"
#include "Features/IModularFeatures.h"
#include "EngineLogs.h"

namespace UE::AnimNext
{

struct FExternalParameterRegistryImpl
{
	void ResetFactories()
	{
		Factories.Reset();
		SourceFactoryMap.Reset();
	}

	void RefreshFactories()
	{
		ResetFactories();

		TArray<const IParameterSourceFactory*> LocalFactories = IModularFeatures::Get().GetModularFeatureImplementations<const IParameterSourceFactory>(IParameterSourceFactory::FeatureName);
		for(const IParameterSourceFactory* Factory : LocalFactories)
		{
			AddFactory(Factory);
		}
	}

	void AddFactory(const IParameterSourceFactory* InFactory)
	{
		Factories.Add(InFactory);
		InFactory->ForEachSource([this, InFactory](FName InSourceName)
		{
			// Skip duplicate names
			if(SourceFactoryMap.Contains(InSourceName))
			{
				UE_LOGFMT(LogAnimation, Error, "Duplicate external parameter source found: {Name}", InSourceName);
			}
			else
			{
				SourceFactoryMap.Add(InSourceName, InFactory);
			}

#if WITH_EDITOR
			InFactory->ForEachParameter(InSourceName, [this, InFactory, InSourceName](FName InParameterName, const IParameterSourceFactory::FParameterInfo& InInfo)
			{
				ParameterSourceMap.Add(InParameterName, InSourceName);
			});
#endif
		});
	}

	void RemoveFactory(const IParameterSourceFactory* InFactory)
	{
		Factories.Remove(InFactory);
		for(auto It = SourceFactoryMap.CreateIterator(); It; ++It)
		{
			if(It->Value == InFactory)
			{
				It.RemoveCurrent();
			}
		}
#if WITH_EDITOR
		InFactory->ForEachSource([this, InFactory](FName InSourceName)
		{
			InFactory->ForEachParameter(InSourceName, [this](FName InParameterName, const IParameterSourceFactory::FParameterInfo& InInfo)
			{
				ParameterSourceMap.Remove(InParameterName);
			});
		});
#endif
	}

	// All known factories
	TArray<const IParameterSourceFactory*> Factories;

	// Map of source name -> factory
	TMap<FName, const IParameterSourceFactory*> SourceFactoryMap;

#if WITH_EDITOR
	// Map of parameter name -> parameter source
	TMap<FName, FName> ParameterSourceMap;
#endif

	FDelegateHandle OnModularFeatureRegisteredHandle;
	FDelegateHandle OnModularFeatureUnregisteredHandle;
};

static FExternalParameterRegistryImpl GExternalParameterRegistry;

void FExternalParameterRegistry::Init()
{
	GExternalParameterRegistry.OnModularFeatureRegisteredHandle = IModularFeatures::Get().OnModularFeatureRegistered().AddLambda([](const FName& InType, IModularFeature* InModularFeature)
	{
		if(InType == IParameterSourceFactory::FeatureName)
		{
			GExternalParameterRegistry.AddFactory(static_cast<IParameterSourceFactory*>(InModularFeature));
		}
	});
	GExternalParameterRegistry.OnModularFeatureUnregisteredHandle = IModularFeatures::Get().OnModularFeatureRegistered().AddLambda([](const FName& InType, IModularFeature* InModularFeature)
	{
		if(InType == IParameterSourceFactory::FeatureName)
		{
			GExternalParameterRegistry.RemoveFactory(static_cast<IParameterSourceFactory*>(InModularFeature));
		}
	});
	GExternalParameterRegistry.RefreshFactories();
}

void FExternalParameterRegistry::Destroy()
{
	IModularFeatures::Get().OnModularFeatureRegistered().Remove(GExternalParameterRegistry.OnModularFeatureRegisteredHandle);
	IModularFeatures::Get().OnModularFeatureUnregistered().Remove(GExternalParameterRegistry.OnModularFeatureUnregisteredHandle);
	GExternalParameterRegistry.ResetFactories();
}

TUniquePtr<IParameterSource> FExternalParameterRegistry::CreateParameterSource(const FExternalParameterContext& InContext, FName InSourceName, TConstArrayView<FName> InRequiredParameters)
{
	using namespace UE::AnimNext;

	if(const IParameterSourceFactory** FoundFactoryPtr = GExternalParameterRegistry.SourceFactoryMap.Find(InSourceName))
	{
		return (*FoundFactoryPtr)->CreateParameterSource(InContext, InSourceName, InRequiredParameters);
	}
	return nullptr;
}

#if WITH_EDITOR

bool FExternalParameterRegistry::FindParameterInfo(FName InParameterName, IParameterSourceFactory::FParameterInfo& OutInfo)
{
	if(const FName* SourceNamePtr = GExternalParameterRegistry.ParameterSourceMap.Find(InParameterName))
	{
		if(const IParameterSourceFactory** Factory = GExternalParameterRegistry.SourceFactoryMap.Find(*SourceNamePtr))
		{
			return (*Factory)->FindParameterInfo(InParameterName, OutInfo);
		}
	}
	return false;
}

void FExternalParameterRegistry::ForEachParameter(TFunctionRef<void(FName, const IParameterSourceFactory::FParameterInfo&)> InFunction)
{
	for(const IParameterSourceFactory* Factory : GExternalParameterRegistry.Factories)
	{
		Factory->ForEachSource([&InFunction, Factory](FName InSourceName)
		{
			Factory->ForEachParameter(InSourceName, InFunction);
		});
	}
}

FName FExternalParameterRegistry::FindSourceForParameter(FName InParameterName)
{
	return GExternalParameterRegistry.ParameterSourceMap.FindRef(InParameterName);
}

#endif

}
