// Copyright Epic Games, Inc. All Rights Reserved.

#include "PersonaAssetFamily.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimBlueprint.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Styling/AppStyle.h"
#include "AssetToolsModule.h"
#include "Preferences/PersonaOptions.h"

#define LOCTEXT_NAMESPACE "PersonaAssetFamily"

FPersonaAssetFamily::FPersonaAssetFamily(const UObject* InFromObject)
	: Skeleton(nullptr)
	, Mesh(nullptr)
	, AnimBlueprint(nullptr)
	, AnimationAsset(nullptr)
	, PhysicsAsset(nullptr)
{
	if (InFromObject)
	{
		if (InFromObject->IsA<USkeleton>())
		{
			Skeleton = CastChecked<USkeleton>(InFromObject);
		}
		else if (InFromObject->IsA<UAnimationAsset>())
		{
			AnimationAsset = CastChecked<UAnimationAsset>(InFromObject);
		}
		else if (InFromObject->IsA<USkeletalMesh>())
		{
			Mesh = CastChecked<USkeletalMesh>(InFromObject);
		}
		else if (InFromObject->IsA<UAnimBlueprint>())
		{
			AnimBlueprint = CastChecked<UAnimBlueprint>(InFromObject);
		}
		else if (InFromObject->IsA<UPhysicsAsset>())
		{
			PhysicsAsset = CastChecked<UPhysicsAsset>(InFromObject);
		}

		FindCounterpartAssets(InFromObject, Skeleton, Mesh);
	}
}

FPersonaAssetFamily::FPersonaAssetFamily(const UObject* InFromObject, const TSharedRef<FPersonaAssetFamily> InFromFamily)
	: Skeleton(InFromFamily->Skeleton)
	, Mesh(InFromFamily->Mesh)
	, AnimBlueprint(InFromFamily->AnimBlueprint)
	, AnimationAsset(InFromFamily->AnimationAsset)
	, PhysicsAsset(InFromFamily->PhysicsAsset)
{
	if (InFromObject)
	{
		if (InFromObject->IsA<USkeleton>())
		{
			Skeleton = CastChecked<USkeleton>(InFromObject);
		}
		else if (InFromObject->IsA<UAnimationAsset>())
		{
			AnimationAsset = CastChecked<UAnimationAsset>(InFromObject);
		}
		else if (InFromObject->IsA<USkeletalMesh>())
		{
			Mesh = CastChecked<USkeletalMesh>(InFromObject);
		}
		else if (InFromObject->IsA<UAnimBlueprint>())
		{
			AnimBlueprint = CastChecked<UAnimBlueprint>(InFromObject);
		}
		else if (InFromObject->IsA<UPhysicsAsset>())
		{
			PhysicsAsset = CastChecked<UPhysicsAsset>(InFromObject);
		}
	}
}

void FPersonaAssetFamily::Initialize()
{
	GetMutableDefault<UPersonaOptions>()->RegisterOnUpdateSettings(UPersonaOptions::FOnUpdateSettingsMulticaster::FDelegate::CreateSP(this, &FPersonaAssetFamily::OnSettingsChange));
}

void FPersonaAssetFamily::GetAssetTypes(TArray<UClass*>& OutAssetTypes) const
{
	OutAssetTypes.Reset();
	OutAssetTypes.Add(USkeleton::StaticClass());
	OutAssetTypes.Add(USkeletalMesh::StaticClass());
	OutAssetTypes.Add(UAnimationAsset::StaticClass());
	OutAssetTypes.Add(UAnimBlueprint::StaticClass());
	OutAssetTypes.Add(UPhysicsAsset::StaticClass());
}

template<typename AssetType>
static void FindAssets(const USkeleton* InSkeleton, TArray<FAssetData>& OutAssetData, FName SkeletonTag)
{
	if (!InSkeleton)
	{
		return;
	}

	InSkeleton->GetCompatibleAssets(AssetType::StaticClass(), *SkeletonTag.ToString(), OutAssetData);
}

