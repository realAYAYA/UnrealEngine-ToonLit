// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDModule.h"
#include "Containers/Array.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"

class FChaosVDMeshComponentPool : public FGCObject
{
public:
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	
	virtual FString GetReferencerName() const override
	{
		return TEXT("FChaosVDMeshComponentPool");
	}

	template<typename TMeshComponent>
	TMeshComponent* AcquireMeshComponent(UObject* Outer, FName Name);

	void DisposeMeshComponent(UMeshComponent* MeshComponent);

private:

	template <typename TDesiredMeshComponent, typename TPooledComponent>
	TDesiredMeshComponent* GetMeshComponentFromPool_Internal(TArray<TObjectPtr<TPooledComponent>>& InMeshComponentPool, UObject* Outer, FName Name);

	void ResetMeshComponent(UMeshComponent* MeshComponent);

	TArray<TObjectPtr<UStaticMeshComponent>> PooledStaticMeshComponent;
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PooledInstancedStaticMeshComponent;
	TArray<TObjectPtr<UDynamicMeshComponent>> PooledDynamicMeshComponent;

	static bool bUseComponentsPool;
	static FAutoConsoleVariableRef CVarUseComponentsPool;
};

template <typename TMeshComponent>
TMeshComponent* FChaosVDMeshComponentPool::AcquireMeshComponent(UObject* Outer, FName Name)
{
	if constexpr (std::is_base_of_v<UStaticMeshComponent, TMeshComponent> && !std::is_base_of_v<UInstancedStaticMeshComponent, TMeshComponent>)
	{
		return GetMeshComponentFromPool_Internal<TMeshComponent>(PooledStaticMeshComponent, Outer, Name);
	}
	else if constexpr (std::is_base_of_v<UInstancedStaticMeshComponent, TMeshComponent>)
	{
		return GetMeshComponentFromPool_Internal<TMeshComponent>(PooledInstancedStaticMeshComponent, Outer, Name);
	}
	else if constexpr (std::is_base_of_v<UDynamicMeshComponent, TMeshComponent>)
	{
		return GetMeshComponentFromPool_Internal<TMeshComponent>(PooledDynamicMeshComponent, Outer, Name);
	}
	else
	{
		return nullptr;
	}
}

template <typename TDesiredMeshComponent, typename TPooledComponent>
TDesiredMeshComponent* FChaosVDMeshComponentPool::GetMeshComponentFromPool_Internal(TArray<TObjectPtr<TPooledComponent>>& InMeshComponentPool, UObject* Outer,  FName Name)
{
	static_assert(std::is_base_of_v<UStaticMeshComponent, TDesiredMeshComponent> || std::is_base_of_v<UInstancedStaticMeshComponent, TDesiredMeshComponent> || std::is_base_of_v<TDesiredMeshComponent, UDynamicMeshComponent>, "GetMeshComponentInternal Only supports DynamicMeshComponent, Static MeshComponent and Instanced Static Mesh Component");

	// We need to ensure unique names
	const FString NewName = Name.ToString() + FGuid::NewGuid().ToString();
	
	if (bUseComponentsPool &&  InMeshComponentPool.Num() > 0)
	{
		if (TDesiredMeshComponent* Component = Cast<TDesiredMeshComponent>(InMeshComponentPool.Pop()))
		{
			Component->Rename(*NewName , Outer, REN_NonTransactional | REN_DoNotDirty | REN_ForceNoResetLoaders | REN_SkipGeneratedClasses | REN_DontCreateRedirectors);
			return Component;
		}
		else
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Selected Pool has [%d] objects, but none where castable to the desired type"), ANSI_TO_TCHAR(__FUNCTION__), InMeshComponentPool.Num());
		}
	}

	return NewObject<TDesiredMeshComponent>(Outer, *NewName);
}
