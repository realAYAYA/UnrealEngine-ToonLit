// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraValidationRules.h"

#include "NiagaraClipboard.h"
#include "NiagaraComponentRendererProperties.h"
#include "NiagaraDataInterfaceCamera.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSettings.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSystemImpl.h"
#include "NiagaraSystemEditorData.h"
#include "DataInterface/NiagaraDataInterfaceActorComponent.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/Stack/NiagaraStackEmitterSettingsGroup.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/Stack/NiagaraStackSystemPropertiesItem.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"

#include "AssetToolsModule.h"
#include "Materials/MaterialInterface.h"

#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraValidationRules)

#define LOCTEXT_NAMESPACE "NiagaraValidationRules"

namespace NiagaraValidation
{
	template<typename T>
	TArray<T*> GetStackEntries(UNiagaraStackViewModel* StackViewModel, bool bRefresh = false)
	{
		TArray<T*> Results;
		TArray<UNiagaraStackEntry*> EntriesToCheck;
		if (UNiagaraStackEntry* RootEntry = StackViewModel->GetRootEntry())
		{
			if (bRefresh)
			{
				RootEntry->RefreshChildren();
			}
			RootEntry->GetUnfilteredChildren(EntriesToCheck);
		}
		while (EntriesToCheck.Num() > 0)
		{
			UNiagaraStackEntry* Entry = EntriesToCheck.Pop();
			if (T* ItemToCheck = Cast<T>(Entry))
			{
				Results.Add(ItemToCheck);
			}
			Entry->GetUnfilteredChildren(EntriesToCheck);
		}
		return Results;
	}

	template<typename T>
	TArray<T*> GetAllStackEntriesInSystem(TSharedPtr<FNiagaraSystemViewModel> ViewModel, bool bRefresh = false)
	{
		TArray<T*> Results;
		Results.Append(GetStackEntries<T>(ViewModel->GetSystemStackViewModel(), bRefresh));
		TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> EmitterHandleViewModels = ViewModel->GetEmitterHandleViewModels();
		for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel : EmitterHandleViewModels)
		{
			Results.Append(GetStackEntries<T>(EmitterHandleModel.Get().GetEmitterStackViewModel(), bRefresh));
		}
		return Results;
	}

	// helper function to retrieve a single stack entry from the system or emitter view model
	template<typename T>
	T* GetStackEntry(UNiagaraStackViewModel* StackViewModel, bool bRefresh = false)
	{
		TArray<T*> StackEntries = GetStackEntries<T>(StackViewModel, bRefresh);
		if (StackEntries.Num() > 0)
		{
			return StackEntries[0];
		}
		return nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------------
	// Common fixes and links

	void AddGoToFXTypeLink(FNiagaraValidationResult& Result, UNiagaraEffectType* FXType)
	{
		if (FXType == nullptr)
		{
			return;
		}

		FNiagaraValidationFix& GoToValidationRulesLink = Result.Links.AddDefaulted_GetRef();
		GoToValidationRulesLink.Description = LOCTEXT("GoToValidationRulesFix", "Go To Validation Rules");
		TWeakObjectPtr<UNiagaraEffectType> WeakFXType = FXType;
		GoToValidationRulesLink.FixDelegate = FNiagaraValidationFixDelegate::CreateLambda([WeakFXType]
			{
				FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
				TWeakPtr<IAssetTypeActions> WeakAssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(UNiagaraEffectType::StaticClass());

				if (UNiagaraEffectType* FXType = WeakFXType.Get())
				{
					if (TSharedPtr<IAssetTypeActions> AssetTypeActions = WeakAssetTypeActions.Pin())
					{
						TArray<UObject*> AssetsToEdit;
						AssetsToEdit.Add(FXType);
						AssetTypeActions->OpenAssetEditor(AssetsToEdit);
						//TODO: Is there a way for us to auto navigate to and open up the validation rules inside FXType?
					}
				}
			});
	}

	FString GetPlatformConflictsString(const FNiagaraPlatformSet& PlatformSetA, const FNiagaraPlatformSet& PlatformSetB, int MaxPlatformsToShow = 4)
	{
		TArray<const FNiagaraPlatformSet*> CheckSets;
		CheckSets.Add(&PlatformSetA);
		CheckSets.Add(&PlatformSetB);

		TArray<FNiagaraPlatformSetConflictInfo> ConflictInfos;
		FNiagaraPlatformSet::GatherConflicts(CheckSets, ConflictInfos);

		if (ConflictInfos.Num() > 0)
		{
			TSet<FName> BannedPlatformNames;
			for (const FNiagaraPlatformSetConflictInfo& ConflictInfo : ConflictInfos)
			{
				for (const FNiagaraPlatformSetConflictEntry& ConflictEntry : ConflictInfo.Conflicts)
				{
					BannedPlatformNames.Add(ConflictEntry.ProfileName);
				}
			}

			TStringBuilder<256> BannedPlatformsString;
			int NumFounds = 0;
			for (FName PlatformName : BannedPlatformNames)
			{
				if (NumFounds >= MaxPlatformsToShow)
				{
					BannedPlatformsString.Append(TEXT(", ..."));
					break;
				}
				if (NumFounds != 0)
				{
					BannedPlatformsString.Append(TEXT(", "));
				}
				++NumFounds;
				PlatformName.AppendString(BannedPlatformsString);
			}
			return BannedPlatformsString.ToString();
		}
		return FString();
	}

	TSharedPtr<FNiagaraEmitterHandleViewModel> GetEmitterViewModel(const FNiagaraValidationContext& Context, UNiagaraEmitter* NiagaraEmitter)
	{
		if (NiagaraEmitter == nullptr)
		{
			return nullptr;
		}

		const TSharedRef<FNiagaraEmitterHandleViewModel>* EmitterViewModel =
			Context.ViewModel->GetEmitterHandleViewModels().FindByPredicate(
				[NiagaraEmitter](const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterViewModelRef)
				{
					FNiagaraEmitterHandle* EmitterHandle = EmitterViewModelRef->GetEmitterHandle();
					return EmitterHandle && EmitterHandle->GetInstance().Emitter == NiagaraEmitter;
				}
			);

		if ( EmitterViewModel )
		{
			return *EmitterViewModel;
		}
		return nullptr;
	}

	TOptional<int32> GetModuleStaticInt32Value(const UNiagaraStackModuleItem* Module, FName ParameterName)
	{
		TArray<UNiagaraStackFunctionInput*> ModuleInputs;
		Module->GetParameterInputs(ModuleInputs);

		for (UNiagaraStackFunctionInput* Input : ModuleInputs)
		{
			if (Input->IsStaticParameter() && Input->GetInputParameterHandle().GetName() == ParameterName)
			{
				return TOptional<int32>(*(int32*)Input->GetLocalValueStruct()->GetStructMemory());
			}
		}
		return TOptional<int32>();
	}

	void SetModuleStaticInt32Value(UNiagaraStackModuleItem* Module, FName ParameterName, int32 NewValue)
	{
		TArray<UNiagaraStackFunctionInput*> ModuleInputs;
		Module->GetParameterInputs(ModuleInputs);

		for (UNiagaraStackFunctionInput* Input : ModuleInputs)
		{
			if (Input->IsStaticParameter() && Input->GetInputParameterHandle().GetName() == ParameterName)
			{
				TSharedRef<FStructOnScope> ValueStruct = MakeShared<FStructOnScope>(Input->GetLocalValueStruct()->GetStruct());
				*(int32*)ValueStruct->GetStructMemory() = NewValue;
				Input->SetLocalValue(ValueStruct);
			}
		}
	};

	bool StructContainsUObjectProperty(UStruct* Struct)
	{
		for (TFieldIterator<const FProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(Property))
			{
				// If we are an array change the property to be the inner one to check for struct / object
				Property = ArrayProperty->Inner;
			}

			if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property))
			{
				if (StructProperty->Struct)
				{
					if (StructContainsUObjectProperty(StructProperty->Struct))
					{
						return true;
					}
				}
			}
			else if (CastField<const FWeakObjectProperty>(Property) || CastField<const FObjectProperty>(Property) || CastField<const FSoftObjectProperty>(Property))
			{
				return true;
			}
		}
		return false;
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------

