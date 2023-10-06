// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserDefinedStructureCompilerUtils.h"

#include "Algo/Copy.h"
#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EdMode.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedStruct.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/StructureEditorUtils.h"
#include "KismetCompiler.h"
#include "KismetCompilerMisc.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Templates/UnrealTemplate.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Trace/Detail/Channel.h"
#include "UObject/Class.h"
#include "UObject/FieldIterator.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

#define LOCTEXT_NAMESPACE "StructureCompiler"

struct FUserDefinedStructureCompilerInner
{
	struct FBlueprintUserStructData
	{
		TArray<uint8> SkeletonCDOData;
		TArray<uint8> GeneratedCDOData;
	};

	static void ClearStructReferencesInBP(UBlueprint* FoundBlueprint, TMap<UBlueprint*, FBlueprintUserStructData>& BlueprintsToRecompile)
	{
		if (!BlueprintsToRecompile.Contains(FoundBlueprint))
		{
			FBlueprintUserStructData& BlueprintData = BlueprintsToRecompile.Add(FoundBlueprint);

			// Write CDO data to temp archive
			//FObjectWriter SkeletonMemoryWriter(FoundBlueprint->SkeletonGeneratedClass->GetDefaultObject(), BlueprintData.SkeletonCDOData);
			//FObjectWriter MemoryWriter(FoundBlueprint->GeneratedClass->GetDefaultObject(), BlueprintData.GeneratedCDOData);

			for (UFunction* Function : TFieldRange<UFunction>(FoundBlueprint->GeneratedClass, EFieldIteratorFlags::ExcludeSuper))
			{
				Function->Script.Empty();
			}
			FoundBlueprint->Status = BS_Dirty;
		}
	}

	static void ReplaceStructWithTempDuplicate(
		UUserDefinedStruct* StructureToReinstance, 
		TMap<UBlueprint*, FBlueprintUserStructData>& BlueprintsToRecompile,
		TArray<UUserDefinedStruct*>& ChangedStructs)
	{
		if (StructureToReinstance)
		{
			UUserDefinedStruct* DuplicatedStruct = NULL;
			{
				const FString ReinstancedName = FString::Printf(TEXT("STRUCT_REINST_%s"), *StructureToReinstance->GetName());
				const FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), UUserDefinedStruct::StaticClass(), FName(*ReinstancedName));

				TGuardValue<FIsDuplicatingClassForReinstancing, bool> IsDuplicatingClassForReinstancing(GIsDuplicatingClassForReinstancing, true);
				DuplicatedStruct = (UUserDefinedStruct*)StaticDuplicateObject(StructureToReinstance, GetTransientPackage(), UniqueName, ~RF_Transactional); 
			}

			DuplicatedStruct->Guid = StructureToReinstance->Guid;
			DuplicatedStruct->Bind();
			DuplicatedStruct->StaticLink(true);
			DuplicatedStruct->PrimaryStruct = StructureToReinstance;
			DuplicatedStruct->Status = EUserDefinedStructureStatus::UDSS_Duplicate;
			DuplicatedStruct->SetFlags(RF_Transient);
			DuplicatedStruct->AddToRoot();

			CastChecked<UUserDefinedStructEditorData>(DuplicatedStruct->EditorData)->RecreateDefaultInstance();

			// List of unique classes and structs to regenerate bytecode and property referenced objects list
			TSet<UStruct*> StructsToRegenerateReferencesFor;

