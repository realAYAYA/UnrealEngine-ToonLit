// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationModifier.h"

#include "Algo/Transform.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetViewUtils.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Editor/Transactor.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/MessageDialog.h"
#include "Misc/OutputDeviceRedirector.h"
#include "ModifierOutputFilter.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Serialization/Archive.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectKey.h"
#include "UObject/Package.h"
#include "UObject/ReleaseObjectVersion.h"
#include "UObject/Script.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "AnimationModifier"

int32 UE::Anim::FApplyModifiersScope::ScopesOpened = 0;
TMap<FObjectKey, TOptional<EAppReturnType::Type>>  UE::Anim::FApplyModifiersScope::PerClassReturnTypeValues;

TOptional<EAppReturnType::Type> UE::Anim::FApplyModifiersScope::GetReturnType(const UAnimationModifier* InModifier)
{
	TOptional<EAppReturnType::Type>* ReturnTypePtr = PerClassReturnTypeValues.Find(FObjectKey(InModifier->GetClass()));
	return ReturnTypePtr ? *ReturnTypePtr : TOptional<EAppReturnType::Type>();
}

void UE::Anim::FApplyModifiersScope::SetReturnType(const UAnimationModifier* InModifier, EAppReturnType::Type InReturnType)
{
	const FObjectKey Key(InModifier->GetClass());
	ensure(!PerClassReturnTypeValues.Contains(Key));
	PerClassReturnTypeValues.Add(Key, InReturnType);
}

const FName UAnimationModifier::RevertModifierObjectName("REVERT_AnimationModifier");

UAnimationModifier::UAnimationModifier()
	: PreviouslyAppliedModifier(nullptr)
{
}

void UAnimationModifier::ApplyToAnimationSequence(class UAnimSequence* InAnimationSequence)
{
	FEditorScriptExecutionGuard ScriptGuard;

	CurrentAnimSequence = InAnimationSequence;
	checkf(CurrentAnimSequence, TEXT("Invalid Animation Sequence supplied"));
	CurrentSkeleton = InAnimationSequence->GetSkeleton();
	checkf(CurrentSkeleton, TEXT("Invalid Skeleton for supplied Animation Sequence"));

	// Filter to check for warnings / errors thrown from animation blueprint library (rudimentary approach for now)
	FCategoryLogOutputFilter OutputLog;
	OutputLog.SetAutoEmitLineTerminator(true);
	OutputLog.AddCategoryName("LogAnimationBlueprintLibrary");
	OutputLog.AddCategoryName("LogAnimation");

	GLog->AddOutputDevice(&OutputLog);
		
	// Transact the modifier to prevent instance variables/data to change during applying
	FTransaction ModifierTransaction;
	ModifierTransaction.SaveObject(this);

	FTransaction AnimationDataTransaction;
	AnimationDataTransaction.SaveObject(CurrentAnimSequence);
	AnimationDataTransaction.SaveObject(CurrentSkeleton);

	/** In case this modifier has been previously applied, revert it using the serialised out version at the time */	
	if (PreviouslyAppliedModifier)
	{
		PreviouslyAppliedModifier->Modify();
		PreviouslyAppliedModifier->OnRevert(CurrentAnimSequence);
	}

	IAnimationDataController& Controller = CurrentAnimSequence->GetController(); //-V595

	{
		IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ApplyModifierBracket", "Applying Animation Modifier"));
		/** Reverting and applying, populates the log with possible warnings and or errors to notify the user about */
		OnApply(CurrentAnimSequence);
	}

	// Apply transaction
	ModifierTransaction.BeginOperation();
	ModifierTransaction.Apply();
	ModifierTransaction.EndOperation();

	GLog->RemoveOutputDevice(&OutputLog);

	// Check if warnings or errors have occurred and show dialog to user to inform them about this
	const bool bWarnings = OutputLog.ContainsWarnings();
	const bool bErrors = OutputLog.ContainsErrors();
	bool bShouldRevert = bErrors;
	
	// If errors have occured - prompt user with OK and revert
	TOptional<EAppReturnType::Type> ScopeReturnType = UE::Anim::FApplyModifiersScope::GetReturnType(this);	
	static const FText MessageTitle = LOCTEXT("ModifierDialogTitle", "Modifier has generated errors during test run.");
	if (bErrors)
	{		
		if (UE::Anim::FApplyModifiersScope::ScopesOpened == 0 || !ScopeReturnType.IsSet() || ScopeReturnType.GetValue() != EAppReturnType::Type::Ok)
		{
			const FText ErrorMessageFormat = FText::FormatOrdered(LOCTEXT("ModifierErrorDescription", "Modifier: {0}\nAsset: {1}\n{2}\nResolve the errors before trying to apply again."), FText::FromString(GetClass()->GetPathName()),
				FText::FromString(CurrentAnimSequence->GetPathName()), FText::FromString(OutputLog));
			
			EAppReturnType::Type ReturnValue = FMessageDialog::Open(EAppMsgType::Ok, ErrorMessageFormat, &MessageTitle);
			UE::Anim::FApplyModifiersScope::SetReturnType(this, ReturnValue);
		}
	}

	// If _only_ warnings have occured - check if user has previously said YesAll / NoAll and process the result
	if (bWarnings && !bShouldRevert)
	{
		if (UE::Anim::FApplyModifiersScope::ScopesOpened == 0 || !ScopeReturnType.IsSet())
		{
			const FText WarningMessage = FText::FormatOrdered(LOCTEXT("ModifierWarningDescription", "Modifier: {0}\nAsset: {1}\n{2}\nAre you sure you want to apply it?"), FText::FromString(GetClass()->GetPathName()),
			FText::FromString(CurrentAnimSequence->GetPathName()), FText::FromString(OutputLog));

			EAppReturnType::Type ReturnValue = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAll, WarningMessage, &MessageTitle);
			bShouldRevert = ReturnValue == EAppReturnType::No || ReturnValue == EAppReturnType::NoAll;

			// check if user response should be stored for further modifier applications
			if(UE::Anim::FApplyModifiersScope::ScopesOpened > 0)
			{
				if (ReturnValue == EAppReturnType::Type::YesAll || ReturnValue == EAppReturnType::Type::NoAll)
				{
					UE::Anim::FApplyModifiersScope::SetReturnType(this, ReturnValue);
				}
			}
		}
		else
		{
			// Revert if previous user prompt return NoAll or if any errors occured previously 
			bShouldRevert = ScopeReturnType.GetValue() == EAppReturnType::NoAll || ScopeReturnType.GetValue() == EAppReturnType::Ok;
		}
	}

	// Revert changes if necessary, otherwise post edit and refresh animation data
	if (bShouldRevert)
	{
		AnimationDataTransaction.BeginOperation();
		AnimationDataTransaction.Apply();
		AnimationDataTransaction.EndOperation();
		CurrentAnimSequence->RefreshCacheData();
	}
	else
	{
		/** Mark the previous modifier pending kill, as it will be replaced with the current modifier state */
		if (PreviouslyAppliedModifier)
		{
			PreviouslyAppliedModifier->MarkAsGarbage();
		}

		PreviouslyAppliedModifier = DuplicateObject(this, GetOuter(), MakeUniqueObjectName(GetOuter(), GetClass(), RevertModifierObjectName));

		CurrentAnimSequence->PostEditChange();
		CurrentSkeleton->PostEditChange();
		CurrentAnimSequence->RefreshCacheData();

		UpdateStoredRevisions();
	}
		
	// Finished
	CurrentAnimSequence = nullptr;
	CurrentSkeleton = nullptr;
}