bool NiagaraValidation::HasValidationRules(UNiagaraSystem* NiagaraSystem)
{
	if ( NiagaraSystem != nullptr )
	{
		if (const UNiagaraEditorSettings* EditorSettings = GetDefault<UNiagaraEditorSettings>())
		{
			for (const TSoftObjectPtr<UNiagaraValidationRuleSet>& ValidationRuleSetPtr : EditorSettings->DefaultValidationRuleSets)
			{
				const UNiagaraValidationRuleSet* ValidationRuleSet = ValidationRuleSetPtr.LoadSynchronous();
				if (ValidationRuleSet != nullptr && ValidationRuleSet->ValidationRules.Num() > 0)
				{
					return true;
				}
			}
		}

		if (UNiagaraEffectType* EffectType = NiagaraSystem->GetEffectType())
		{
			if (EffectType->ValidationRules.Num() > 0)
			{
				return true;
			}

			for (UNiagaraValidationRuleSet* ValidationRuleSet : EffectType->ValidationRuleSets)
			{
				if (ValidationRuleSet != nullptr && ValidationRuleSet->ValidationRules.Num() > 0)
				{
					return true;
				}
			}
		}
	}
	return false;
}

// --------------------------------------------------------------------------------------------------------------------------------------------

void NiagaraValidation::ValidateAllRulesInSystem(TSharedPtr<FNiagaraSystemViewModel> SysViewModel, TFunction<void(const FNiagaraValidationResult& Result)> ResultCallback)
{
	if (SysViewModel == nullptr)
	{
		return;
	}

	FNiagaraValidationContext Context;
	Context.ViewModel = SysViewModel;
	TArray<FNiagaraValidationResult> NiagaraValidationResults;
	
	UNiagaraSystem& NiagaraSystem = SysViewModel->GetSystem();

	// Helper function
	const auto& ExecuteValidateRules =
		[&](TConstArrayView<TObjectPtr<UNiagaraValidationRule>> ValidationRules)
		{
			for (const UNiagaraValidationRule* ValidationRule : ValidationRules)
			{
				if (ValidationRule)
				{
					ValidationRule->CheckValidity(Context, NiagaraValidationResults);
				}
		}
	};

	// Validate Global Rules
	if (const UNiagaraEditorSettings* EditorSettings = GetDefault<UNiagaraEditorSettings>())
	{
		for (const TSoftObjectPtr<UNiagaraValidationRuleSet>& ValidationRuleSetPtr : EditorSettings->DefaultValidationRuleSets)
		{
			if (const UNiagaraValidationRuleSet* ValidationRuleSet = ValidationRuleSetPtr.LoadSynchronous())
			{
				ExecuteValidateRules(ValidationRuleSet->ValidationRules);
			}
		}
	}

	// Validate EffectType Rules
	if (UNiagaraEffectType* EffectType = NiagaraSystem.GetEffectType())
	{
		ExecuteValidateRules(EffectType->ValidationRules);
		for (UNiagaraValidationRuleSet* ValidationRuleSet : EffectType->ValidationRuleSets)
		{
			if (ValidationRuleSet != nullptr)
			{
				ExecuteValidateRules(ValidationRuleSet->ValidationRules);
			}
		}
	}

	// Validate Module Specific Rules
	TArray<UNiagaraStackModuleItem*> StackModuleItems =	NiagaraValidation::GetAllStackEntriesInSystem<UNiagaraStackModuleItem>(Context.ViewModel);
	for (UNiagaraStackModuleItem* Module : StackModuleItems)
	{
		if (Module && Module->GetIsEnabled())
		{
			if (UNiagaraScript* Script = Module->GetModuleNode().FunctionScript)
			{
				Context.Source = Module;
				for (UNiagaraValidationRule* ValidationRule : Script->ValidationRules)
				{
					if (ValidationRule)
					{
						ValidationRule->CheckValidity(Context, NiagaraValidationResults);
					}
				}
			}
		}
	}

	// process results
	for (const FNiagaraValidationResult& Result : NiagaraValidationResults)
	{
		ResultCallback(Result);
	}
}