			for (TAllFieldsIterator<FStructProperty> FieldIt(RF_NoFlags, EInternalObjectFlags::Garbage); FieldIt; ++FieldIt)
			{
				FStructProperty* StructProperty = *FieldIt;
				if (StructProperty && (StructureToReinstance == StructProperty->Struct))
				{
					if (UBlueprintGeneratedClass* OwnerClass = Cast<UBlueprintGeneratedClass>(StructProperty->GetOwnerClass()))
					{
						if (UBlueprint* FoundBlueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy))
						{
							ClearStructReferencesInBP(FoundBlueprint, BlueprintsToRecompile);
							StructProperty->Struct = DuplicatedStruct;
							StructsToRegenerateReferencesFor.Add(OwnerClass);
						}
					}
					else if (UUserDefinedStruct* OwnerStruct = Cast<UUserDefinedStruct>(StructProperty->GetOwnerStruct()))
					{
						check(OwnerStruct != DuplicatedStruct);
						const bool bValidStruct = (OwnerStruct->GetOutermost() != GetTransientPackage())
							&& IsValid(OwnerStruct)
							&& (EUserDefinedStructureStatus::UDSS_Duplicate != OwnerStruct->Status.GetValue());

						if (bValidStruct)
						{
							ChangedStructs.AddUnique(OwnerStruct);

							if (FStructureEditorUtils::FStructEditorManager::ActiveChange != FStructureEditorUtils::EStructureEditorChangeInfo::DefaultValueChanged)
							{
								// Don't change this for a default value only change, it won't get correctly replaced later
								StructProperty->Struct = DuplicatedStruct;
								StructsToRegenerateReferencesFor.Add(OwnerStruct);
							}
						}
					}
					else
					{
						UE_LOG(LogK2Compiler, Error, TEXT("ReplaceStructWithTempDuplicate unknown owner"));
					}
				}
			}

			// Make sure we update the list of objects referenced by structs after we replaced the struct in FStructProperties
			for (UStruct* Struct : StructsToRegenerateReferencesFor)
			{
				Struct->CollectBytecodeAndPropertyReferencedObjects();
			}

			DuplicatedStruct->RemoveFromRoot();

			for (UBlueprint* Blueprint : TObjectRange<UBlueprint>(RF_ClassDefaultObject, /** bIncludeDerivedClasses */ true, /** InternalExcludeFlags */ EInternalObjectFlags::Garbage))
			{
				if (Blueprint && !BlueprintsToRecompile.Contains(Blueprint))
				{
					FBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(Blueprint);
					if (Blueprint->CachedUDSDependencies.Contains(StructureToReinstance))
					{
						ClearStructReferencesInBP(Blueprint, BlueprintsToRecompile);
					}
				}
			}
		}
	}

	static void CleanAndSanitizeStruct(UUserDefinedStruct* StructToClean)
	{
		check(StructToClean);

		if (UUserDefinedStructEditorData* EditorData = Cast<UUserDefinedStructEditorData>(StructToClean->EditorData))
		{
			EditorData->CleanDefaultInstance();
		}

		if (FStructureEditorUtils::FStructEditorManager::ActiveChange != FStructureEditorUtils::EStructureEditorChangeInfo::DefaultValueChanged)
		{
			StructToClean->SetSuperStruct(nullptr);
			StructToClean->Children = nullptr;
			StructToClean->DestroyChildPropertiesAndResetPropertyLinks();
			StructToClean->Script.Empty();
			StructToClean->MinAlignment = 0;
			StructToClean->ScriptAndPropertyObjectReferences.Empty();
			StructToClean->ErrorMessage.Empty();
			StructToClean->SetStructTrashed(true);
		}
	}

	static void LogError(UUserDefinedStruct* Struct, FCompilerResultsLog& MessageLog, const FString& ErrorMsg)
	{
		MessageLog.Error(*ErrorMsg);
		if (Struct && Struct->ErrorMessage.IsEmpty())
		{
			Struct->ErrorMessage = ErrorMsg;
		}
	}

	static void CreateVariables(UUserDefinedStruct* Struct, const class UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog)
	{
		check(Struct && Schema);

		//FKismetCompilerUtilities::LinkAddedProperty push property to begin, so we revert the order
		for (int32 VarDescIdx = FStructureEditorUtils::GetVarDesc(Struct).Num() - 1; VarDescIdx >= 0; --VarDescIdx)
		{
			FStructVariableDescription& VarDesc = FStructureEditorUtils::GetVarDesc(Struct)[VarDescIdx];
			VarDesc.bInvalidMember = true;

			FEdGraphPinType VarType = VarDesc.ToPinType();

			FString ErrorMsg;
			if(!FStructureEditorUtils::CanHaveAMemberVariableOfType(Struct, VarType, &ErrorMsg))
			{
				LogError(
					Struct,
					MessageLog,
					FText::Format(
						LOCTEXT("StructureGeneric_ErrorFmt", "Structure: {0} Error: {1}"),
						FText::FromString(Struct->GetFullName()),
						FText::FromString(ErrorMsg)
					).ToString()
				);
				continue;
			}

			FProperty* VarProperty = nullptr;

			bool bIsNewVariable = false;
			if (FStructureEditorUtils::FStructEditorManager::ActiveChange == FStructureEditorUtils::EStructureEditorChangeInfo::DefaultValueChanged)
			{
				VarProperty = FindFProperty<FProperty>(Struct, VarDesc.VarName);
				if (!ensureMsgf(VarProperty, TEXT("Could not find the expected property (%s); was the struct (%s) unexpectedly sanitized?"), *VarDesc.VarName.ToString(), *Struct->GetName()))
				{
					VarProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Struct, VarDesc.VarName, VarType, NULL, CPF_None, Schema, MessageLog);
					bIsNewVariable = true;
				}
			}
			else
			{
				VarProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Struct, VarDesc.VarName, VarType, NULL, CPF_None, Schema, MessageLog);
				bIsNewVariable = true;
			}

			if (VarProperty == nullptr)
			{
				LogError(
					Struct,
					MessageLog,
					FText::Format(
						LOCTEXT("VariableInvalidType_ErrorFmt", "The variable {0} declared in {1} has an invalid type {2}"),
						FText::FromName(VarDesc.VarName),
						FText::FromString(Struct->GetName()),
						UEdGraphSchema_K2::TypeToText(VarType)
					).ToString()
				);
				continue;
			}
			else if (bIsNewVariable)
			{
				VarProperty->SetFlags(RF_LoadCompleted);
				FKismetCompilerUtilities::LinkAddedProperty(Struct, VarProperty);
			}
			
			VarProperty->SetPropertyFlags(CPF_Edit | CPF_BlueprintVisible);
			if (VarDesc.bDontEditOnInstance)
			{
				VarProperty->SetPropertyFlags(CPF_DisableEditOnInstance);
			}
			if (VarDesc.bEnableSaveGame)
			{
				VarProperty->SetPropertyFlags(CPF_SaveGame);
			}
			if (VarDesc.bEnableMultiLineText)
			{
				VarProperty->SetMetaData("MultiLine", TEXT("true"));
			}
			if (VarDesc.bEnable3dWidget)
			{
				VarProperty->SetMetaData(FEdMode::MD_MakeEditWidget, TEXT("true"));
			}
			VarProperty->SetMetaData(TEXT("DisplayName"), *VarDesc.FriendlyName);
			VarProperty->SetMetaData(FBlueprintMetadata::MD_Tooltip, *VarDesc.ToolTip);
			VarProperty->RepNotifyFunc = NAME_None;

			if (!VarDesc.DefaultValue.IsEmpty())
			{
				VarProperty->SetMetaData(TEXT("MakeStructureDefaultValue"), *VarDesc.DefaultValue);
			}
			VarDesc.CurrentDefaultValue = VarDesc.DefaultValue;

			VarDesc.bInvalidMember = false;

			if (VarProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference))
			{
				Struct->StructFlags = EStructFlags(Struct->StructFlags | STRUCT_HasInstancedReference);
			}

			if (VarType.PinSubCategoryObject.IsValid())
			{
				const UClass* ClassObject = Cast<UClass>(VarType.PinSubCategoryObject.Get());

				if (ClassObject && ClassObject->IsChildOf(AActor::StaticClass()) && (VarType.PinCategory == UEdGraphSchema_K2::PC_Object || VarType.PinCategory == UEdGraphSchema_K2::PC_Interface))
				{
					// NOTE: Right now the code that stops hard AActor references from being set in unsafe places is tied to this flag,
					// which is not generally respected in other places for struct properties
					VarProperty->PropertyFlags |= CPF_DisableEditOnTemplate;
				}
				else
				{
					// clear the disable-default-value flag that might have been present (if this was an AActor variable before)
					VarProperty->PropertyFlags &= ~(CPF_DisableEditOnTemplate);
				}
			}
		}
	}

	static void InnerCompileStruct(UUserDefinedStruct* Struct, const class UEdGraphSchema_K2* K2Schema, class FCompilerResultsLog& MessageLog)
	{
		check(Struct);
		const int32 ErrorNum = MessageLog.NumErrors;

		Struct->SetMetaData(FBlueprintMetadata::MD_Tooltip, *FStructureEditorUtils::GetTooltip(Struct));

		UUserDefinedStructEditorData* EditorData = CastChecked<UUserDefinedStructEditorData>(Struct->EditorData);

		CreateVariables(Struct, K2Schema, MessageLog);

		Struct->Bind();
		Struct->StaticLink(true);

		if (Struct->GetStructureSize() <= 0)
		{
			LogError(
				Struct,
				MessageLog,
				FText::Format(
					LOCTEXT("StructurEmpty_ErrorFmt", "Structure '{0}' is empty "),
					FText::FromString(Struct->GetFullName())
				).ToString()
			);
		}

		FString DefaultInstanceError;
		EditorData->RecreateDefaultInstance(&DefaultInstanceError);
		if (!DefaultInstanceError.IsEmpty())
		{
			LogError(Struct, MessageLog, DefaultInstanceError);
		}

		const bool bNoErrorsDuringCompilation = (ErrorNum == MessageLog.NumErrors);
		Struct->Status = bNoErrorsDuringCompilation ? EUserDefinedStructureStatus::UDSS_UpToDate : EUserDefinedStructureStatus::UDSS_Error;
	}

	static bool ShouldBeCompiled(const UUserDefinedStruct* Struct)
	{
		if (Struct && (EUserDefinedStructureStatus::UDSS_UpToDate == Struct->Status))
		{
			return false;
		}
		return true;
	}

	static void BuildDependencyMapAndCompile(const TArray<UUserDefinedStruct*>& ChangedStructs, FCompilerResultsLog& MessageLog)
	{
		struct FDependencyMapEntry
		{
			UUserDefinedStruct* Struct;
			TSet<UUserDefinedStruct*> StructuresToWaitFor;

			FDependencyMapEntry() : Struct(NULL) {}

			void Initialize(UUserDefinedStruct* ChangedStruct, const TArray<UUserDefinedStruct*>& AllChangedStructs) 
			{ 
				Struct = ChangedStruct;
				check(Struct);

				for (FStructVariableDescription& VarDesc : FStructureEditorUtils::GetVarDesc(Struct))
				{
					UUserDefinedStruct* StructType = Cast<UUserDefinedStruct>(VarDesc.SubCategoryObject.Get());
					if (StructType && (VarDesc.Category == UEdGraphSchema_K2::PC_Struct) && AllChangedStructs.Contains(StructType))
					{
						StructuresToWaitFor.Add(StructType);
					}
				}
			}
		};

		TArray<FDependencyMapEntry> DependencyMap;
		for (UUserDefinedStruct* ChangedStruct : ChangedStructs)
		{
			DependencyMap.Add(FDependencyMapEntry());
			DependencyMap.Last().Initialize(ChangedStruct, ChangedStructs);
		}

		while (DependencyMap.Num())
		{
			int32 StructureToCompileIndex = INDEX_NONE;
			for (int32 EntryIndex = 0; EntryIndex < DependencyMap.Num(); ++EntryIndex)
			{
				if(0 == DependencyMap[EntryIndex].StructuresToWaitFor.Num())
				{
					StructureToCompileIndex = EntryIndex;
					break;
				}
			}
			check(INDEX_NONE != StructureToCompileIndex);
			UUserDefinedStruct* Struct = DependencyMap[StructureToCompileIndex].Struct;
			check(Struct);

			FUserDefinedStructureCompilerInner::CleanAndSanitizeStruct(Struct);
			FUserDefinedStructureCompilerInner::InnerCompileStruct(Struct, GetDefault<UEdGraphSchema_K2>(), MessageLog);
			
			if (UUserDefinedStructEditorData* EditorData = Cast<UUserDefinedStructEditorData>(Struct->EditorData))
			{
				// Ensure that editor data is in sync w/ the current default instance (if valid) so that it can be reinitialized later.
				EditorData->RefreshValuesFromDefaultInstance();
			}

			DependencyMap.RemoveAtSwap(StructureToCompileIndex);

			for (FDependencyMapEntry& MapEntry : DependencyMap)
			{
				MapEntry.StructuresToWaitFor.Remove(Struct);
			}
		}
	}
};