FAssetData FPersonaAssetFamily::FindAssetOfType(UClass* InAssetClass) const
{
	if (InAssetClass)
	{
		if (InAssetClass->IsChildOf<USkeleton>())
		{
			// we should always have a skeleton here, this asset family is based on it
			return FAssetData(Skeleton.Get());
		}
		else if (InAssetClass->IsChildOf<UAnimationAsset>())
		{
			if (AnimationAsset.IsValid())
			{
				return FAssetData(AnimationAsset.Get());
			}
			else
			{
				TArray<FAssetData> Assets;
				FindAssets<UAnimationAsset>(Skeleton.Get(), Assets, "Skeleton");
				if (Assets.Num() > 0)
				{
					return Assets[0];
				}
			}
		}
		else if (InAssetClass->IsChildOf<USkeletalMesh>())
		{
			if (Mesh.IsValid())
			{
				return FAssetData(Mesh.Get());
			}
			else
			{
				TArray<FAssetData> Assets;
				FindAssets<USkeletalMesh>(Skeleton.Get(), Assets, "Skeleton");
				if (Assets.Num() > 0)
				{
					return Assets[0];
				}
			}
		}
		else if (InAssetClass->IsChildOf<UAnimBlueprint>())
		{
			if (AnimBlueprint.IsValid())
			{
				return FAssetData(AnimBlueprint.Get());
			}
			else
			{
				TArray<FAssetData> Assets;
				FindAssets<UAnimBlueprint>(Skeleton.Get(), Assets, "TargetSkeleton");
				if (Assets.Num() > 0)
				{
					return Assets[0];
				}
			}
		}
		else if (InAssetClass->IsChildOf<UPhysicsAsset>())
		{
			if (PhysicsAsset.IsValid())
			{
				return FAssetData(PhysicsAsset.Get());
			}
			else
			{
				// If we have a mesh and it has a physics asset, use it
				if (Mesh.IsValid())
				{
					if (UPhysicsAsset* MeshPhysicsAsset = Mesh->GetPhysicsAsset())
					{
						return FAssetData(MeshPhysicsAsset);
					}
				}

				TArray<FAssetData> Assets;

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				FARFilter Filter;
				Filter.bRecursiveClasses = true;
				Filter.ClassPaths.Add(UPhysicsAsset::StaticClass()->GetClassPathName());

				// If we have a mesh, look for a physics asset that has that mesh as its preview mesh
				if (Mesh.IsValid())
				{
					Filter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UPhysicsAsset, PreviewSkeletalMesh), FSoftObjectPath(Mesh.Get()).ToString());
				}

				AssetRegistryModule.Get().GetAssets(Filter, Assets);

				if (Assets.Num() > 0)
				{
					return Assets[0];
				}
			}
		}
	}

	return FAssetData();
}

void FPersonaAssetFamily::FindAssetsOfType(UClass* InAssetClass, TArray<FAssetData>& OutAssets) const
{
	if (InAssetClass)
	{
		if (InAssetClass->IsChildOf<USkeleton>())
		{
			Skeleton.Get()->GetCompatibleSkeletonAssets(OutAssets);
		}
		else if (InAssetClass->IsChildOf<UAnimationAsset>())
		{
			FindAssets<UAnimationAsset>(Skeleton.Get(), OutAssets, "Skeleton");
		}
		else if (InAssetClass->IsChildOf<USkeletalMesh>())
		{
			FindAssets<USkeletalMesh>(Skeleton.Get(), OutAssets, "Skeleton");
		}
		else if (InAssetClass->IsChildOf<UAnimBlueprint>())
		{
			FindAssets<UAnimBlueprint>(Skeleton.Get(), OutAssets, "TargetSkeleton");
		}
		else if (InAssetClass->IsChildOf<UPhysicsAsset>())
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			FARFilter Filter;
			Filter.bRecursiveClasses = true;
			Filter.ClassPaths.Add(UPhysicsAsset::StaticClass()->GetClassPathName());

			// If we have a mesh, look for a physics asset that has that mesh as its preview mesh
			if (Mesh.IsValid())
			{
				Filter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UPhysicsAsset, PreviewSkeletalMesh), FSoftObjectPath(Mesh.Get()).ToString());
			}

			AssetRegistryModule.Get().GetAssets(Filter, OutAssets);

			// If we have a mesh and it has a physics asset, use it but only if its different from the one on the preview mesh
			if (Mesh.IsValid())
			{
				if (UPhysicsAsset* MeshPhysicsAsset = Mesh->GetPhysicsAsset())
				{
					OutAssets.AddUnique(FAssetData(MeshPhysicsAsset));
				}
			}
		}
	}
}

FText FPersonaAssetFamily::GetAssetTypeDisplayName(UClass* InAssetClass) const
{
	if (InAssetClass)
	{
		if (InAssetClass->IsChildOf<USkeleton>())
		{
			return LOCTEXT("SkeletonAssetDisplayName", "Skeleton");
		}
		else if (InAssetClass->IsChildOf<UAnimationAsset>())
		{
			return LOCTEXT("AnimationAssetDisplayName", "Animation");
		}
		else if (InAssetClass->IsChildOf<USkeletalMesh>())
		{
			return LOCTEXT("SkeletalMeshAssetDisplayName", "Mesh");
		}
		else if (InAssetClass->IsChildOf<UAnimBlueprint>())
		{
			return LOCTEXT("AnimBlueprintAssetDisplayName", "Blueprint");
		}
		else if (InAssetClass->IsChildOf<UPhysicsAsset>())
		{
			return LOCTEXT("PhysicsAssetDisplayName", "Physics");
		}
	}

	return FText();
}

