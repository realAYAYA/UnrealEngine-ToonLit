// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PhysicalMaterials.h"
#include "HAL/LowLevelMemTracker.h"

namespace Chaos
{
	FChaosPhysicsMaterial* FMaterialHandle::Get() const
	{
		if(InnerHandle.IsValid())
		{
			return FPhysicalMaterialManager::Get().Resolve(InnerHandle);
		}
		return nullptr;
	}

	const FChaosPhysicsMaterial* FConstMaterialHandle::Get() const
	{
		if(InnerHandle.IsValid())
		{
			return FPhysicalMaterialManager::Get().Resolve(InnerHandle);
		}
		return nullptr;
	}

	FChaosPhysicsMaterialMask* FMaterialMaskHandle::Get() const
	{
		if (InnerHandle.IsValid())
		{
			return FPhysicalMaterialManager::Get().Resolve(InnerHandle);
		}
		return nullptr;
	}

	const FChaosPhysicsMaterialMask* FConstMaterialMaskHandle::Get() const
	{
		if (InnerHandle.IsValid())
		{
			return FPhysicalMaterialManager::Get().Resolve(InnerHandle);
		}
		return nullptr;
	}

	FPhysicalMaterialManager::FPhysicalMaterialManager()
		: Materials(InitialCapacity)
		, MaterialMasks()
	{

	}

	FPhysicalMaterialManager& FPhysicalMaterialManager::Get()
	{
		static FPhysicalMaterialManager Instance;
		return Instance;
	}

	FChaosPhysicsMaterial* FPhysicalMaterialManager::Resolve(FChaosMaterialHandle InHandle) const
	{
		return Materials.Get(InHandle);
	}

	const FChaosPhysicsMaterial* FPhysicalMaterialManager::Resolve(FChaosConstMaterialHandle InHandle) const
	{
		return Materials.Get(InHandle);
	}

	FChaosPhysicsMaterialMask* FPhysicalMaterialManager::Resolve(FChaosMaterialMaskHandle InHandle) const
	{
		return MaterialMasks.Get(InHandle);
	}

	const FChaosPhysicsMaterialMask* FPhysicalMaterialManager::Resolve(FChaosConstMaterialMaskHandle InHandle) const
	{
		return MaterialMasks.Get(InHandle);
	}
	
	void FPhysicalMaterialManager::UpdateMaterial(FMaterialHandle InHandle)
	{
		check(IsInGameThread());

		OnMaterialUpdated.Broadcast(InHandle);
	}

	void FPhysicalMaterialManager::UpdateMaterialMask(FMaterialMaskHandle InHandle)
	{
		check(IsInGameThread());

		OnMaterialMaskUpdated.Broadcast(InHandle);
	}

	const Chaos::THandleArray<FChaosPhysicsMaterial>& FPhysicalMaterialManager::GetMasterMaterials_External() const
	{
		return GetPrimaryMaterials_External();
	}

	const Chaos::THandleArray<FChaosPhysicsMaterialMask>& FPhysicalMaterialManager::GetMasterMaterialMasks_External() const
	{
		return GetPrimaryMaterialMasks_External();
	}
	
	const Chaos::THandleArray<FChaosPhysicsMaterial>& FPhysicalMaterialManager::GetPrimaryMaterials_External() const
	{
		return Materials;
	}

	const Chaos::THandleArray<FChaosPhysicsMaterialMask>& FPhysicalMaterialManager::GetPrimaryMaterialMasks_External() const
	{
		return MaterialMasks;
	}

	FMaterialHandle FPhysicalMaterialManager::Create()
	{
		LLM_SCOPE(ELLMTag::ChaosMaterial);

		check(IsInGameThread());
		FMaterialHandle OutHandle;
		OutHandle.InnerHandle = Materials.Create();

		OnMaterialCreated.Broadcast(OutHandle);

		return OutHandle;
	}

	FMaterialMaskHandle FPhysicalMaterialManager::CreateMask()
	{
		check(IsInGameThread());
		FMaterialMaskHandle OutHandle;
		OutHandle.InnerHandle = MaterialMasks.Create();

		OnMaterialMaskCreated.Broadcast(OutHandle);

		return OutHandle;
	}

	void FPhysicalMaterialManager::Destroy(FMaterialHandle InHandle)
	{
		LLM_SCOPE(ELLMTag::ChaosMaterial);

		check(IsInGameThread());
		if(InHandle.InnerHandle.IsValid())
		{
			OnMaterialDestroyed.Broadcast(InHandle);

			Materials.Destroy(InHandle.InnerHandle);
		}
	}

	void FPhysicalMaterialManager::Destroy(FMaterialMaskHandle InHandle)
	{
		check(IsInGameThread());
		if (InHandle.InnerHandle.IsValid())
		{
			OnMaterialMaskDestroyed.Broadcast(InHandle);

			MaterialMasks.Destroy(InHandle.InnerHandle);
		}
	}
}