void UAnimationModifier::UpdateCompressedAnimationData()
{
	if (CurrentAnimSequence->DoesNeedRecompress())
	{
		CurrentAnimSequence->RequestSyncAnimRecompression(false);
	}
}

void UAnimationModifier::RevertFromAnimationSequence(class UAnimSequence* InAnimationSequence)
{
	FEditorScriptExecutionGuard ScriptGuard;

	/** Can only revert if previously applied, which means there should be a previous modifier */
	if (PreviouslyAppliedModifier)
	{
		checkf(InAnimationSequence, TEXT("Invalid Animation Sequence supplied"));
		CurrentAnimSequence = InAnimationSequence;
		CurrentSkeleton = InAnimationSequence->GetSkeleton();

		// Transact the modifier to prevent instance variables/data to change during reverting
		FTransaction Transaction;
		Transaction.SaveObject(this);

		PreviouslyAppliedModifier->Modify();

		IAnimationDataController& Controller = CurrentAnimSequence->GetController();

		{
			IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("RevertModifierBracket", "Reverting Animation Modifier"));
			PreviouslyAppliedModifier->OnRevert(CurrentAnimSequence);
		}

		// Apply transaction
		Transaction.BeginOperation();
		Transaction.Apply();
		Transaction.EndOperation();

	    CurrentAnimSequence->PostEditChange();
	    CurrentSkeleton->PostEditChange();
	    CurrentAnimSequence->RefreshCacheData();

		ResetStoredRevisions();

		// Finished
		CurrentAnimSequence = nullptr;
		CurrentSkeleton = nullptr;

		PreviouslyAppliedModifier->MarkAsGarbage();
		PreviouslyAppliedModifier = nullptr;
	}
}

bool UAnimationModifier::IsLatestRevisionApplied() const
{
	return (AppliedGuid == RevisionGuid);
}

void UAnimationModifier::PostInitProperties()
{
	Super::PostInitProperties();
	UpdateNativeRevisionGuid();
}

