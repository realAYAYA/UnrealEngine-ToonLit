// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handling/AvaMaskText3DActorMaterialCollectionHandle.h"

#include "AvaMaskLog.h"
#include "AvaMaskUtilities.h"
#include "AvaObjectHandleSubsystem.h"
#include "AvaText3DComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Handling/AvaHandleUtilities.h"
#include "IAvaMaskMaterialCollectionHandle.h"
#include "IAvaMaskMaterialHandle.h"
#include "MaterialHub/AvaTextMaterialHub.h"
#include "Materials/MaterialInterface.h"
#include "Mesh.h"
#include "Text3DComponent.h"

namespace UE::AvaMask::Private
{
	EAvaTextTranslucency GetTargetTranslucencyType(
		const EAvaTextTranslucency InFromMaterial
		, const EBlendMode InRequired)
	{
		if (InRequired == EBlendMode::BLEND_Opaque)
		{
			return InFromMaterial;
		}
		
		if (InFromMaterial == EAvaTextTranslucency::None)
		{
			return EAvaTextTranslucency::Translucent;
		}

		return InFromMaterial;
	}
}

FAvaMaskText3DActorMaterialCollectionHandle::FAvaMaskText3DActorMaterialCollectionHandle(AActor* InActor)
	: WeakActor(InActor)
	, WeakComponent(InActor->GetComponentByClass<UText3DComponent>())
{
}

TArray<TObjectPtr<UMaterialInterface>> FAvaMaskText3DActorMaterialCollectionHandle::GetMaterials()
{
	TArray<TObjectPtr<UMaterialInterface>> Materials;
	Materials.Reserve(GetNumMaterials());
	
	if (const UText3DComponent* TextComponent = GetComponent())
	{
		Materials.Emplace(TextComponent->GetFrontMaterial());
		Materials.Emplace(TextComponent->GetBevelMaterial());
		Materials.Emplace(TextComponent->GetExtrudeMaterial());
		Materials.Emplace(TextComponent->GetBackMaterial());
	}

	return Materials;
}

TArray<TSharedPtr<IAvaMaskMaterialHandle>> FAvaMaskText3DActorMaterialCollectionHandle::GetMaterialHandles()
{
	if (MaterialHandles.IsEmpty() || !UE::Ava::Internal::IsHandleValid(MaterialHandles.Last()))
	{
		if (const UText3DComponent* TextComponent = GetComponent())
		{
			UAvaObjectHandleSubsystem* HandleSubsystem = UE::Ava::Internal::GetObjectHandleSubsystem();
			if (!HandleSubsystem)
			{
				return MaterialHandles;
			}
			
			MaterialHandles.Init(nullptr, static_cast<int32>(EText3DGroupType::TypeCount));
			MaterialHandles[static_cast<int32>(EText3DGroupType::Front)] = HandleSubsystem->MakeHandle<IAvaMaskMaterialHandle>(TextComponent->GetFrontMaterial(), UE::AvaMask::Internal::HandleTag);
			MaterialHandles[static_cast<int32>(EText3DGroupType::Bevel)] = HandleSubsystem->MakeHandle<IAvaMaskMaterialHandle>(TextComponent->GetBevelMaterial(), UE::AvaMask::Internal::HandleTag);
			MaterialHandles[static_cast<int32>(EText3DGroupType::Extrude)] = HandleSubsystem->MakeHandle<IAvaMaskMaterialHandle>(TextComponent->GetExtrudeMaterial(), UE::AvaMask::Internal::HandleTag);
			MaterialHandles[static_cast<int32>(EText3DGroupType::Back)] = HandleSubsystem->MakeHandle<IAvaMaskMaterialHandle>(TextComponent->GetBackMaterial(), UE::AvaMask::Internal::HandleTag);
		}
	}

	return MaterialHandles;
}