void UNiagaraValidationRule_NoWarmupTime::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& Results)  const
{
	UNiagaraSystem& System = Context.ViewModel->GetSystem();
	if (System.NeedsWarmup())
	{
		UNiagaraStackSystemPropertiesItem* SystemProperties = NiagaraValidation::GetStackEntry<UNiagaraStackSystemPropertiesItem>(Context.ViewModel->GetSystemStackViewModel());
		FNiagaraValidationResult Result(ENiagaraValidationSeverity::Error, LOCTEXT("WarumupSummary", "Warmuptime > 0 is not allowed"), LOCTEXT("WarmupDescription", "Systems with the chosen effect type do not allow warmup time, as it costs too much performance.\nPlease set the warmup time to 0 in the system properties."), SystemProperties);
		Results.Add(Result);
	}
}

void UNiagaraValidationRule_FixedGPUBoundsSet::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& Results)  const
{
	// if the system has fixed bounds set then it overrides the emitter settings
	if (Context.ViewModel->GetSystem().bFixedBounds)
	{
		return;
	}

	// check that all the gpu emitters have fixed bounds set
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> EmitterHandleViewModels = Context.ViewModel->GetEmitterHandleViewModels();
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel : EmitterHandleViewModels)
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandleModel.Get().GetEmitterHandle()->GetEmitterData();
		if (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim && EmitterData->CalculateBoundsMode == ENiagaraEmitterCalculateBoundMode::Dynamic)
		{
			UNiagaraStackEmitterPropertiesItem* EmitterProperties = NiagaraValidation::GetStackEntry<UNiagaraStackEmitterPropertiesItem>(EmitterHandleModel.Get().GetEmitterStackViewModel());
			FNiagaraValidationResult Result(ENiagaraValidationSeverity::Error, LOCTEXT("GpuDynamicBoundsErrorSummary", "GPU emitters do not support dynamic bounds"), LOCTEXT("GpuDynamicBoundsErrorDescription", "Gpu emitter should either not be in dynamic mode or the system must have fixed bounds."), EmitterProperties);
			Results.Add(Result);
		}
	}
}

bool IsEnabledForMaxQualityLevel(FNiagaraPlatformSet Platforms, int32 MaxQualityLevel)
{
	for (int i = 0; i < MaxQualityLevel; i++)
	{
		if (Platforms.IsEnabledForQualityLevel(i))
		{
			return true;
		}
	}
	return false;
}

void UNiagaraValidationRule_BannedRenderers::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& Results)  const
{
	UNiagaraSystem& System = Context.ViewModel->GetSystem();
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> EmitterHandleViewModels = Context.ViewModel->GetEmitterHandleViewModels();
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel : EmitterHandleViewModels)
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandleModel.Get().GetEmitterHandle()->GetEmitterData();
		EmitterData->ForEachRenderer([&Results, EmitterHandleModel, this, &System](UNiagaraRendererProperties* RendererProperties)
		{
			if (RendererProperties->GetIsEnabled() && BannedRenderers.Contains(RendererProperties->GetClass()))
			{
				TArray<const FNiagaraPlatformSet*> CheckSets;
				CheckSets.Add(&Platforms);
				CheckSets.Add(&RendererProperties->Platforms);

				TArray<FNiagaraPlatformSetConflictInfo> Conflicts;
				FNiagaraPlatformSet::GatherConflicts(CheckSets, Conflicts);
				if (Conflicts.Num() > 0)
				{
					TArray<UNiagaraStackRendererItem*> RendererItems = NiagaraValidation::GetStackEntries<UNiagaraStackRendererItem>(EmitterHandleModel.Get().GetEmitterStackViewModel());
					for (UNiagaraStackRendererItem* Item : RendererItems)
					{
						if (Item->GetRendererProperties() != RendererProperties)
						{
							continue;
						}						
						FNiagaraValidationResult Result = Results.AddDefaulted_GetRef();
						
						Result.Severity = ENiagaraValidationSeverity::Warning;
						Result.SummaryText = LOCTEXT("BannedRenderSummary", "Banned renderers used.");
						Result.Description = LOCTEXT("BannedRenderDescription", "Please ensure only allowed renderers are used for each platform according to the validation rules in the System's Effect Type.");
						Result.SourceObject = Item;
						
						NiagaraValidation::AddGoToFXTypeLink(Result, System.GetEffectType());

						//Add autofix to disable the module
						FNiagaraValidationFix& DisableRendererFix = Result.Fixes.AddDefaulted_GetRef();
						DisableRendererFix.Description = LOCTEXT("DisableBannedRendererFix", "Disable Banned Renderer");
						TWeakObjectPtr<UNiagaraStackRendererItem> WeakRendererItem = Item;
						DisableRendererFix.FixDelegate = FNiagaraValidationFixDelegate::CreateLambda(
							[WeakRendererItem]()
							{
								if (UNiagaraStackRendererItem* RendererItem = WeakRendererItem.Get())
								{
									RendererItem->SetIsEnabled(false);
								}
							});

						Results.Add(Result);
					}
				}
			}
		});
	}
} 

