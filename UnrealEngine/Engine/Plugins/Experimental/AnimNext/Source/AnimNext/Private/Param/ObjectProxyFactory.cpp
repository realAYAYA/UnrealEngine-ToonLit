// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ObjectProxyFactory.h"

#include "AnimNextConfig.h"
#include "ClassProxy.h"
#include "ObjectAccessor.h"
#include "ObjectProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Logging/StructuredLog.h"
#include "Param/AnimNextObjectAccessorConfig.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Features/IModularFeatures.h"
#include "Engine/World.h"

namespace UE::AnimNext
{

static FObjectProxyFactory GObjectProxyFactory;

void FObjectProxyFactory::Init()
{
	Refresh();
}

void FObjectProxyFactory::Destroy()
{
	GObjectProxyFactory.Reset();

	IModularFeatures::Get().UnregisterModularFeature(IParameterSourceFactory::FeatureName, &GObjectProxyFactory);
}

void FObjectProxyFactory::Reset()
{
	ClassMap.Reset();
	ObjectAccessors.Reset();
	ParameterMap.Reset();
}

void FObjectProxyFactory::Refresh()
{
	IModularFeatures::Get().UnregisterModularFeature(IParameterSourceFactory::FeatureName, &GObjectProxyFactory);

	GObjectProxyFactory.Reset();

	for(const FAnimNextObjectAccessorConfig& AccessorConfig : GetDefault<UAnimNextConfig>()->ExposedClasses)
	{
		UClass* Class = AccessorConfig.Class.LoadSynchronous();
		if(Class && AccessorConfig.AccessorName != NAME_None)
		{
			GObjectProxyFactory.RegisterObjectAccessor(AccessorConfig.AccessorName, Class);
		}
	}

	// Refresh RigVM registry as we have updated allowed types
	FRigVMRegistry::Get().RefreshEngineTypes();

	IModularFeatures::Get().RegisterModularFeature(IParameterSourceFactory::FeatureName, &GObjectProxyFactory);
}

void FObjectProxyFactory::ForEachSource(TFunctionRef<void(FName)> InFunction) const
{
	for(const TPair<FName, TSharedPtr<FObjectAccessor>>& ObjectAccessorPair : ObjectAccessors)
	{
		InFunction(ObjectAccessorPair.Key);
	}
}

TUniquePtr<IParameterSource> FObjectProxyFactory::CreateParameterSource(const FExternalParameterContext& InContext, FName InSourceName, TConstArrayView<FName> InRequiredParameters) const
{
	using namespace UE::AnimNext;

	if(TSharedPtr<FObjectAccessor> ObjectAccessor = FindObjectAccessor(InSourceName))
	{
		if(UObject* Object = ObjectAccessor->Function(InContext))
		{
			TUniquePtr<FObjectProxy> ObjectProxy = MakeUnique<FObjectProxy>(Object, ObjectAccessor.ToSharedRef());
			ObjectProxy->RequestParameterCache(InRequiredParameters);

			return MoveTemp(ObjectProxy);
		}
	}
	return nullptr;
}

#if WITH_EDITOR

bool FObjectProxyFactory::FindParameterInfo(FName InParameterName, FParameterInfo& OutInfo) const
{
	if(const TWeakPtr<FObjectAccessor>* WeakAccessor = ParameterMap.Find(InParameterName))
	{
		if(TSharedPtr<FObjectAccessor> ObjectAccessor = WeakAccessor->Pin())
		{
			if(int32* ParamIndexPtr = ObjectAccessor->RemappedParametersMap.Find(InParameterName))
			{
				OutInfo.Type = ObjectAccessor->ClassProxy->Parameters[*ParamIndexPtr].Type;
				OutInfo.Tooltip = ObjectAccessor->ClassProxy->Parameters[*ParamIndexPtr].Tooltip;
				OutInfo.bThreadSafe = ObjectAccessor->ClassProxy->Parameters[*ParamIndexPtr].bThreadSafe;
				return true;
			}
		}
	}
	return false;
}

void FObjectProxyFactory::ForEachParameter(FName InSourceName, TFunctionRef<void(FName, const FParameterInfo&)> InFunction) const
{
	if(TSharedPtr<FObjectAccessor> ObjectAccessor = FindObjectAccessor(InSourceName))
	{
		TSharedPtr<FClassProxy> ClassProxy = ObjectAccessor->ClassProxy;
		check(ClassProxy->Parameters.Num() == ObjectAccessor->RemappedParameters.Num());
		for(int32 ParameterIndex = 0, ParameterCount = ObjectAccessor->RemappedParameters.Num(); ParameterIndex < ParameterCount; ++ParameterIndex)
		{
			FParameterInfo ParameterInfo;
			ParameterInfo.Type = ClassProxy->Parameters[ParameterIndex].Type;
			ParameterInfo.Tooltip = ClassProxy->Parameters[ParameterIndex].Tooltip;
			ParameterInfo.bThreadSafe = ClassProxy->Parameters[ParameterIndex].bThreadSafe;
			InFunction(ObjectAccessor->RemappedParameters[ParameterIndex], ParameterInfo);
		}
	}
}

#endif

void FObjectProxyFactory::RegisterObjectAccessor(FName InAccessorName, const UClass* InTargetClass)
{
	check(IsInGameThread());
	check(InAccessorName != NAME_None);
	check(InTargetClass != nullptr);

	if(InTargetClass->IsChildOf<UActorComponent>())
	{
		RegisterActorComponentAccessor(InAccessorName, const_cast<UClass*>(InTargetClass));
	}
	else if(InTargetClass->IsChildOf<AActor>())
	{
		RegisterActorAccessor(InAccessorName, const_cast<UClass*>(InTargetClass));
	}
	else if(InTargetClass->IsChildOf<UWorld>())
	{
		RegisterObjectAccessor(InAccessorName, const_cast<UClass*>(InTargetClass), [](const FExternalParameterContext& InContext) -> UObject*
		{
			return InContext.Object->GetWorld();
		});
	}
	else 
	{
		UE_LOGFMT(LogAnimation, Warning, "Class {ClassName} passed to FObjectProxyFactory::RegisterObjectAccessor that does not have a built-in accessor, please use the custom accessor overload.", InTargetClass->GetFName());
	}
}

void FObjectProxyFactory::RegisterActorAccessor(FName InAccessorName, TSubclassOf<AActor> InTargetClass)
{
	RegisterObjectAccessor(InAccessorName, InTargetClass.Get(), [InTargetClass](const FExternalParameterContext& InContext) -> UObject*
	{
		if(UActorComponent* ContextComponent = Cast<UActorComponent>(InContext.Object))
		{
			if(ContextComponent->GetOwner()->GetClass()->IsChildOf(InTargetClass.Get()))
			{
				return ContextComponent->GetOwner();
			}
		}

		return nullptr;
	});
}

void FObjectProxyFactory::RegisterActorComponentAccessor(FName InAccessorName, TSubclassOf<UActorComponent> InTargetClass)
{
	RegisterObjectAccessor(InAccessorName, InTargetClass.Get(), [InTargetClass](const FExternalParameterContext& InContext) -> UObject*
	{
		if(UActorComponent* ContextComponent = Cast<UActorComponent>(InContext.Object))
		{
			return ContextComponent->GetOwner()->FindComponentByClass(InTargetClass.Get());
		}

		return nullptr;
	});
}

void FObjectProxyFactory::RegisterObjectAccessor(FName InAccessorName, const UClass* InTargetClass, FObjectAccessorFunction&& InFunction)
{
	check(IsInGameThread());
	check(InAccessorName != NAME_None);
	check(InTargetClass != nullptr);

	// Ensure we can use the type as a pin in RigVM graphs 
	FRigVMRegistry::Get().RegisterObjectTypes( {{ const_cast<UClass*>(InTargetClass), FRigVMRegistry::ERegisterObjectOperation::Class } });

	UE_MT_SCOPED_WRITE_ACCESS(ObjectAccessorsAccessDetector);
	if(!ObjectAccessors.Contains(InAccessorName))
	{
		TSharedRef<FClassProxy> ClassProxy = FindOrCreateClassProxy(InTargetClass);
		TSharedRef<FObjectAccessor> ObjectAccessor = MakeShared<FObjectAccessor>(InAccessorName, MoveTemp(InFunction), ClassProxy);
		ObjectAccessors.Add(InAccessorName, ObjectAccessor);

		// Add object accessor to parameter lookup
		ParameterMap.Add(InAccessorName, ObjectAccessor);

		// Add sub-parameters to parameter lookup
		for(FName RemappedParameter : ObjectAccessor->RemappedParameters)
		{
			ParameterMap.Add(RemappedParameter, ObjectAccessor);
		}
	}
}

void FObjectProxyFactory::UnregisterObjectAccessor(FName InAccessorName)
{
	check(IsInGameThread());

	UE_MT_SCOPED_WRITE_ACCESS(ObjectAccessorsAccessDetector);
	if(const TSharedPtr<FObjectAccessor>* FoundAccessor = ObjectAccessors.Find(InAccessorName))
	{
		// Remove sub-parameters
		for(FName RemappedParameter : (*FoundAccessor)->RemappedParameters)
		{
			ParameterMap.Remove(RemappedParameter);
		}

		// Remove object accessor
		ParameterMap.Remove(InAccessorName);

		// Remove accessor itself
		ObjectAccessors.Remove(InAccessorName);
	}
}

TSharedRef<FClassProxy> FObjectProxyFactory::FindOrCreateClassProxy(const UClass* InClass)
{
	UE_MT_SCOPED_WRITE_ACCESS(ObjectAccessorsAccessDetector);
	if(TSharedPtr<FClassProxy>* FoundProxy = ClassMap.Find(InClass))
	{
		return FoundProxy->ToSharedRef();
	}

	return ClassMap.Add(InClass, MakeShared<FClassProxy>(InClass)).ToSharedRef();
}

TSharedPtr<FObjectAccessor> FObjectProxyFactory::FindObjectAccessor(FName InAccessorName) const
{
	UE_MT_SCOPED_READ_ACCESS(ObjectAccessorsAccessDetector);
	return ObjectAccessors.FindRef(InAccessorName);
}

}