void FAvaMaskText3DActorMaterialCollectionHandle::SetMaterial(
	const FSoftComponentReference& InComponent
	, const int32 InSlotIdx
	, UMaterialInterface* InMaterial)
{
	if (!ensure(InSlotIdx < 4))
	{
		UE_LOG(LogAvaMask, Error, TEXT("SlotIdx out of range, should be from 0-3, was %u"), InSlotIdx);
		return;
	}

	if (UText3DComponent* TextComponent = GetComponent())
	{
		switch (InSlotIdx)
		{
		case 0:
			TextComponent->SetFrontMaterial(InMaterial);

		case 1:
			TextComponent->SetBevelMaterial(InMaterial);

		case 2:
			TextComponent->SetExtrudeMaterial(InMaterial);

		case 3:
			TextComponent->SetBackMaterial(InMaterial);

		default:
			return;
		}
	}
}

void FAvaMaskText3DActorMaterialCollectionHandle::SetMaterials(
	const TArray<TObjectPtr<UMaterialInterface>>& InMaterials
	, const TBitArray<>& InSetToggle)
{
	if (!ensure(InMaterials.Num() == GetNumMaterials()))
	{
		UE_LOG(LogAvaMask, Warning, TEXT("Expected 4 materials, got %u"), InMaterials.Num());
		return;
	}

	if (!ensure(InSetToggle.Num() == GetNumMaterials()))
	{
		UE_LOG(LogAvaMask, Warning, TEXT("Expected 4 materials, got %u"), InSetToggle.Num());
		return;
	}

	if (UText3DComponent* TextComponent = GetComponent())
	{
		if (InSetToggle[0])
		{
			TextComponent->SetFrontMaterial(InMaterials[0]);	
		}

		if (InSetToggle[1])
		{
			TextComponent->SetBevelMaterial(InMaterials[1]);	
		}

		if (InSetToggle[2])
		{
			TextComponent->SetExtrudeMaterial(InMaterials[2]);	
		}

		if (InSetToggle[3])
		{
			TextComponent->SetBackMaterial(InMaterials[3]);	
		}
	}
}

int32 FAvaMaskText3DActorMaterialCollectionHandle::GetNumMaterials() const
{
	constexpr int32 TextMaterialCount = static_cast<int32>(EText3DGroupType::TypeCount);
	static_assert(TextMaterialCount == 4);
	
	return TextMaterialCount;
}

void FAvaMaskText3DActorMaterialCollectionHandle::ForEachMaterial(
	TFunctionRef<bool(
		const FSoftComponentReference& InComponent
		, const int32 InIdx
		, UMaterialInterface* InMaterial)> InFunction)
{
	if (const UText3DComponent* TextComponent = GetComponent())
	{
		const FSoftComponentReference ComponentReference = UE::AvaMask::Internal::MakeComponentReference(TextComponent->GetOwner(), TextComponent);
		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Front), TextComponent->GetFrontMaterial()))
		{
			return;
		}

		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Bevel), TextComponent->GetBevelMaterial()))
		{
			return;
		}

		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Extrude), TextComponent->GetExtrudeMaterial()))
		{
			return;
		}
		
		InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Back), TextComponent->GetBackMaterial());
	}
}

void FAvaMaskText3DActorMaterialCollectionHandle::ForEachMaterialHandle(TFunctionRef<bool(const FSoftComponentReference& InComponent, const int32 InIdx, const bool bInIsSlotOccupied, const TSharedPtr<IAvaMaskMaterialHandle>& InMaterialHandle)> InFunction)
{
	if (const UText3DComponent* TextComponent = GetComponent())
	{
		// Effectively refreshes material handles if necessary
		GetMaterialHandles();
		
		const FSoftComponentReference ComponentReference = UE::AvaMask::Internal::MakeComponentReference(TextComponent->GetOwner(), TextComponent);
		TSharedPtr<IAvaMaskMaterialHandle> MaterialHandle = MaterialHandles[static_cast<int32>(EText3DGroupType::Front)];
		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Front), MaterialHandle.IsValid(), MaterialHandle))
		{
			return;
		}

		MaterialHandle = MaterialHandles[static_cast<int32>(EText3DGroupType::Bevel)];
		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Bevel), MaterialHandle.IsValid(), MaterialHandle))
		{
			return;
		}

		MaterialHandle = MaterialHandles[static_cast<int32>(EText3DGroupType::Extrude)];
		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Extrude), MaterialHandle.IsValid(), MaterialHandle))
		{
			return;
		}

		MaterialHandle = MaterialHandles[static_cast<int32>(EText3DGroupType::Back)];
		InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Back), MaterialHandle.IsValid(), MaterialHandle);
	}
}