void UNiagaraValidationRule_BannedModules::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& Results)  const
{
	UNiagaraSystem& System = Context.ViewModel->GetSystem();

	TArray<UNiagaraStackModuleItem*> StackModuleItems =	NiagaraValidation::GetAllStackEntriesInSystem<UNiagaraStackModuleItem>(Context.ViewModel);

	for (UNiagaraStackModuleItem* Item : StackModuleItems)
	{
		if (Item && Item->GetIsEnabled())
		{
			UNiagaraNodeFunctionCall& FuncCall = Item->GetModuleNode();

			for (UNiagaraScript* BannedModule : BannedModules)
			{
				if (BannedModule == FuncCall.FunctionScript)
				{
					FVersionedNiagaraEmitterData* EmitterData = Item->GetEmitterViewModel().IsValid() ? Item->GetEmitterViewModel()->GetEmitter().GetEmitterData() : nullptr;

					bool bApplyBan = true;
					if (EmitterData)
					{
						//If we're on an emitter, this emitter may be culled on the platforms the rule applies to.
						TArray<const FNiagaraPlatformSet*> CheckSets;
						CheckSets.Add(&Platforms);
						CheckSets.Add(&EmitterData->Platforms);
						TArray<FNiagaraPlatformSetConflictInfo> Conflicts;
						FNiagaraPlatformSet::GatherConflicts(CheckSets, Conflicts);
						bApplyBan = Conflicts.Num() > 0;
					}

					if (!bApplyBan)
					{
						continue;
					}

					const FTextFormat Format(LOCTEXT("BannedModuleFormat", "Module {0} is banned on some currently enabled platforms"));
					const FText WarningMessage = FText::Format(Format, FText::FromString(FuncCall.FunctionScript->GetName()));

					FNiagaraValidationResult& Result = Results.AddDefaulted_GetRef();
					Result.Severity = ENiagaraValidationSeverity::Warning;
					Result.SummaryText = WarningMessage;
					Result.Description = LOCTEXT("BanndeModulesDescription", "Check this module against the Effect Type's Banned Modules validators");
					Result.SourceObject = Item;

					NiagaraValidation::AddGoToFXTypeLink(Result, System.GetEffectType());

					//Add autofix to disable the module
					FNiagaraValidationFix& DisableModuleFix = Result.Fixes.AddDefaulted_GetRef();
					DisableModuleFix.Description = LOCTEXT("DisableBannedModuleFix", "Disable Banned Module");					
					TWeakObjectPtr<UNiagaraStackModuleItem> WeakModuleItem = Item;
					DisableModuleFix.FixDelegate = FNiagaraValidationFixDelegate::CreateLambda([WeakModuleItem]()
					{
						if (UNiagaraStackModuleItem* ModuleItem = WeakModuleItem.Get())
						{
							ModuleItem->SetEnabled(false);
						}
					});
				}
			}
		}
	}
}

void UNiagaraValidationRule_BannedDataInterfaces::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& Results)  const
{
	UNiagaraSystem* NiagaraSystem = &Context.ViewModel->GetSystem();

	FNiagaraDataInterfaceUtilities::ForEachDataInterface(
		NiagaraSystem,
		[&](const FNiagaraDataInterfaceUtilities::FDataInterfaceUsageContext& UsageContext) -> bool
		{
			UClass* DIClass = UsageContext.DataInterface->GetClass();
			if (BannedDataInterfaces.Contains(DIClass) == false)
			{
				return true;
			}

			static const FText WarningFormat(LOCTEXT("BannedDataInteraceFormatWarn", "DataInterface '{0}' is banned on currently enabled platforms"));
			static const FText SystemDescFormat(LOCTEXT("BannedDataInteraceSystemFormatDesc", "DataInterface '{0} - {1}' is banned on currently enabled platforms"));
			static const FText EmitterDescFormat(LOCTEXT("BannedDataInteraceEmitterFormatDesc", "DataInterface '{0} - {1}' is banned on currently enabled platforms '{2}'"));

			UObject* WarningObject = nullptr;
			if (UNiagaraEmitter* NiagaraEmitter = Cast<UNiagaraEmitter>(UsageContext.OwnerObject))
			{
				const TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterViewModel = NiagaraValidation::GetEmitterViewModel(Context, NiagaraEmitter);
				if (EmitterViewModel == nullptr)
				{
					return true;
				}

				const FVersionedNiagaraEmitterData* EmitterData = EmitterViewModel->GetEmitterHandle()->GetEmitterData();
				if (EmitterData == nullptr)
				{
					return true;
				}

				const bool bIsBanEnabled =
					((EmitterData->SimTarget == ENiagaraSimTarget::CPUSim) && bBanOnCpu) ||
					((EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim) && bBanOnGpu);

				if (bIsBanEnabled)
				{
					FString PlatformConflictsString = NiagaraValidation::GetPlatformConflictsString(Platforms, EmitterData->Platforms);
					if (PlatformConflictsString.IsEmpty() == false)
					{
						Results.Emplace(
							Severity,
							FText::Format(WarningFormat, FText::FromName(UsageContext.Variable.GetName())),
							FText::Format(EmitterDescFormat, FText::FromName(UsageContext.Variable.GetName()), FText::FromName(DIClass->GetFName()), FText::FromString(PlatformConflictsString)),
							NiagaraValidation::GetStackEntry<UNiagaraStackEmitterPropertiesItem>(EmitterViewModel->GetEmitterStackViewModel())
						);
					}
				}
			}
			else if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(UsageContext.OwnerObject))
			{
				if (bBanOnCpu == true)
				{
					Results.Emplace(
						ENiagaraValidationSeverity::Warning,
						FText::Format(WarningFormat, FText::FromName(UsageContext.Variable.GetName())),
						FText::Format(SystemDescFormat, FText::FromName(UsageContext.Variable.GetName()), FText::FromName(DIClass->GetFName())),
						NiagaraValidation::GetStackEntry<UNiagaraStackSystemPropertiesItem>(Context.ViewModel->GetSystemStackViewModel())
					);
				}
			}

			return true;
		}
	);
}