void UAnimationModifier::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	/** Back-wards compatibility, assume the current modifier as previously applied */
	if (Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::SerializeAnimModifierState)
	{
		PreviouslyAppliedModifier = DuplicateObject(this, GetOuter());
	}
}

void UAnimationModifier::PostLoad()
{
	Super::PostLoad();

	UClass* Class = GetClass();
	UObject* DefaultObject = Class->GetDefaultObject();

	// CDO, set GUID if invalid
	if(DefaultObject == this)
	{
		// Ensure we always have a valid guid
		if (!RevisionGuid.IsValid())
		{
			UpdateRevisionGuid(GetClass());
			MarkPackageDirty();
		}
	}
	// Non CDO, update revision GUID
	else if(UAnimationModifier* TypedDefaultObject = Cast<UAnimationModifier>(DefaultObject))
	{
		RevisionGuid = TypedDefaultObject->RevisionGuid;
	}
}

const USkeleton* UAnimationModifier::GetSkeleton()
{
	return CurrentSkeleton;
}

void UAnimationModifier::UpdateRevisionGuid(UClass* ModifierClass)
{
	if (ModifierClass)
	{
		RevisionGuid = FGuid::NewGuid();

		// Apply to any currently loaded instances of this class
		for (TObjectIterator<UAnimationModifier> It; It; ++It)
		{
			if (*It != this && It->GetClass() == ModifierClass)
			{
				It->SetInstanceRevisionGuid(RevisionGuid);
			}
		}
	}
}

void UAnimationModifier::UpdateNativeRevisionGuid()
{
	UClass* Class = GetClass();
	// Check if this is the class default object
	if (this == GetDefault<UAnimationModifier>(Class))
	{
		// If so check whether or not the config stored revision matches the natively defined one
		if (StoredNativeRevision != GetNativeClassRevision())
		{
			// If not update the blueprint revision GUID
			UpdateRevisionGuid(Class);
			StoredNativeRevision = GetNativeClassRevision();

			MarkPackageDirty();

			// Save the new native revision to config files
			SaveConfig();
			TryUpdateDefaultConfigFile();
		}
	}
}

void UAnimationModifier::ApplyToAll(TSubclassOf<UAnimationModifier> ModifierSubClass, bool bForceApply /*= true*/)
{
	if (UClass* ModifierClass = ModifierSubClass.Get())
	{
		// Make sure all packages (in this case UAnimSequences) are loaded to ensure the TObjectIterator has any instances to iterate over
		LoadModifierReferencers(ModifierSubClass);
		
		const FScopedTransaction Transaction(LOCTEXT("UndoAction_ApplyModifiers", "Applying Animation Modifier to Animation Sequence(s)"));		
		for (TObjectIterator<UAnimationModifier> It; It; ++It)
		{
			// Check if valid, of the required class, not pending kill and not a modifier back-up for reverting
			const bool bIsRevertModifierInstance = *It && It->GetFName().ToString().StartsWith(RevertModifierObjectName.ToString());
			if (*It && It->GetClass() == ModifierClass && IsValidChecked(*It) && !bIsRevertModifierInstance)
			{
				if (bForceApply || !It->IsLatestRevisionApplied())
				{
					// Go through outer chain to find AnimSequence
					UObject* Outer = It->GetOuter();

					while(Outer && !Outer->IsA<UAnimSequence>())
					{
						Outer = Outer->GetOuter();
					}

					if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Outer))
					{
						AnimSequence->Modify();
						It->ApplyToAnimationSequence(AnimSequence);
					}	
				}
			}
		}
	}	
}

void UAnimationModifier::LoadModifierReferencers(TSubclassOf<UAnimationModifier> ModifierSubClass)
{
	if (UClass* ModifierClass = ModifierSubClass.Get())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FName> PackageDependencies;
		AssetRegistryModule.GetRegistry().GetReferencers(ModifierClass->GetPackage()->GetFName(), PackageDependencies);

		TArray<FString> PackageNames;
		Algo::Transform(PackageDependencies, PackageNames, [](FName Name) { return Name.ToString(); });
		TArray<UPackage*> Packages = AssetViewUtils::LoadPackages(PackageNames);
	}
}

int32 UAnimationModifier::GetNativeClassRevision() const
{
	// Overriden in derrived classes to perform native revisioning
	return 0;
}

const UAnimSequence* UAnimationModifier::GetAnimationSequence()
{
	return CurrentAnimSequence;
}

void UAnimationModifier::UpdateStoredRevisions()
{
	AppliedGuid = RevisionGuid;
}

void UAnimationModifier::ResetStoredRevisions()
{
	AppliedGuid.Invalidate();
}

void UAnimationModifier::SetInstanceRevisionGuid(FGuid Guid)
{
	RevisionGuid = Guid;
}

#undef LOCTEXT_NAMESPACE