void FAvaMaskText3DActorMaterialCollectionHandle::MapEachMaterial(
	TFunctionRef<UMaterialInterface*(
		const FSoftComponentReference& InComponent
		, const int32 InIdx
		, UMaterialInterface* InMaterial)> InFunction)
{	
	if (const UText3DComponent* TextComponent = GetComponent())
	{
		TArray<TObjectPtr<UMaterialInterface>> MappedMaterials;
		MappedMaterials.Init(nullptr, static_cast<TArray<TObjectPtr<UMaterialInterface>>::SizeType>(EText3DGroupType::TypeCount));

		TBitArray<> SetFlags;
        SetFlags.Init(false, MappedMaterials.Num());

		const FSoftComponentReference ComponentReference = UE::AvaMask::Internal::MakeComponentReference(TextComponent->GetOwner(), TextComponent);
		
		MappedMaterials[0] = InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Front), TextComponent->GetFrontMaterial());
		SetFlags[0] = MappedMaterials[0] != nullptr;

		MappedMaterials[1] = InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Bevel), TextComponent->GetBevelMaterial());
		SetFlags[1] = MappedMaterials[1] != nullptr;

		MappedMaterials[2] = InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Extrude), TextComponent->GetExtrudeMaterial());
		SetFlags[2] = MappedMaterials[2] != nullptr;

		MappedMaterials[3] = InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Back), TextComponent->GetBackMaterial());
		SetFlags[3] = MappedMaterials[3] != nullptr;

		SetMaterials(MappedMaterials, SetFlags);
	}
}

void FAvaMaskText3DActorMaterialCollectionHandle::MapEachMaterialHandle(TFunctionRef<TSharedPtr<IAvaMaskMaterialHandle>(const FSoftComponentReference& InComponent, const int32 InIdx, const bool bInIsSlotOccupied, const TSharedPtr<IAvaMaskMaterialHandle>& InMaterialHandle)> InFunction)
{
	if (const UText3DComponent* TextComponent = GetComponent())
	{
		// Effectively refreshes material handles if necessary
		GetMaterialHandles();

		TArray<TObjectPtr<UMaterialInterface>> MappedMaterials;
		MappedMaterials.Reserve(GetNumMaterials());

		TBitArray<> SetFlags;
		SetFlags.Init(false, GetNumMaterials());

		const FSoftComponentReference ComponentReference = UE::AvaMask::Internal::MakeComponentReference(TextComponent->GetOwner(), TextComponent);

		auto TryMap = [this, InFunction, &ComponentReference, &MappedMaterials, &SetFlags](const EText3DGroupType InMaterialGroup)
		{
			int32 InMaterialGroupIdx = static_cast<int32>(InMaterialGroup);			
			
			TSharedPtr<IAvaMaskMaterialHandle>& MaterialHandle = MaterialHandles[InMaterialGroupIdx];
			if (ensureAlways(UE::Ava::Internal::IsHandleValid(MaterialHandle)))
			{
				const TSharedPtr<IAvaMaskMaterialHandle> MappedMaterialHandle = InFunction(ComponentReference, InMaterialGroupIdx, true, MaterialHandle);
				UMaterialInterface* RemappedMaterial = nullptr;
				if (MappedMaterialHandle.IsValid())
				{
					RemappedMaterial = MappedMaterialHandle->GetMaterial();	
				}
				else
				{
					RemappedMaterial = MaterialHandle->GetMaterial();
				}

				MappedMaterials[InMaterialGroupIdx] = RemappedMaterial;
				SetFlags[InMaterialGroupIdx] = RemappedMaterial != nullptr;
			}	
		};

		TryMap(EText3DGroupType::Front);
		TryMap(EText3DGroupType::Bevel);
		TryMap(EText3DGroupType::Extrude);
		TryMap(EText3DGroupType::Back);

		SetMaterials(MappedMaterials, SetFlags);
	}
}