void UNiagaraValidationRule_GpuUsage::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const
{
	UNiagaraSystem& System = Context.ViewModel->GetSystem();
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel : Context.ViewModel->GetEmitterHandleViewModels())
	{
		FNiagaraEmitterHandle* EmitterHandle = EmitterHandleModel.Get().GetEmitterHandle();
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle->GetEmitterData();
		if (EmitterData->SimTarget != ENiagaraSimTarget::GPUComputeSim)
		{
			continue;
		}

		const FString PlatformConflictsString = NiagaraValidation::GetPlatformConflictsString(Platforms, EmitterData->Platforms);
		if (PlatformConflictsString.IsEmpty())
		{
			continue;
		}

		FNiagaraValidationResult& ValidationResult = OutResults.Emplace_GetRef(
			Severity,
			LOCTEXT("GpuUsageInfo", "GPU usage may not function as expected"),
			FText::Format(LOCTEXT("GpuUsageInfoDetails", "GPU usage may not function as expected on '{0}'."), FText::FromString(PlatformConflictsString)),
			NiagaraValidation::GetStackEntry<UNiagaraStackEmitterPropertiesItem>(EmitterHandleModel->GetEmitterStackViewModel())
		);
		
		ValidationResult.Fixes.Emplace(
			FText::Format(LOCTEXT("GpuUsageInfoFix_DisablePlatforms", "Disable emitter on '{0}'."), FText::FromString(PlatformConflictsString)),
			FNiagaraValidationFixDelegate::CreateLambda(
				[WeakEmitterPtr=EmitterHandle->GetInstance().ToWeakPtr(), PlatformsToDisable=Platforms]()
				{
					FVersionedNiagaraEmitter VersionedEmitter = WeakEmitterPtr.ResolveWeakPtr();
					if (FVersionedNiagaraEmitterData* VersionedEmitterData = VersionedEmitter.GetEmitterData())
					{
						TArray<FNiagaraPlatformSetConflictInfo> ConflictInfos;
						FNiagaraPlatformSet::GatherConflicts({&VersionedEmitterData->Platforms, &PlatformsToDisable}, ConflictInfos);

						for (const FNiagaraPlatformSetConflictInfo& ConflictInfo : ConflictInfos)
						{
							for (const FNiagaraPlatformSetConflictEntry& ConflictEntry : ConflictInfo.Conflicts)
							{
								UDeviceProfile* DeviceProfile = UDeviceProfileManager::Get().FindProfile(ConflictEntry.ProfileName.ToString());
								for (int32 iQualityLevel=0; iQualityLevel < 32; ++iQualityLevel)
								{
									if ( (ConflictEntry.QualityLevelMask & (1 << iQualityLevel)) != 0 )
									{
										VersionedEmitterData->Platforms.SetDeviceProfileState(DeviceProfile, iQualityLevel, ENiagaraPlatformSelectionState::Disabled);
									}
								}
							}
						}
					}
				}
			)
		);
			
		ValidationResult.Fixes.Emplace(
			LOCTEXT("GpuUsageInfoFix_SwitchToCput", "Set emitter to CPU"),
			FNiagaraValidationFixDelegate::CreateLambda(
				[WeakEmitterPtr=EmitterHandle->GetInstance().ToWeakPtr()]()
				{
					FVersionedNiagaraEmitter VersionedEmitter = WeakEmitterPtr.ResolveWeakPtr();
					if (FVersionedNiagaraEmitterData* VersionedEmitterData = VersionedEmitter.GetEmitterData())
					{
						VersionedEmitterData->SimTarget = ENiagaraSimTarget::CPUSim;

						FProperty* SimTargetProperty = FindFProperty<FProperty>(FVersionedNiagaraEmitterData::StaticStruct(), GET_MEMBER_NAME_CHECKED(FVersionedNiagaraEmitterData, SimTarget));
						FPropertyChangedEvent PropertyChangedEvent(SimTargetProperty);
						VersionedEmitter.Emitter->PostEditChangeVersionedProperty(PropertyChangedEvent, VersionedEmitter.Version);
					}
				}
			)
		);
	}
}

void UNiagaraValidationRule_InvalidEffectType::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& Results)  const
{
	UNiagaraStackSystemPropertiesItem* SystemProperties = NiagaraValidation::GetStackEntry<UNiagaraStackSystemPropertiesItem>(Context.ViewModel->GetSystemStackViewModel());
	FNiagaraValidationResult Result(ENiagaraValidationSeverity::Error, LOCTEXT("InvalidEffectSummary", "Invalid Effect Type"), LOCTEXT("InvalidEffectDescription", "The effect type on this system was marked as invalid for production content and should only be used as placeholder."), SystemProperties);
	Results.Add(Result);
}