void FUserDefinedStructureCompilerUtils::CompileStruct(class UUserDefinedStruct* Struct, class FCompilerResultsLog& MessageLog, bool bForceRecompile)
{
	if (FStructureEditorUtils::UserDefinedStructEnabled() && Struct)
	{
		TArray<UUserDefinedStruct*> ChangedStructs; 
		if (FUserDefinedStructureCompilerInner::ShouldBeCompiled(Struct) || bForceRecompile)
		{
			ChangedStructs.Add(Struct);
		}

		TMap<UBlueprint*, FUserDefinedStructureCompilerInner::FBlueprintUserStructData> BlueprintsToRecompile;
		for (int32 StructIdx = 0; StructIdx < ChangedStructs.Num(); ++StructIdx)
		{
			UUserDefinedStruct* ChangedStruct = ChangedStructs[StructIdx];
			if (ChangedStruct)
			{
				FStructureEditorUtils::BroadcastPreChange(ChangedStruct);
				FUserDefinedStructureCompilerInner::ReplaceStructWithTempDuplicate(ChangedStruct, BlueprintsToRecompile, ChangedStructs);
				ChangedStruct->Status = EUserDefinedStructureStatus::UDSS_Dirty;
			}
		}

		// COMPILE IN PROPER ORDER
		FUserDefinedStructureCompilerInner::BuildDependencyMapAndCompile(ChangedStructs, MessageLog);

		// UPDATE ALL THINGS DEPENDENT ON COMPILED STRUCTURES
		TSet<UScriptStruct*> ChangedStructsSet;
		ChangedStructsSet.Reserve(ChangedStructs.Num());
		Algo::Copy(ChangedStructs, ChangedStructsSet);
		TSet<UBlueprint*> BlueprintsThatHaveBeenRecompiled;
		FBlueprintEditorUtils::FindScriptStructsInNodes(ChangedStructsSet, [&BlueprintsThatHaveBeenRecompiled, &BlueprintsToRecompile](UBlueprint* Blueprint, UK2Node* Node)
			{
				// We need to recombine any nested subpins on this node, otherwise there will be an
				// unexpected amount of pins during reconstruction. 
				FBlueprintEditorUtils::RecombineNestedSubPins(Node);

				if (Blueprint)
				{
					// The blueprint skeleton needs to be updated before we reconstruct the node
					// or else we may have member references that point to the old skeleton
					if (!BlueprintsThatHaveBeenRecompiled.Contains(Blueprint))
					{
						BlueprintsThatHaveBeenRecompiled.Add(Blueprint);
						BlueprintsToRecompile.Remove(Blueprint);

						// Reapply CDO data

						FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
					}
					Node->ReconstructNode();
				}
			}
		);

		for (TPair<UBlueprint*, FUserDefinedStructureCompilerInner::FBlueprintUserStructData>& Pair : BlueprintsToRecompile)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Pair.Key);
		}

		for (UUserDefinedStruct* ChangedStruct : ChangedStructs)
		{
			if (ChangedStruct)
			{
				FStructureEditorUtils::BroadcastPostChange(ChangedStruct);
				ChangedStruct->MarkPackageDirty();
			}
		}
	}
}