bool FAvaMaskText3DActorMaterialCollectionHandle::IsValid() const
{
	return WeakActor.IsValid() && WeakComponent.IsValid();
}

bool FAvaMaskText3DActorMaterialCollectionHandle::IsSupported(
	const UStruct* InStruct
	, const TVariant<UObject*, FStructView>& InInstance
	, FName InTag)
{
	if (InTag == UE::AvaMask::Internal::HandleTag
		&& InStruct
		&& ::IsValid(InStruct)
		&& InStruct->IsChildOf<AActor>()
		&& InInstance.TryGet<UObject*>() != nullptr)
	{
		if (const AActor* Actor = Cast<AActor>(InInstance.Get<UObject*>()))
		{
			return Actor->GetComponentByClass<UText3DComponent>() != nullptr;
		}
	}

	return false;
}

FStructView FAvaMaskText3DActorMaterialCollectionHandle::GetMaterialHandleData(
	FHandleData* InParentHandleData
	, const FSoftComponentReference& InComponent
	, const int32 InSlotIdx)
{
	if (InParentHandleData)
	{
		if (ensure(InParentHandleData->GroupMaterialData.Contains(InSlotIdx)))
		{
			return InParentHandleData->GroupMaterialData[InSlotIdx];
		}
	}

	return nullptr;
}

FStructView FAvaMaskText3DActorMaterialCollectionHandle::GetOrAddMaterialHandleData(
	FHandleData* InParentHandleData
	, const TSharedPtr<IAvaMaskMaterialHandle>& InMaterialHandle
	, const FSoftComponentReference& InComponent
	, const int32 InSlotIdx)
{
	if (InParentHandleData)
	{
		return InParentHandleData->GroupMaterialData.FindOrAdd(InSlotIdx, InMaterialHandle->MakeDataStruct());
	}

	return nullptr;
}

UText3DComponent* FAvaMaskText3DActorMaterialCollectionHandle::GetComponent()
{
	if (UText3DComponent* Component = WeakComponent.Get())
	{
		return Component;
	}

	if (const AActor* Actor = WeakActor.Get())
	{
		WeakComponent = Actor->GetComponentByClass<UText3DComponent>();
		if (UText3DComponent* Component = WeakComponent.Get())
		{
			return Component;
		}
	}

	return nullptr;
}

FAvaMaskAvaTextActorMaterialCollectionHandle::FAvaMaskAvaTextActorMaterialCollectionHandle(AActor* InActor)
	: WeakActor(InActor)
	, WeakComponent(InActor->GetComponentByClass<UAvaText3DComponent>())
{
	if (UAvaText3DComponent* TextComponent = GetComponent())
	{
		TextComponent->GetMaterialProviderDelegate().BindRaw(this, &FAvaMaskAvaTextActorMaterialCollectionHandle::ProvideMaterialWithSettings);
	}
}

FAvaMaskAvaTextActorMaterialCollectionHandle::~FAvaMaskAvaTextActorMaterialCollectionHandle()
{
	if (UAvaText3DComponent* TextComponent = GetComponent())
	{
		TextComponent->GetMaterialProviderDelegate().Unbind();
	}
}

TArray<TObjectPtr<UMaterialInterface>> FAvaMaskAvaTextActorMaterialCollectionHandle::GetMaterials()
{
	TArray<TObjectPtr<UMaterialInterface>> Materials;
	Materials.Reserve(GetNumMaterials());
	
	if (const UAvaText3DComponent* TextComponent = GetComponent())
	{
		Materials.Emplace(TextComponent->GetFrontMaterial());
		Materials.Emplace(TextComponent->GetBevelMaterial());
		Materials.Emplace(TextComponent->GetExtrudeMaterial());
		Materials.Emplace(TextComponent->GetBackMaterial());
	}

	return Materials;
}