void UNiagaraValidationRule_LWC::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& Results)  const
{
	const UNiagaraSettings* Settings = GetDefault<UNiagaraSettings>();
	UNiagaraSystem& System = Context.ViewModel->GetSystem();
	if (!System.SupportsLargeWorldCoordinates())
	{
		return;
	}

	// gather all the modules in the system, excluding localspace emitters
	TArray<UNiagaraStackModuleItem*> AllModules;
	AllModules.Append(NiagaraValidation::GetStackEntries<UNiagaraStackModuleItem>(Context.ViewModel->GetSystemStackViewModel()));
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> EmitterHandleViewModels = Context.ViewModel->GetEmitterHandleViewModels();
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleModel : EmitterHandleViewModels)
	{
		if (EmitterHandleModel->GetEmitterHandle()->GetEmitterData()->bLocalSpace == false)
		{
			AllModules.Append(NiagaraValidation::GetStackEntries<UNiagaraStackModuleItem>(EmitterHandleModel.Get().GetEmitterStackViewModel()));
		}
	}

	for (UNiagaraStackModuleItem* Module : AllModules)
	{
		TArray<UNiagaraStackFunctionInput*> StackInputs;
		Module->GetParameterInputs(StackInputs);
		
		for (UNiagaraStackFunctionInput* Input : StackInputs)
		{
			if (Input->GetInputType() == FNiagaraTypeDefinition::GetPositionDef())
			{
				// check if any position inputs are set locally to absolute values
				if (Input->GetValueMode() == UNiagaraStackFunctionInput::EValueMode::Local)
				{
					FNiagaraValidationResult Result(ENiagaraValidationSeverity::Warning, FText::Format(LOCTEXT("LocalPosInputSummary", "Input '{0}' set to absolute value"), Input->GetDisplayName()), LOCTEXT("LocalPosInputDescription", "Position attributes should never be set to an absolute values, because they will be offset when using large world coordinates.\nInstead, set them relative to a known position like Engine.Owner.Position."), Input);
					Results.Add(Result);
				}

				// check if the linked dynamic input script outputs a vector
				if (Input->GetValueMode() == UNiagaraStackFunctionInput::EValueMode::Dynamic && Input->GetDynamicInputNode() && Settings->bEnforceStrictStackTypes)
				{
					if (UNiagaraScriptSource* DynamicInputSource = Cast<UNiagaraScriptSource>(Input->GetDynamicInputNode()->GetFunctionScriptSource()))
					{
						TArray<FNiagaraVariable> OutNodes;
						DynamicInputSource->NodeGraph->GetOutputNodeVariables(OutNodes);
						for (const FNiagaraVariable& OutVariable : OutNodes)
						{
							if (OutVariable.GetType() == FNiagaraTypeDefinition::GetVec3Def())
							{
								FTextFormat DescriptionFormat = LOCTEXT("VecDILinkedToPosInputDescription", "The position input {0} is linked to a dynamic input that outputs a vector.\nPlease use a dynamic input that outputs a position instead or explicitly convert the vector to a position type.");
								FNiagaraValidationResult Result(ENiagaraValidationSeverity::Warning, LOCTEXT("VecDILinkedToPosInputSummary", "Position input is linked to a vector output"), FText::Format(DescriptionFormat, Input->GetDisplayName()), Input);
								Results.Add(Result);
							}
						}
					}
				}

				// check if the linked input variable is a vector
				if (Input->GetValueMode() == UNiagaraStackFunctionInput::EValueMode::Linked && Settings->bEnforceStrictStackTypes)
				{
					FNiagaraVariable VectorVar(FNiagaraTypeDefinition::GetVec3Def(), Input->GetLinkedValueHandle().GetParameterHandleString());
					const UNiagaraGraph* NiagaraGraph = Input->GetInputFunctionCallNode().GetNiagaraGraph();

					// we check if metadata for a vector attribute with the linked name exists in the emitter/system script graph. Not 100% correct, but it needs to be fast and a few false negatives are acceptable.
					if (NiagaraGraph && NiagaraGraph->GetMetaData(VectorVar).IsSet())
					{
						FNiagaraValidationResult Result(ENiagaraValidationSeverity::Warning, FText::Format(LOCTEXT("PositionLinkedVectorSummary", "Input '{0}' is linked to a vector attribute"), Input->GetDisplayName()), LOCTEXT("PositionLinkedVectorDescription", "Position types should only be linked to position attributes. In this case, it is linked to a vector attribute and the implicit conversion can cause problems with large world coordinates."), Input);
						Results.Add(Result);
					}
				}
			}
		}
	}
}

void UNiagaraValidationRule_NoOpaqueRenderMaterial::CheckValidity(const FNiagaraValidationContext& Context,	TArray<FNiagaraValidationResult>& Results) const
{
	// check that we are called from a valid module
	UNiagaraStackModuleItem* SourceModule = Cast<UNiagaraStackModuleItem>(Context.Source);
	if (SourceModule && SourceModule->GetIsEnabled() && SourceModule->GetEmitterViewModel())
	{
		static const FName NAME_GPUCollisionType("GPU Collision Type");
		static const FName NAME_ZDepthQueryType("Z Depth Query Type");
		
		// search for the right emitter view model
		for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterHandleModel : Context.ViewModel->GetEmitterHandleViewModels())
		{
			FVersionedNiagaraEmitterData* EmitterData = EmitterHandleModel->GetEmitterHandle()->GetEmitterData();
			if (EmitterHandleModel->GetIsEnabled() && EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim && EmitterHandleModel->GetEmitterViewModel() == SourceModule->GetEmitterViewModel())
			{
				// Note: for these BP driven enums we can't compare the values
				TOptional<int32> GPUCollisionType = NiagaraValidation::GetModuleStaticInt32Value(SourceModule, NAME_GPUCollisionType);
				if (!GPUCollisionType.IsSet() || GPUCollisionType.GetValue() != 0)
				{
					continue;
				}
				TOptional<int32> ZDepthQueryType = NiagaraValidation::GetModuleStaticInt32Value(SourceModule, NAME_ZDepthQueryType);
				if (!ZDepthQueryType.IsSet() || ZDepthQueryType.GetValue() != 0)
				{
					continue;
				}

				// check the renderers
				TArray<UNiagaraStackRendererItem*> RendererItems = NiagaraValidation::GetStackEntries<UNiagaraStackRendererItem>(EmitterHandleModel->GetEmitterStackViewModel());
				for (UNiagaraStackRendererItem* Renderer : RendererItems)
				{
					if (UNiagaraRendererProperties* RendererProperties = Renderer->GetRendererProperties())
					{
						TArray<UMaterialInterface*> OutMaterials;
						RendererProperties->GetUsedMaterials(nullptr, OutMaterials);
						for (UMaterialInterface* Material : OutMaterials)
						{
							if (!Material)
							{
								continue;
							}
							
							if (IsOpaqueOrMaskedBlendMode(*Material))
							{
								FText Description = LOCTEXT("NoOpaqueRenderMaterialDescription", "This renderer uses a material with a masked or opaque blend mode, which writes to the depth buffer.\nThis will cause conflicts when the collision module also uses depth buffer collisions.");
								FNiagaraValidationResult Result(ENiagaraValidationSeverity::Warning, FText::Format(LOCTEXT("NoOpaqueRenderMaterialSummary", "Renderer '{0}' has an opaque material"), Renderer->GetDisplayName()), Description, Renderer);
								
								//Add autofix to switch to distance field collisions if possible
								static const auto* CVarGenerateMeshDistanceFields = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
								if (CVarGenerateMeshDistanceFields != nullptr && CVarGenerateMeshDistanceFields->GetValueOnGameThread() > 0)
								{
									FNiagaraValidationFix& DisableRendererFix = Result.Fixes.AddDefaulted_GetRef();
									DisableRendererFix.Description = LOCTEXT("SwitchCollisionFix", "Change collision type to distance fields");
									TWeakObjectPtr<UNiagaraStackModuleItem> WeakSourceModule = SourceModule;
								
									DisableRendererFix.FixDelegate = FNiagaraValidationFixDelegate::CreateLambda(
										[WeakSourceModule]()
										{
											if (UNiagaraStackModuleItem* CollisionModule = WeakSourceModule.Get())
											{
												NiagaraValidation::SetModuleStaticInt32Value(CollisionModule, NAME_GPUCollisionType, 1);
											}
										});
								}
								Results.Add(Result);
							}
						}
					}
				}
			}
		}
	}
}