const FSlateBrush* FPersonaAssetFamily::GetAssetTypeDisplayIcon(UClass* InAssetClass) const
{
	if (InAssetClass)
	{
		if (InAssetClass->IsChildOf<USkeleton>())
		{
			return FAppStyle::Get().GetBrush("Persona.AssetClass.Skeleton");
		}
		else if (InAssetClass->IsChildOf<UAnimationAsset>())
		{
			return FAppStyle::Get().GetBrush("Persona.AssetClass.Animation");
		}
		else if (InAssetClass->IsChildOf<USkeletalMesh>())
		{
			return FAppStyle::Get().GetBrush("Persona.AssetClass.SkeletalMesh");
		}
		else if (InAssetClass->IsChildOf<UAnimBlueprint>())
		{
			return FAppStyle::Get().GetBrush("Persona.AssetClass.Blueprint");
		}
		else if (InAssetClass->IsChildOf<UPhysicsAsset>())
		{
			return FAppStyle::Get().GetBrush("Persona.AssetClass.Physics");
		}
	}

	return nullptr;
}	

FSlateColor FPersonaAssetFamily::GetAssetTypeDisplayTint(UClass* InAssetClass) const
{
	static const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UClass* UseAssetClass = nullptr;
	if (InAssetClass)
	{
		if (InAssetClass->IsChildOf<USkeleton>())
		{
			UseAssetClass = USkeleton::StaticClass();
		}
		else if (InAssetClass->IsChildOf<UAnimationAsset>())
		{
			UseAssetClass = UAnimationAsset::StaticClass();
		}
		else if (InAssetClass->IsChildOf<USkeletalMesh>())
		{
			UseAssetClass = USkeletalMesh::StaticClass();
		}
		else if (InAssetClass->IsChildOf<UAnimBlueprint>())
		{
			UseAssetClass = UAnimBlueprint::StaticClass();
		}
		else if (InAssetClass->IsChildOf<UPhysicsAsset>())
		{
			UseAssetClass = UPhysicsAsset::StaticClass();
		}
	}

	if (UseAssetClass)
	{
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(UseAssetClass);
		if (AssetTypeActions.IsValid())
		{
			return AssetTypeActions.Pin()->GetTypeColor();
		}
	}
	return FSlateColor::UseForeground();
}

bool FPersonaAssetFamily::IsAssetCompatible(const FAssetData& InAssetData) const
{
	UClass* Class = InAssetData.GetClass();
	if (Class)
	{
		if (Class->IsChildOf<USkeleton>())
		{
			if (Skeleton.Get())
			{
				return Skeleton.Get()->IsCompatibleForEditor(InAssetData);
			}
		}
		else if (Class->IsChildOf<UAnimationAsset>() || Class->IsChildOf<USkeletalMesh>())
		{
			FAssetDataTagMapSharedView::FFindTagResult Result = InAssetData.TagsAndValues.FindTag("Skeleton");

			if (Result.IsSet())
			{
				if (Skeleton.Get())
				{
					return Skeleton.Get()->IsCompatibleForEditor(Result.GetValue());
				}
			}
		}
		else if (Class->IsChildOf<UAnimBlueprint>())
		{
			FAssetDataTagMapSharedView::FFindTagResult Result = InAssetData.TagsAndValues.FindTag("TargetSkeleton");

			if (Result.IsSet())
			{
				if (Skeleton.Get())
				{
					return Skeleton.Get()->IsCompatibleForEditor(Result.GetValue());
				}
			}
		}
		else if (Class->IsChildOf<UPhysicsAsset>())
		{
			// If our mesh is valid and this is the physics asset used on it, we are compatible
			if (Mesh.IsValid() && InAssetData.GetSoftObjectPath() == FSoftObjectPath(Mesh.Get()->GetPhysicsAsset()))
			{
				return true;
			}

			// Otherwise check if our mesh is the preview mesh of the physics asset
			FAssetDataTagMapSharedView::FFindTagResult Result = InAssetData.TagsAndValues.FindTag(GET_MEMBER_NAME_CHECKED(UPhysicsAsset, PreviewSkeletalMesh));
			if (Result.IsSet() && Mesh.IsValid())
			{
				return Result.GetValue() == FSoftObjectPath(Mesh.Get()).ToString();
			}
		}
	}

	return false;
}