TArray<TSharedPtr<IAvaMaskMaterialHandle>> FAvaMaskAvaTextActorMaterialCollectionHandle::GetMaterialHandles()
{
	if (MaterialHandles.IsEmpty() || !UE::Ava::Internal::IsHandleValid(MaterialHandles.Last()))
	{
		if (const UAvaText3DComponent* TextComponent = GetComponent())
		{
			MaterialHandles.Init(nullptr, static_cast<int32>(EText3DGroupType::TypeCount));

			UAvaObjectHandleSubsystem* HandleSubsystem = UE::Ava::Internal::GetObjectHandleSubsystem();
			if (!HandleSubsystem)
			{
				return MaterialHandles;
			}

			UMaterialInterface* FrontMaterial = TextComponent->GetFrontMaterial();
			MaterialHandles[static_cast<int32>(EText3DGroupType::Front)] = HandleSubsystem->MakeHandle<IAvaMaskMaterialHandle>(FrontMaterial, UE::AvaMask::Internal::HandleTag);

			UMaterialInterface* BevelMaterial = TextComponent->GetBevelMaterial();
			MaterialHandles[static_cast<int32>(EText3DGroupType::Bevel)] = HandleSubsystem->MakeHandle<IAvaMaskMaterialHandle>(BevelMaterial, UE::AvaMask::Internal::HandleTag);

			UMaterialInterface* ExtrudeMaterial = TextComponent->GetBevelMaterial();
			MaterialHandles[static_cast<int32>(EText3DGroupType::Extrude)] = HandleSubsystem->MakeHandle<IAvaMaskMaterialHandle>(ExtrudeMaterial, UE::AvaMask::Internal::HandleTag);

			UMaterialInterface* BackMaterial = TextComponent->GetBevelMaterial();
			MaterialHandles[static_cast<int32>(EText3DGroupType::Back)] = HandleSubsystem->MakeHandle<IAvaMaskMaterialHandle>(BackMaterial, UE::AvaMask::Internal::HandleTag);
		}
	}

	return MaterialHandles;
}

int32 FAvaMaskAvaTextActorMaterialCollectionHandle::GetNumMaterials() const
{
	constexpr int32 TextMaterialCount = static_cast<int32>(EText3DGroupType::TypeCount);
	static_assert(TextMaterialCount == 4);
	
	return TextMaterialCount;
}

void FAvaMaskAvaTextActorMaterialCollectionHandle::SetMaterial(
	const FSoftComponentReference& InComponent
	, const int32 InSlotIdx
	, UMaterialInterface* InMaterial)
{
	if (!ensure(InSlotIdx < 4))
	{
		UE_LOG(LogAvaMask, Error, TEXT("SlotIdx out of range, should be from 0-3, was %u"), InSlotIdx);
		return;
	}

	if (UAvaText3DComponent* TextComponent = GetComponent())
    {
		switch (InSlotIdx)
		{
		case 0:
			TextComponent->SetFrontMaterial(InMaterial);

		case 1:
			TextComponent->SetBevelMaterial(InMaterial);

		case 2:
			TextComponent->SetExtrudeMaterial(InMaterial);

		case 3:
			TextComponent->SetBackMaterial(InMaterial);

		default:
			return;
		}
    }
}

void FAvaMaskAvaTextActorMaterialCollectionHandle::SetMaterials(
	const TArray<TObjectPtr<UMaterialInterface>>& InMaterials
	, const TBitArray<>& InSetToggle)
{
	if (!ensure(InMaterials.Num() == GetNumMaterials()))
	{
		UE_LOG(LogAvaMask, Warning, TEXT("Expected 4 materials, got %u"), InMaterials.Num());
		return;
	}

	if (!ensure(InSetToggle.Num() == GetNumMaterials()))
	{
		UE_LOG(LogAvaMask, Warning, TEXT("Expected 4 materials, got %u"), InSetToggle.Num());
		return;
	}
	
	if (UAvaText3DComponent* TextComponent = GetComponent())
	{
		if (InSetToggle[0])
		{
			TextComponent->SetFrontMaterial(InMaterials[0]);	
		}

		if (InSetToggle[1])
		{
			TextComponent->SetBevelMaterial(InMaterials[1]);	
		}

		if (InSetToggle[2])
		{
			TextComponent->SetExtrudeMaterial(InMaterials[2]);	
		}

		if (InSetToggle[3])
		{
			TextComponent->SetBackMaterial(InMaterials[3]);	
		}
	}
}