void UNiagaraValidationRule_NoFixedDeltaTime::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const
{
	// check to see if we're called from a module or the effect type
	if (UNiagaraStackModuleItem* SourceModule = Cast<UNiagaraStackModuleItem>(Context.Source))
	{
		if (SourceModule->GetIsEnabled())
		{
			UNiagaraSystem& System = SourceModule->GetSystemViewModel()->GetSystem();
			if (System.HasFixedTickDelta())
			{
				OutResults.Emplace_GetRef(
					ENiagaraValidationSeverity::Warning,
					LOCTEXT("NoFixedDeltaTimeModule", "Module does not support fixed tick delta time"),
					LOCTEXT("NoFixedDeltaTimeModuleDetailed", "This system uses a fixed tick delta time, which means it might tick multiple times per frame or might skip ticks depending on the global tick rate.\nModules that depend on external assets such as render targets or collision data will NOT work correctly when their tick is different from the engine tick.\nConsider disabling the fixed tick delta time."),
					SourceModule
				);
			}
		}
	}
	else
	{
		UNiagaraSystem& System = Context.ViewModel->GetSystem();
		if (System.HasFixedTickDelta())
		{
			OutResults.Emplace_GetRef(
				ENiagaraValidationSeverity::Error,
				LOCTEXT("NoFixedDeltaTime", "Effect type does not allow fixed tick delta time"),
				LOCTEXT("NoFixedDeltaTimeDetailed", "This system uses a fixed tick delta time, which means it might tick multiple times per frame or might skip ticks depending on the global tick rate.\nThe selected effect type does not allow fixed tick delta times."),
				NiagaraValidation::GetStackEntry<UNiagaraStackSystemPropertiesItem>(Context.ViewModel->GetSystemStackViewModel())
			);
		}
	}
}

void UNiagaraValidationRule_SimulationStageBudget::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const
{
	for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterHandleModel : Context.ViewModel->GetEmitterHandleViewModels())
	{
		// Skip disabled
		if ( EmitterHandleModel->GetIsEnabled() == false )
		{	
			continue;
		}

		// Simulation stages are GPU only currently
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandleModel.Get().GetEmitterHandle()->GetEmitterData();
		if ( EmitterData->SimTarget != ENiagaraSimTarget::GPUComputeSim )
		{
			continue;
		}

		int32 TotalIterations = 0;
		int32 TotalEnabledStages = 0;
		for ( UNiagaraSimulationStageBase* SimStageBase : EmitterData->GetSimulationStages() )
		{
			UNiagaraSimulationStageGeneric* SimStage = Cast<UNiagaraSimulationStageGeneric>(SimStageBase);
			if ( SimStage == nullptr || SimStage->bEnabled == false )
			{
				continue;
			}

			const int32 StageNumIterations = SimStage->NumIterations.GetDefaultValue<int32>();
			++TotalEnabledStages;
			TotalIterations += StageNumIterations;
			if ( bMaxIterationsPerStageEnabled && StageNumIterations > MaxIterationsPerStage)
			{
				UNiagaraStackEmitterPropertiesItem* EmitterProperties = NiagaraValidation::GetStackEntry<UNiagaraStackEmitterPropertiesItem>(EmitterHandleModel.Get().GetEmitterStackViewModel());
				OutResults.Emplace_GetRef(
					Severity,
					FText::Format(LOCTEXT("SimStageTooManyIterationsFormat", "Simulation Stage '{0}' has too many iterations"), FText::FromName(SimStage->SimulationStageName)),
					FText::Format(LOCTEXT("SimStageTooManyIterationsDetailedFormat", "Simulation Stage '{0}' has {1} iterations and we only allow {2}"), FText::FromName(SimStage->SimulationStageName), FText::AsNumber(StageNumIterations), FText::AsNumber(MaxIterationsPerStage)),
					EmitterProperties
				);
			}
		}

		if ( bMaxTotalIterationsEnabled && TotalIterations > MaxTotalIterations )
		{
			UNiagaraStackEmitterPropertiesItem* EmitterProperties = NiagaraValidation::GetStackEntry<UNiagaraStackEmitterPropertiesItem>(EmitterHandleModel.Get().GetEmitterStackViewModel());
			OutResults.Emplace(
				Severity,
				LOCTEXT("SimStageTooManyTotalIterationsFormat", "Emitter has too many total simulation stage iterations"),
				FText::Format(LOCTEXT("SimStageTooManyTotalIterationsDetailedFormat", "Emitter has {0} total simulation stage iterations and we only allow {1}"), FText::AsNumber(TotalIterations), FText::AsNumber(MaxTotalIterations)),
				EmitterProperties
			);
		}

		if ( bMaxSimulationStagesEnabled && TotalEnabledStages > MaxSimulationStages )
		{
			UNiagaraStackEmitterPropertiesItem* EmitterProperties = NiagaraValidation::GetStackEntry<UNiagaraStackEmitterPropertiesItem>(EmitterHandleModel.Get().GetEmitterStackViewModel());
			OutResults.Emplace(
				Severity,
				LOCTEXT("TooManySimStagesFormat", "Emitter has too many simulation stages"),
				FText::Format(LOCTEXT("TooManySimStagesDetailedFormat", "Emitter has {0} simulation stages active and we only allow {1}"), FText::AsNumber(TotalEnabledStages), FText::AsNumber(MaxSimulationStages)),
				EmitterProperties
			);
		}
	}
}