UClass* FPersonaAssetFamily::GetAssetFamilyClass(UClass* InClass) const
{
	if (InClass)
	{
		if (InClass->IsChildOf<USkeleton>())
		{
			return USkeleton::StaticClass();
		}
		else if (InClass->IsChildOf<UAnimationAsset>())
		{
			return UAnimationAsset::StaticClass();
		}
		else if (InClass->IsChildOf<USkeletalMesh>())
		{
			return USkeletalMesh::StaticClass();
		}
		else if (InClass->IsChildOf<UAnimBlueprint>())
		{
			return UAnimBlueprint::StaticClass();
		}
		else if (InClass->IsChildOf<UPhysicsAsset>())
		{
			return UPhysicsAsset::StaticClass();
		}
	}

	return nullptr;
}

void FPersonaAssetFamily::RecordAssetOpened(const FAssetData& InAssetData)
{
	if (IsAssetCompatible(InAssetData))
	{
		UClass* Class = InAssetData.GetClass();
		if (Class)
		{
			if (Class->IsChildOf<UAnimationAsset>())
			{
				AnimationAsset = Cast<UAnimationAsset>(InAssetData.GetAsset());
			}
			else if (Class->IsChildOf<USkeletalMesh>())
			{
				Mesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
			}
			else if (Class->IsChildOf<UAnimBlueprint>())
			{
				AnimBlueprint = Cast<UAnimBlueprint>(InAssetData.GetAsset());
			}
			else if (Class->IsChildOf<UPhysicsAsset>())
			{
				PhysicsAsset = Cast<UPhysicsAsset>(InAssetData.GetAsset());
			}
		}

		OnAssetOpened.Broadcast(InAssetData.GetAsset());
	}
}

void FPersonaAssetFamily::FindCounterpartAssets(const UObject* InAsset, TWeakObjectPtr<const USkeleton>& OutSkeleton, TWeakObjectPtr<const USkeletalMesh>& OutMesh)
{
	const USkeleton* CounterpartSkeleton = OutSkeleton.Get();
	const USkeletalMesh* CounterpartMesh = OutMesh.Get();
	FindCounterpartAssets(InAsset, CounterpartSkeleton, CounterpartMesh);
	OutSkeleton = CounterpartSkeleton;
	OutMesh = CounterpartMesh;
}

void FPersonaAssetFamily::FindCounterpartAssets(const UObject* InAsset, const USkeleton*& OutSkeleton, const USkeletalMesh*& OutMesh)
{
	if (InAsset->IsA<USkeleton>())
	{
		OutSkeleton = CastChecked<USkeleton>(InAsset);
		OutMesh = OutSkeleton->GetPreviewMesh();
		if(OutMesh == nullptr)
		{
			OutMesh = OutSkeleton->FindCompatibleMesh();
		}
	}
	else if (InAsset->IsA<UAnimationAsset>())
	{
		const UAnimationAsset* AnimationAsset = CastChecked<const UAnimationAsset>(InAsset);
		OutSkeleton = AnimationAsset->GetSkeleton();
		OutMesh = AnimationAsset->GetPreviewMesh();
		if (OutMesh == nullptr)
		{
			OutMesh = OutSkeleton->GetPreviewMesh();
		}
		if(OutMesh == nullptr)
		{
			OutMesh = OutSkeleton->FindCompatibleMesh();
		}
	}
	else if (InAsset->IsA<USkeletalMesh>())
	{
		OutMesh = CastChecked<USkeletalMesh>(InAsset);
		OutSkeleton = OutMesh->GetSkeleton();
	}
	else if (InAsset->IsA<UAnimBlueprint>())
	{
		const UAnimBlueprint* AnimBlueprint = CastChecked<const UAnimBlueprint>(InAsset);
		OutSkeleton = AnimBlueprint->TargetSkeleton;
		OutMesh = AnimBlueprint->GetPreviewMesh();
		check(AnimBlueprint->BlueprintType == BPTYPE_Interface || AnimBlueprint->bIsTemplate || AnimBlueprint->TargetSkeleton != nullptr);
		if(OutMesh == nullptr && AnimBlueprint->TargetSkeleton)
		{
			OutMesh = AnimBlueprint->TargetSkeleton->GetPreviewMesh();
			if(OutMesh == nullptr)
			{
				OutMesh = AnimBlueprint->TargetSkeleton->FindCompatibleMesh();
			}
		}
	}
	else if (InAsset->IsA<UPhysicsAsset>())
	{
		const UPhysicsAsset* PhysicsAsset = CastChecked<const UPhysicsAsset>(InAsset);
		OutMesh = PhysicsAsset->PreviewSkeletalMesh.LoadSynchronous();
		if(OutMesh != nullptr)
		{
			OutSkeleton = OutMesh->GetSkeleton();
		}
	}
}

void FPersonaAssetFamily::OnSettingsChange(const UPersonaOptions* InOptions, EPropertyChangeType::Type InChangeType)
{
	OnAssetFamilyChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
 