void FAvaMaskAvaTextActorMaterialCollectionHandle::ForEachMaterial(
	TFunctionRef<bool(
		const FSoftComponentReference& InComponent
		, const int32 InIdx
		, UMaterialInterface* InMaterial)> InFunction)
{
	if (const UAvaText3DComponent* TextComponent = GetComponent())
	{
		const FSoftComponentReference ComponentReference = UE::AvaMask::Internal::MakeComponentReference(TextComponent->GetOwner(), TextComponent);
		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Front), TextComponent->GetFrontMaterial()))
		{
			return;
		}
		
		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Bevel), TextComponent->GetBevelMaterial()))
		{
			return;
		}
		
		if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Extrude), TextComponent->GetExtrudeMaterial()))
		{
			return;
		}
		
		InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Back), TextComponent->GetBackMaterial());
	}
}

void FAvaMaskAvaTextActorMaterialCollectionHandle::ForEachMaterialHandle(TFunctionRef<bool(const FSoftComponentReference& InComponent, const int32 InIdx, const bool bInIsSlotOccupied, const TSharedPtr<IAvaMaskMaterialHandle>& InMaterialHandle)> InFunction)
{
	if (const UAvaText3DComponent* TextComponent = GetComponent())
	{
		// Effectively refreshes material handles if necessary
		GetMaterialHandles();
		
		const FSoftComponentReference ComponentReference = UE::AvaMask::Internal::MakeComponentReference(TextComponent->GetOwner(), TextComponent);
		TSharedPtr<IAvaMaskMaterialHandle>& MaterialHandle = MaterialHandles[static_cast<int32>(EText3DGroupType::Front)];
		if (ensureAlways(UE::Ava::Internal::IsHandleValid(MaterialHandle)))
		{
			if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Front), true, MaterialHandle))
			{
				return;
			}
		}

		MaterialHandle = MaterialHandles[static_cast<int32>(EText3DGroupType::Bevel)];
		if (ensureAlways(UE::Ava::Internal::IsHandleValid(MaterialHandle)))
		{
			if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Bevel), true, MaterialHandle))
			{
				return;
			}
		}

		MaterialHandle = MaterialHandles[static_cast<int32>(EText3DGroupType::Extrude)];
		if (ensureAlways(UE::Ava::Internal::IsHandleValid(MaterialHandle)))
		{
			if (!InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Extrude), true, MaterialHandle))
			{
				return;
			}
		}

		MaterialHandle = MaterialHandles[static_cast<int32>(EText3DGroupType::Back)];
		if (ensureAlways(UE::Ava::Internal::IsHandleValid(MaterialHandle)))
		{
			InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Back), true, MaterialHandle);
		}
	}
}

void FAvaMaskAvaTextActorMaterialCollectionHandle::MapEachMaterial(
	TFunctionRef<UMaterialInterface*(
		const FSoftComponentReference& InComponent
		, const int32 InIdx
		, UMaterialInterface* InMaterial)> InFunction)
{
	if (const UAvaText3DComponent* TextComponent = GetComponent())
	{
		TArray<TObjectPtr<UMaterialInterface>> MappedMaterials;
		MappedMaterials.Init(nullptr, static_cast<int32>(EText3DGroupType::TypeCount));

		TBitArray<> SetFlags;
		SetFlags.Init(false, MappedMaterials.Num());

		const FSoftComponentReference ComponentReference = UE::AvaMask::Internal::MakeComponentReference(TextComponent->GetOwner(), TextComponent);
		
		MappedMaterials[0] = InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Front), TextComponent->GetFrontMaterial());
		SetFlags[0] = MappedMaterials[0] != nullptr;
		
		MappedMaterials[1] = InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Bevel), TextComponent->GetBevelMaterial());
		SetFlags[1] = MappedMaterials[1] != nullptr;
		
		MappedMaterials[2] = InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Extrude), TextComponent->GetExtrudeMaterial());
		SetFlags[2] = MappedMaterials[2] != nullptr;
		
		MappedMaterials[3] = InFunction(ComponentReference, static_cast<int32>(EText3DGroupType::Back), TextComponent->GetBackMaterial());
		SetFlags[3] = MappedMaterials[3] != nullptr;
		
		SetMaterials(MappedMaterials, SetFlags);
	}
}

