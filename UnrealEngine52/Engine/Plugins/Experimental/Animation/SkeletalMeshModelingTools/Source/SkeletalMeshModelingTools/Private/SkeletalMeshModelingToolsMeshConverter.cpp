// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshModelingToolsMeshConverter.h"

#include "AssetToolsModule.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "PropertyEditorModule.h"
#include "StaticToSkeletalMeshConverter.h"
#include "Dialog/SCustomDialog.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkeletalMeshModelingToolsMeshConverter, Log, All)

#define LOCTEXT_NAMESPACE "SkeletalMeshModelingToolsMeshConverter"


USkeletonFromStaticMeshFactory::USkeletonFromStaticMeshFactory(
	const FObjectInitializer& InObjectInitializer
	) :
	Super(InObjectInitializer)
{
	SupportedClass = USkeleton::StaticClass();
	bCreateNew = true;
	bEditAfterNew = false;
}


UObject* USkeletonFromStaticMeshFactory::FactoryCreateNew(
	UClass* InClass,
	UObject* InParent, 
	FName InName,
	EObjectFlags InFlags,
	UObject* InContext,
	FFeedbackContext* InWarn
	)
{
	static const FVector RootBoneRelativePosition(0.5, 0.5, 0.0);

	USkeleton* Skeleton = NewObject<USkeleton>(InParent, InName, InFlags);
	
	if (!FStaticToSkeletalMeshConverter::InitializeSkeletonFromStaticMesh(Skeleton, StaticMesh, RootBoneRelativePosition))
	{
		return nullptr;
	}

	return Skeleton;
}


USkeletalMeshFromStaticMeshFactory::USkeletalMeshFromStaticMeshFactory(
	const FObjectInitializer& InObjectInitializer
	) :
	Super(InObjectInitializer)
{
	SupportedClass = USkeletalMesh::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


UObject* USkeletalMeshFromStaticMeshFactory::FactoryCreateNew(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags InFlags,
	UObject* InContext,
	FFeedbackContext* InWarn
	)
{
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(InParent, InName, InFlags);
	
	if (!FStaticToSkeletalMeshConverter::InitializeSkeletalMeshFromStaticMesh(SkeletalMesh, StaticMesh, ReferenceSkeleton, BindBoneName))
	{
		return nullptr;
	}
	
	// Update the skeletal mesh and the skeleton so that their ref skeletons are in sync and the skeleton's preview mesh
	// is the one we just created.
	SkeletalMesh->SetSkeleton(Skeleton);
	Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);
	if (!Skeleton->GetPreviewMesh())
	{
		Skeleton->SetPreviewMesh(SkeletalMesh);
	}
	
	return SkeletalMesh;
}


USkeleton* UStaticMeshToSkeletalMeshConvertOptions::GetSkeleton(
	bool& bInvalidSkeletonIsError,
	const IPropertyHandle* PropertyHandle)
{
	// bInvalidSkeletonIsError = true;

	switch (SkeletonImportOption)
	{
	default:
	case EReferenceSkeletonImportOption::CreateNew:
		return nullptr;
	case EReferenceSkeletonImportOption::UseExistingSkeleton:
		return Cast<USkeleton>(Skeleton.ResolveObject());
	case EReferenceSkeletonImportOption::UseExistingSkeletalMesh:
		{
			USkeletalMesh* SkeletalMeshPtr = Cast<USkeletalMesh>(SkeletalMesh.ResolveObject());
			return SkeletalMeshPtr ? SkeletalMeshPtr->GetSkeleton() : nullptr;
		}
	}
}


void UStaticMeshToSkeletalMeshConvertOptions::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UStaticMeshToSkeletalMeshConvertOptions, Skeleton) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UStaticMeshToSkeletalMeshConvertOptions, SkeletalMesh))
		{
			(void)SkeletonProviderChanged.ExecuteIfBound();
		}
	}
}