void FUserDefinedStructureCompilerUtils::ReplaceStructWithTempDuplicateByPredicate(
	UUserDefinedStruct* StructureToReinstance,
	TFunctionRef<bool(FStructProperty* InStructProperty)> ShouldReplaceStructInStructProperty,
	TFunctionRef<void(UStruct* InStruct)> PostReplace)
{
	if (StructureToReinstance)
	{
		UUserDefinedStruct* DuplicatedStruct = NULL;
		{
			const FString ReinstancedName = FString::Printf(TEXT("STRUCT_REINST_%s"), *StructureToReinstance->GetName());
			const FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), UUserDefinedStruct::StaticClass(), FName(*ReinstancedName));

			TGuardValue<FIsDuplicatingClassForReinstancing, bool> IsDuplicatingClassForReinstancing(GIsDuplicatingClassForReinstancing, true);
			DuplicatedStruct = (UUserDefinedStruct*)StaticDuplicateObject(StructureToReinstance, GetTransientPackage(), UniqueName, ~RF_Transactional); 
		}

		DuplicatedStruct->Guid = StructureToReinstance->Guid;
		DuplicatedStruct->Bind();
		DuplicatedStruct->StaticLink(true);
		DuplicatedStruct->PrimaryStruct = StructureToReinstance;
		DuplicatedStruct->Status = EUserDefinedStructureStatus::UDSS_Duplicate;
		DuplicatedStruct->SetFlags(RF_Transient);
		DuplicatedStruct->AddToRoot();

		CastChecked<UUserDefinedStructEditorData>(DuplicatedStruct->EditorData)->RecreateDefaultInstance();

		// List of unique classes and structs to regenerate
		TSet<UStruct*> StructsToRegenerateReferencesFor;

		for (TAllFieldsIterator<FStructProperty> FieldIt(RF_NoFlags, EInternalObjectFlags::Garbage); FieldIt; ++FieldIt)
		{
			FStructProperty* StructProperty = *FieldIt;
			if (StructProperty && (StructureToReinstance == StructProperty->Struct))
			{
				if(ShouldReplaceStructInStructProperty(StructProperty))
				{
					StructProperty->Struct = DuplicatedStruct;
					StructsToRegenerateReferencesFor.Add(StructProperty->GetOwnerClass());
				}
			}
		}

		for (UStruct* Struct : StructsToRegenerateReferencesFor)
		{
			Struct->CollectBytecodeAndPropertyReferencedObjects();

			PostReplace(Struct);
		}

		// as property owners are re-created, the duplicated struct will be GCed
		DuplicatedStruct->RemoveFromRoot();
	}
}

#undef LOCTEXT_NAMESPACE