void FAvaMaskAvaTextActorMaterialCollectionHandle::MapEachMaterialHandle(TFunctionRef<TSharedPtr<IAvaMaskMaterialHandle>(const FSoftComponentReference& InComponent, const int32 InIdx, const bool bInIsSlotOccupied, const TSharedPtr<IAvaMaskMaterialHandle>& InMaterialHandle)> InFunction)
{
	if (const UAvaText3DComponent* TextComponent = GetComponent())
	{		
		// Effectively refreshes material handles if necessary
		GetMaterialHandles();

		TArray<TObjectPtr<UMaterialInterface>> MappedMaterials;
		MappedMaterials.Reserve(GetNumMaterials());

		TBitArray<> SetFlags;
		SetFlags.Init(false, GetNumMaterials());

		const FSoftComponentReference ComponentReference = UE::AvaMask::Internal::MakeComponentReference(TextComponent->GetOwner(), TextComponent);

		auto TryMap = [this, InFunction, &ComponentReference, &MappedMaterials, &SetFlags](const EText3DGroupType InMaterialGroup)
		{
			int32 InMaterialGroupIdx = static_cast<int32>(InMaterialGroup);			
			
			TSharedPtr<IAvaMaskMaterialHandle>& MaterialHandle = MaterialHandles[InMaterialGroupIdx];
			if (ensureAlways(UE::Ava::Internal::IsHandleValid(MaterialHandle)))
			{
				const TSharedPtr<IAvaMaskMaterialHandle> MappedMaterialHandle = InFunction(ComponentReference, InMaterialGroupIdx, true, MaterialHandle);
				UMaterialInterface* RemappedMaterial = nullptr;
				if (MappedMaterialHandle.IsValid())
				{
					RemappedMaterial = MappedMaterialHandle->GetMaterial();	
				}
				else
				{
					RemappedMaterial = MaterialHandle->GetMaterial();
				}

				MappedMaterials[InMaterialGroupIdx] = RemappedMaterial;
				SetFlags[InMaterialGroupIdx] = RemappedMaterial != nullptr;
			}	
		};

		TryMap(EText3DGroupType::Front);
		TryMap(EText3DGroupType::Bevel);
		TryMap(EText3DGroupType::Extrude);
		TryMap(EText3DGroupType::Back);

		SetMaterials(MappedMaterials, SetFlags);
	}
}

bool FAvaMaskAvaTextActorMaterialCollectionHandle::SaveOriginalState(const FStructView& InHandleData)
{
	if (FHandleData* HandleData = InHandleData.GetPtr<FHandleData>())
	{
		ForEachMaterialHandle([this, HandleData](
			const FSoftComponentReference& InComponent
			, const int32 InSlotIdx
			, const bool bInIsSlotOccupied
			, const TSharedPtr<IAvaMaskMaterialHandle>& InMaterialHandle)
		{
			FInstancedStruct& MaterialHandleData = HandleData->GroupMaterialData.FindOrAdd(InSlotIdx, InMaterialHandle->MakeDataStruct());
			return InMaterialHandle->SaveOriginalState(MaterialHandleData);
		});

		if (const UAvaText3DComponent* TextComponent = GetComponent())
		{
			HandleData->OriginalTranslucencyStyle = TextComponent->GetTranslucencyStyle();
			return true;
		}
		
		return true;
	}

	return false;
}

bool FAvaMaskAvaTextActorMaterialCollectionHandle::ApplyOriginalState(const FStructView& InHandleData)
{
	if (const FHandleData* HandleData = InHandleData.GetPtr<FHandleData>())
	{
		if (UAvaText3DComponent* TextComponent = GetComponent())
		{
			// Only restore if the original was NOT translucent AND the current alpha is 1.0
			if (HandleData->OriginalTranslucencyStyle == EAvaTextTranslucency::None
				&& FMath::IsNearlyEqual(TextComponent->GetOpacity(), 1.0f))
			{
				TextComponent->SetTranslucencyStyle(HandleData->OriginalTranslucencyStyle);	
			}
			TextComponent->RefreshMaterialInstances();
			
			Super::ApplyOriginalState(InHandleData);
			
			return true;
		}

		return false;
	}

	return false;
}