static bool ConvertSingleMeshToSkeletalMeshInteractive(
	UStaticMesh* InStaticMesh,
	TArray<UObject*>& OutObjectsAdded
	)
{
	UStaticMeshToSkeletalMeshConvertOptions* Options = NewObject<UStaticMeshToSkeletalMeshConvertOptions>();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// If the object paths don't resolve properly, the details view will still show something, which will be invalidated
	// later.
	if (Cast<USkeleton>(Options->Skeleton.ResolveObject()) == nullptr)
	{
		Options->Skeleton.Reset();
	}
	if (Cast<USkeletalMesh>(Options->SkeletalMesh.ResolveObject()) == nullptr)
	{
		Options->SkeletalMesh.Reset();
	}

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(Options);

	TSharedRef<SCustomDialog> OptionsDialog = SNew(SCustomDialog)
		.Title(LOCTEXT("OptionsDialogTitle", "Static Mesh Conversion Options"))
		.Content()
		[
			DetailsView
		]
	.Buttons({
		SCustomDialog::FButton(LOCTEXT("DialogButtonConvert", "Convert")),
		SCustomDialog::FButton(LOCTEXT("DialogButtonCancel", "Cancel"))
	});

	Options->SkeletonProviderChanged.BindLambda([DetailsView]()
	{
		DetailsView->ForceRefresh();
	});

	if (OptionsDialog->ShowModal() != 0)
	{
		return false;
	}

	Options->SaveConfig();

	// Make these configurable.
	// ReSharper disable CppTooWideScope
	static const TCHAR* DefaultSkeletonSuffix = TEXT("_Skel");
	static const TCHAR* DefaultSkeletalMeshSuffix = TEXT("_SkelMesh");

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	FReferenceSkeleton ReferenceSkeleton;
	USkeleton* Skeleton = nullptr;

	switch(Options->SkeletonImportOption)
	{
	case EReferenceSkeletonImportOption::CreateNew:
		{
			FString SkeletonName;
			FString SkeletonPackageName;
			AssetTools.CreateUniqueAssetName(InStaticMesh->GetOutermost()->GetName(), DefaultSkeletonSuffix, SkeletonPackageName, SkeletonName);

			USkeletonFromStaticMeshFactory* SkeletonFactory = NewObject<USkeletonFromStaticMeshFactory>();
			SkeletonFactory->StaticMesh = InStaticMesh;

			Skeleton = Cast<USkeleton>(AssetTools.CreateAsset(SkeletonName, FPackageName::GetLongPackagePath(SkeletonPackageName), USkeleton::StaticClass(), SkeletonFactory));
			ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
		}
		break;
		
	case EReferenceSkeletonImportOption::UseExistingSkeleton:
		{
			Skeleton = Cast<USkeleton>(Options->Skeleton.ResolveObject());
			if (!Skeleton)
			{
				UE_LOG(LogSkeletalMeshModelingToolsMeshConverter, Error, TEXT("No valid skeleton given as a reference. Aborting conversion."));
				return false;
			}
			ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
		}
		break;
	case EReferenceSkeletonImportOption::UseExistingSkeletalMesh:
		{
			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Options->SkeletalMesh.ResolveObject());
			if (!SkeletalMesh)
			{
				UE_LOG(LogSkeletalMeshModelingToolsMeshConverter, Error, TEXT("No valid skeletal mesh given as reference. Aborting conversion."));
				return false;
			}
			Skeleton = SkeletalMesh->GetSkeleton();
			ReferenceSkeleton = SkeletalMesh->GetRefSkeleton();
		}
		break;
	}

	USkeletalMesh *SkeletalMesh = nullptr;
	if (ensure(Skeleton) && ensure(ReferenceSkeleton.GetNum() != 0))
	{
		FString SkeletalMeshName;
		FString SkeletalMeshPackageName;
		AssetTools.CreateUniqueAssetName(InStaticMesh->GetOutermost()->GetName(), DefaultSkeletalMeshSuffix, SkeletalMeshPackageName, SkeletalMeshName);

		USkeletalMeshFromStaticMeshFactory* SkeletalMeshFactory = NewObject<USkeletalMeshFromStaticMeshFactory>();
		SkeletalMeshFactory->StaticMesh = InStaticMesh;
		SkeletalMeshFactory->ReferenceSkeleton = ReferenceSkeleton;
		SkeletalMeshFactory->Skeleton = Skeleton;
		SkeletalMeshFactory->BindBoneName = Options->BindingBoneName.BoneName; 

		SkeletalMesh = Cast<USkeletalMesh>(AssetTools.CreateAsset(SkeletalMeshName, FPackageName::GetLongPackagePath(SkeletalMeshPackageName), USkeletalMesh::StaticClass(), SkeletalMeshFactory));
	}

	if (ensure(SkeletalMesh))
	{
		if (Options->SkeletonImportOption == EReferenceSkeletonImportOption::CreateNew)
		{
			OutObjectsAdded.Add(Skeleton);
		}
		OutObjectsAdded.Add(SkeletalMesh);
	}

	return true;
}


void ConvertStaticMeshToSkeletalMeshInteractive(
	const TArray<FAssetData>& InStaticMeshAssets
	)
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	
	TArray<UObject*> ObjectsAdded;

	for (const FAssetData& AssetData: InStaticMeshAssets)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset());
		if (!ensure(StaticMesh))
		{
			continue;
		}

		if (!ConvertSingleMeshToSkeletalMeshInteractive(StaticMesh, ObjectsAdded))
		{
			break;
		}
	}
	
	AssetTools.SyncBrowserToAssets(ObjectsAdded);
}

#undef LOCTEXT_NAMESPACE