void UNiagaraValidationRule_TickDependencyCheck::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const
{
	UNiagaraSystem* NiagaraSystem = &Context.ViewModel->GetSystem();

	if (!bCheckActorComponentInterface && !bCheckCameraDataInterface && !bCheckSkeletalMeshInterface)
	{
		return;
	}

	NiagaraSystem->ForEachScript(
		[&](UNiagaraScript* NiagaraScript)
		{
			const TArray<FNiagaraScriptDataInterfaceInfo>& CachedDefaultDIs = NiagaraScript->GetCachedDefaultDataInterfaces();
			for ( const FNiagaraScriptDataInterfaceInfo& CachedDefaultDI : CachedDefaultDIs )
			{
				bool bWarnTickDependency = false;
				if (UNiagaraDataInterfaceCamera* CameraDataInterface = Cast<UNiagaraDataInterfaceCamera>(CachedDefaultDI.DataInterface))
				{
					bWarnTickDependency = bCheckCameraDataInterface && CameraDataInterface->bRequireCurrentFrameData;
				}
				else if (UNiagaraDataInterfaceSkeletalMesh* SkeletalMeshDataInterface = Cast<UNiagaraDataInterfaceSkeletalMesh>(CachedDefaultDI.DataInterface))
				{
					bWarnTickDependency = bCheckSkeletalMeshInterface && SkeletalMeshDataInterface->bRequireCurrentFrameData;
				}
				else if (UNiagaraDataInterfaceActorComponent* ActorComponentDataInterface = Cast<UNiagaraDataInterfaceActorComponent>(CachedDefaultDI.DataInterface))
				{
					bWarnTickDependency = bCheckActorComponentInterface && ActorComponentDataInterface->bRequireCurrentFrameData;
				}

				if (bWarnTickDependency)
				{
					UNiagaraEmitter* NiagaraEmitter = NiagaraScript->GetTypedOuter<UNiagaraEmitter>();

					UObject* StackObject = nullptr;
					if ( NiagaraEmitter )
					{
						// If we are attatched to an emitter than is not enabled bail
						const TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterViewModel = NiagaraValidation::GetEmitterViewModel(Context, NiagaraEmitter);
						if (EmitterViewModel == nullptr || EmitterViewModel->GetIsEnabled() == false)
						{
							return;
						}
						StackObject = NiagaraValidation::GetStackEntry<UNiagaraStackEmitterPropertiesItem>(EmitterViewModel->GetEmitterStackViewModel());
					}
					else
					{
						StackObject = NiagaraValidation::GetStackEntry<UNiagaraStackSystemPropertiesItem>(Context.ViewModel->GetSystemStackViewModel());
					}

					const FText OwnerStackText = FText::FromString(NiagaraEmitter ? NiagaraEmitter->GetName() : FString(TEXT("System")));
					const FText DIClassText = FText::FromName(CachedDefaultDI.DataInterface->GetClass()->GetFName());
					const FText VariableText = FText::FromName(CachedDefaultDI.Name);
					OutResults.Emplace(
						Severity,
						FText::Format(LOCTEXT("TickDependencyCheckFormat", "'{0}' has a tick dependency that can potentially be removed"), OwnerStackText),
						FText::Format(LOCTEXT("TickDependencyCheckDetailedFormat", "'{0}' has a tick dependency from data interace '{1}' variable '{2}' that can removed by unchecking 'RequireCurrentFrameData'"), OwnerStackText, DIClassText, VariableText),
						StackObject
					);
				}
			}
		}
	);
}

void UNiagaraValidationRule_UserDataInterfaces::CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const
{
	UNiagaraSystem* NiagaraSystem = &Context.ViewModel->GetSystem();
	const FNiagaraUserRedirectionParameterStore& ExposedParameters = NiagaraSystem->GetExposedParameters();
	if (ExposedParameters.GetDataInterfaces().Num() == 0)
	{
		return;
	}

	UObject* StackObject = NiagaraValidation::GetStackEntry<UNiagaraStackSystemPropertiesItem>(Context.ViewModel->GetSystemStackViewModel());
	for (const FNiagaraVariableWithOffset& Variable : ExposedParameters.ReadParameterVariables())
	{
		if (Variable.IsDataInterface() == false)
		{
			continue;
		}

		UClass* DIClass = Variable.GetType().GetClass();
		if (BannedDataInterfaces.Num() > 0 && !BannedDataInterfaces.Contains(DIClass))
		{
			continue;
		}


		if (AllowDataInterfaces.Num() > 0 && AllowDataInterfaces.Contains(DIClass))
		{
			continue;
		}

		if (bOnlyIncludeExposedUObjects && !NiagaraValidation::StructContainsUObjectProperty(DIClass))
		{
			continue;
		}

		const FText DIClassText = FText::FromName(DIClass->GetFName());
		const FText VariableText = FText::FromName(Variable.GetName());
		OutResults.Emplace(
			Severity,
			FText::Format(LOCTEXT("UserDataInterfaceFormat", "User DataInterface '{0}' should be removed."), VariableText),
			FText::Format(LOCTEXT("UserDataInterfaceDetailedFormat", "DataInterface '{0}' type '{1}' may cause issues when exposed to UEFN and reduce performance when creating an instance.  Consider moving to system level and use object parameter binding on the data interface instead."), VariableText, DIClassText),
			StackObject
		);
	}
}

#undef LOCTEXT_NAMESPACE