bool FAvaMaskAvaTextActorMaterialCollectionHandle::ApplyModifiedState(
	const FAvaMask2DSubjectParameters& InModifiedParameters
	, const FStructView& InHandleData)
{
	if (InHandleData.GetPtr<FHandleData>())
	{
		if (!Super::ApplyModifiedState(InModifiedParameters, InHandleData))
		{
			return false;
		}

		if (UAvaText3DComponent* TextComponent = GetComponent())
		{
			const EAvaTextTranslucency TargetTranslucencyStyle = UE::AvaMask::Private::GetTargetTranslucencyType(TextComponent->GetTranslucencyStyle(), InModifiedParameters.MaterialParameters.BlendMode);
			TextComponent->SetTranslucencyStyle(TargetTranslucencyStyle);
			TextComponent->RefreshMaterialInstances();
			return true;
		}

		return true;
	}

	return false;
}

bool FAvaMaskAvaTextActorMaterialCollectionHandle::IsValid() const
{
	return WeakActor.IsValid() && WeakComponent.IsValid();
}

bool FAvaMaskAvaTextActorMaterialCollectionHandle::IsSupported(
	const UStruct* InStruct
	, const TVariant<UObject*, FStructView>& InInstance
	, FName InTag)
{
	if (InTag == UE::AvaMask::Internal::HandleTag
		&& InStruct
		&& ::IsValid(InStruct)
		&& InStruct->IsChildOf<AActor>()
		&& InInstance.TryGet<UObject*>() != nullptr)
	{
		if (const AActor* Actor = Cast<AActor>(InInstance.Get<UObject*>()))
		{
			return Actor->GetComponentByClass<UAvaText3DComponent>() != nullptr;
		}
	}

	return false;
}

FStructView FAvaMaskAvaTextActorMaterialCollectionHandle::GetMaterialHandleData(
	FHandleData* InParentHandleData
	, const FSoftComponentReference& InComponent
	, const int32 InSlotIdx)
{
	if (InParentHandleData)
	{
		if (ensure(InParentHandleData->GroupMaterialData.Contains(InSlotIdx)))
		{
			return InParentHandleData->GroupMaterialData[InSlotIdx];
		}
	}

	return nullptr;	
}

FStructView FAvaMaskAvaTextActorMaterialCollectionHandle::GetOrAddMaterialHandleData(
	FHandleData* InParentHandleData
	, const TSharedPtr<IAvaMaskMaterialHandle>& InMaterialHandle
	, const FSoftComponentReference& InComponent
	, const int32 InSlotIdx)
{
	if (InParentHandleData)
	{
		return InParentHandleData->GroupMaterialData.FindOrAdd(InSlotIdx, InMaterialHandle->MakeDataStruct());
	}

	return nullptr;
}

UAvaText3DComponent* FAvaMaskAvaTextActorMaterialCollectionHandle::GetComponent()
{
	if (UAvaText3DComponent* Component = WeakComponent.Get())
	{
		return Component;
	}

	if (const AActor* Actor = WeakActor.Get())
	{
		WeakComponent = Actor->GetComponentByClass<UAvaText3DComponent>();
		if (UAvaText3DComponent* Component = WeakComponent.Get())
		{
			return Component;
		}
	}

	return nullptr;
}

UMaterialInterface* FAvaMaskAvaTextActorMaterialCollectionHandle::ProvideMaterialWithSettings(const UMaterialInterface* InPreviousMaterial, const FAvaTextMaterialSettings& InMaterialSettings)
{
	UMaterialInterface* AttemptedAssignedMaterial = UAvaTextMaterialHub::GetMaterial(InMaterialSettings);

	if (OnSourceMaterialPreAssignment().IsBound())
	{
		UMaterialInterface* UserAssignedMaterial = OnSourceMaterialPreAssignment().Execute(InPreviousMaterial, AttemptedAssignedMaterial);

		// Flag handles for refresh
		MaterialHandles.Reset();
		GetMaterialHandles();

		return UserAssignedMaterial;
	}

	return nullptr;
}
