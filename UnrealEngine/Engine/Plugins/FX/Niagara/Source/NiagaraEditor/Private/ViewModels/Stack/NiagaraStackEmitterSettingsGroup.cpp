// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackEmitterSettingsGroup.h"
#include "ViewModels/Stack/NiagaraStackObject.h"
#include "ViewModels/Stack/NiagaraStackSummaryViewInputCollection.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterEditorData.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraScriptMergeManager.h"
#include "NiagaraEmitterDetailsCustomization.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraSystem.h"
#include "ScopedTransaction.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraSystemSelectionViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "Styling/AppStyle.h"
#include "IDetailTreeNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraStackEmitterSettingsGroup)

#define LOCTEXT_NAMESPACE "UNiagaraStackEmitterItemGroup"

void UNiagaraStackEmitterPropertiesItem::Initialize(FRequiredEntryData InRequiredEntryData)
{
	Super::Initialize(InRequiredEntryData, TEXT("EmitterProperties"));
	EmitterWeakPtr = GetEmitterViewModel()->GetEmitter().ToWeakPtr();
	EmitterWeakPtr.Emitter->OnPropertiesChanged().AddUObject(this, &UNiagaraStackEmitterPropertiesItem::EmitterPropertiesChanged);
}

void UNiagaraStackEmitterPropertiesItem::FinalizeInternal()
{
	if (EmitterWeakPtr.IsValid())
	{
		EmitterWeakPtr.Emitter->OnPropertiesChanged().RemoveAll(this);
	}
	Super::FinalizeInternal();
}

FText UNiagaraStackEmitterPropertiesItem::GetDisplayName() const
{
	return LOCTEXT("EmitterPropertiesName", "Emitter Properties");
}

FText UNiagaraStackEmitterPropertiesItem::GetTooltipText() const
{
	return LOCTEXT("EmitterPropertiesTooltip", "Properties that are handled per Emitter. These cannot change at runtime.");
}

bool UNiagaraStackEmitterPropertiesItem::TestCanResetToBaseWithMessage(FText& OutCanResetToBaseMessage) const
{
	if (GetEmitterViewModel().IsValid() == false)
	{
		bCanResetToBaseCache = false;
	}
	else if (bCanResetToBaseCache.IsSet() == false)
	{
		FVersionedNiagaraEmitter BaseEmitter = GetEmitterViewModel()->GetParentEmitter();
		if (BaseEmitter.GetEmitterData() != nullptr && EmitterWeakPtr.Emitter != BaseEmitter.Emitter)
		{
			TSharedRef<FNiagaraScriptMergeManager> MergeManager = FNiagaraScriptMergeManager::Get();
			bCanResetToBaseCache = MergeManager->IsEmitterEditablePropertySetDifferentFromBase(EmitterWeakPtr.ResolveWeakPtr(), BaseEmitter);
		}
		else
		{
			bCanResetToBaseCache = false;
		}
	}
	if (bCanResetToBaseCache.GetValue())
	{
		OutCanResetToBaseMessage = LOCTEXT("CanResetToBase", "Reset the emitter properties to the state defined by the parent emitter.");
		return true;
	}
	else
	{
		OutCanResetToBaseMessage = LOCTEXT("CanNotResetToBase", "No parent to reset to, or not different from parent.");
		return false;
	}
}

void UNiagaraStackEmitterPropertiesItem::ResetToBase()
{
	FText Unused;
	if (TestCanResetToBaseWithMessage(Unused))
	{
		FVersionedNiagaraEmitter BaseEmitter = GetEmitterViewModel()->GetParentEmitter();
		TSharedRef<FNiagaraScriptMergeManager> MergeManager = FNiagaraScriptMergeManager::Get();
		MergeManager->ResetEmitterEditablePropertySetToBase(EmitterWeakPtr.ResolveWeakPtr(), BaseEmitter);
	}
}

const FSlateBrush* UNiagaraStackEmitterPropertiesItem::GetIconBrush() const
{
	if (FVersionedNiagaraEmitterData* EmitterData = EmitterWeakPtr.GetEmitterData())
	{
		if (EmitterData->SimTarget == ENiagaraSimTarget::CPUSim)
		{
			return FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stack.CPUIcon");
		}
		if (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
		{
			return FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stack.GPUIcon");
		}
	}
	return FAppStyle::GetBrush("NoBrush");
}

void UNiagaraStackEmitterPropertiesItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	if (EmitterObject == nullptr)
	{
		EmitterObject = NewObject<UNiagaraStackObject>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(), FExecutionCategoryNames::Emitter, NAME_None, GetStackEditorData());
		bool bIsTopLevelObject = true;
		EmitterObject->Initialize(RequiredEntryData, EmitterWeakPtr.Emitter.Get(), bIsTopLevelObject, GetStackEditorDataKey());
		EmitterObject->RegisterInstancedCustomPropertyLayout(UNiagaraEmitter::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraEmitterDetails::MakeInstance));
	}

	NewChildren.Add(EmitterObject);
	bCanResetToBaseCache.Reset();
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
	RefreshIssues(NewIssues);
}

UNiagaraStackEntry::FStackIssueFixDelegate UNiagaraStackEmitterPropertiesItem::GetUpgradeVersionFix()
{
	return FStackIssueFixDelegate::CreateLambda([=]()
	{
		FGuid NewVersion = GetEmitterViewModel()->GetParentEmitter().Emitter->GetExposedVersion().VersionGuid;
		FNiagaraEditorUtilities::SwitchParentEmitterVersion(GetEmitterViewModel().ToSharedRef(), GetSystemViewModel(), NewVersion);
	});
}

void UNiagaraStackEmitterPropertiesItem::RefreshIssues(TArray<FStackIssue>& NewIssues)
{
	FNiagaraStackGraphUtilities::CheckForDeprecatedEmitterVersion(GetEmitterViewModel(), GetStackEditorDataKey(), GetUpgradeVersionFix(), NewIssues);
	
	FVersionedNiagaraEmitterData* ActualEmitterData = GetEmitterViewModel()->GetEmitter().GetEmitterData();
	if (ActualEmitterData)
	{
		if (ActualEmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim && ActualEmitterData->CalculateBoundsMode == ENiagaraEmitterCalculateBoundMode::Dynamic)
		{
			bool bAddError = true;

			UNiagaraSystem& Sys = GetSystemViewModel()->GetSystem();
			if (Sys.bFixedBounds)
			{
				bAddError = false;
			}

			if (bAddError)
			{
				FStackIssue MissingRequiredFixedBoundsModuleError(
					EStackIssueSeverity::Warning,
					LOCTEXT("RequiredFixedBoundsWarningFormat", "The emitter is GPU and is using dynamic bounds mode.\r\nPlease update the Emitter or System properties otherwise bounds may be incorrect."),
					LOCTEXT("MissingFixedBounds", "Missing fixed bounds."),
					GetStackEditorDataKey(),
					false);

				NewIssues.Add(MissingRequiredFixedBoundsModuleError);
			}
		}

		if ( ActualEmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim && ActualEmitterData->bGpuAlwaysRunParticleUpdateScript )
		{
			const FText FixGpuInterpolatedDisabledText = LOCTEXT("FixGpuInterpolatedDisabled", "Fix GPU emitter to not run particle update script on particle spawn");
			FVersionedNiagaraEmitterWeakPtr WeakEmitter = GetEmitterViewModel()->GetEmitter().ToWeakPtr();

			NewIssues.Emplace(
				EStackIssueSeverity::Warning,
				LOCTEXT("GpuNoInterpolatedIncorrect", "GPU is incorrectly running particle update script on particle spawn."),
				LOCTEXT("GpuNoInterpolatedIncorrectLong", "Due to a previous GPU codegen issue both particle spawn & update scripts were running on particle spawn when interpolated spawning is disabled.  This is now fixed for new content to match CPU behavior where only particle spawn will run when interpolated spawning is disabled."),
				GetStackEditorDataKey(),
				true,
				FStackIssueFix(
					FixGpuInterpolatedDisabledText,
					FStackIssueFixDelegate::CreateLambda(
						[WeakEmitter, FixGpuInterpolatedDisabledText]()
						{
							FVersionedNiagaraEmitter PinnedEmitter = WeakEmitter.ResolveWeakPtr();
							if (PinnedEmitter.Emitter)
							{
								FScopedTransaction ScopedTransaction(FixGpuInterpolatedDisabledText);

								PinnedEmitter.Emitter->Modify();
								PinnedEmitter.GetEmitterData()->bGpuAlwaysRunParticleUpdateScript = false;
								UNiagaraSystem::RequestCompileForEmitter(PinnedEmitter);
							}
						}
					)
				)
			);
		}

		UNiagaraSystem& System = GetSystemViewModel()->GetSystem();
		TWeakPtr<FNiagaraSystemViewModel> WeakSysViewModel = GetSystemViewModel();
		if (System.NeedsWarmup())
		{
			float WarmupDelta = System.GetWarmupTickDelta();
			if (ActualEmitterData->bLimitDeltaTime && ActualEmitterData->MaxDeltaTimePerTick < WarmupDelta)
			{
				TArray<FStackIssueFix> Fixes;

				float MaxEmitterDt = ActualEmitterData->MaxDeltaTimePerTick;
				//This emitter does not allow ticks with a delta time so large.
				FText FixDescriptionReduceWarmupDt = LOCTEXT("FixWarmupDeltaTime", "Reduce System Warmup Delta Time");
				Fixes.Emplace(
					FixDescriptionReduceWarmupDt,
					FStackIssueFixDelegate::CreateLambda([=]()
						{
							if (auto Pinned = WeakSysViewModel.Pin())
							{
								FScopedTransaction ScopedTransaction(FixDescriptionReduceWarmupDt);
								Pinned->GetSystem().Modify();
								Pinned->GetSystem().SetWarmupTickDelta(MaxEmitterDt);
								Pinned->RefreshAll();
							}
						}));

				FVersionedNiagaraEmitterWeakPtr WeakEmitter = GetEmitterViewModel()->GetEmitter().ToWeakPtr();
				FText FixDescriptionReduceIncreaseEmitterDt = LOCTEXT("FixEmitterDeltaTime", "Increase Max Emitter Delta Time");
				Fixes.Emplace(
					FixDescriptionReduceIncreaseEmitterDt,
					FStackIssueFixDelegate::CreateLambda([=]()
						{
							auto PinnedSysViewModel = WeakSysViewModel.Pin();
							FVersionedNiagaraEmitter PinnedEmitter = WeakEmitter.ResolveWeakPtr();
							if (PinnedEmitter.Emitter && PinnedSysViewModel)
							{
								FScopedTransaction ScopedTransaction(FixDescriptionReduceIncreaseEmitterDt);

								PinnedEmitter.Emitter->Modify();
								PinnedEmitter.GetEmitterData()->MaxDeltaTimePerTick = WarmupDelta;
								PinnedSysViewModel->RefreshAll();
							}
						}));

				FStackIssue WarmupDeltaTimeExceedsEmitterDeltaTimeWarning(
					EStackIssueSeverity::Warning,
					LOCTEXT("WarmupDeltaTimeExceedsEmitterDeltaTimeWarningSummary", "System Warmup Delta Time Exceeds Emitter Max Delta Time."),
					LOCTEXT("WarmupDeltaTimeExceedsEmitterDeltaTimeWarningText", "Max Tick Delta Time is smaller than the System's Warmup Delta Time. This could cause unintended results during warmup for this emitter."),
					GetStackEditorDataKey(),
					false,
					Fixes);

				NewIssues.Add(WarmupDeltaTimeExceedsEmitterDeltaTimeWarning);
			}
		}

		// check for any emitters which are exclusively using Emitter sourced renderers and dynamic bounds.  Currently our bounds calculators don't support
		// emitter sources and so no valid bounds will be generated
		if (ActualEmitterData->SimTarget == ENiagaraSimTarget::CPUSim && ActualEmitterData->CalculateBoundsMode == ENiagaraEmitterCalculateBoundMode::Dynamic)
		{
			UNiagaraSystem& Sys = GetSystemViewModel()->GetSystem();
			if (!Sys.bFixedBounds)
			{
				bool bHasParticleSourcedRenderer = false;
				ActualEmitterData->ForEachEnabledRenderer([&](UNiagaraRendererProperties* RendererProperties)
				{
					if (RendererProperties && RendererProperties->GetCurrentSourceMode() == ENiagaraRendererSourceDataMode::Particles)
					{
						bHasParticleSourcedRenderer = true;
					}
				});

				if (!bHasParticleSourcedRenderer)
				{
					FStackIssue EmitterSourcedEmitterRequiredFixedBoundsError(
						EStackIssueSeverity::Warning,
						LOCTEXT("EmitterSourcedWarningFormat", "The emitter is using dynamic bounds mode but only using Emitter sourced renderers.\r\nPlease update the Emitter or System properties otherwise bounds may be incorrect."),
						LOCTEXT("EmitterSourcedWarning", "Missing fixed bounds."),
						GetStackEditorDataKey(),
						false);

					NewIssues.Add(EmitterSourcedEmitterRequiredFixedBoundsError);
				}
			}
		}
	}

}

void UNiagaraStackEmitterPropertiesItem::EmitterPropertiesChanged()
{
	if (IsFinalized() == false)
	{
		// Undo/redo can cause objects to disappear and reappear which can prevent safe removal of delegates
		// so guard against receiving an event when finalized here.
		bCanResetToBaseCache.Reset();
		RefreshChildren();

		if (EmitterWeakPtr.IsValid())
		{
			GetSystemViewModel()->GetEmitterHandleViewModelForEmitter(EmitterWeakPtr.ResolveWeakPtr()).Get()->GetEmitterStackViewModel()->RequestValidationUpdate();
		}
	}
}

void UNiagaraStackEmitterSummaryItem::Initialize(FRequiredEntryData InRequiredEntryData)
{
	Super::Initialize(InRequiredEntryData, TEXT("EmitterParameters"));
	Emitter = GetEmitterViewModel()->GetEmitter().ToWeakPtr();
}

FText UNiagaraStackEmitterSummaryItem::GetDisplayName() const
{

	return LOCTEXT("EmitterSummaryName", "Emitter Summary");
}

FText UNiagaraStackEmitterSummaryItem::GetTooltipText() const
{

	return LOCTEXT("EmitterSummaryTooltip", "Subset of parameters from the stack, summarized here for easier access.");
}

FText UNiagaraStackEmitterSummaryItem::GetIconText() const
{
	return FText::FromString(FString(TEXT("\xf0ca")/* fa-list-ul */));
}

void UNiagaraStackEmitterSummaryItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	if (GetEmitterViewModel()->GetSummaryIsInEditMode())
	{
		if (SummaryEditorData == nullptr)
		{
			SummaryEditorData = NewObject<UNiagaraStackObject>(this);
			bool bIsTopLevelObject = true;
			SummaryEditorData->Initialize(CreateDefaultChildRequiredData(), &GetEmitterViewModel()->GetOrCreateEditorData(), bIsTopLevelObject, GetStackEditorDataKey());
			SummaryEditorData->SetOnSelectRootNodes(UNiagaraStackObject::FOnSelectRootNodes::CreateUObject(this,
				&UNiagaraStackEmitterSummaryItem::SelectSummaryNodesFromEmitterEditorDataRootNodes));
		}
		NewChildren.Add(SummaryEditorData);
	}
	else
	{
		SummaryEditorData = nullptr;
	}

	if (FilteredObject == nullptr)
	{
		FilteredObject = NewObject<UNiagaraStackSummaryViewObject>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(), FExecutionCategoryNames::Emitter, NAME_None, GetStackEditorData());
		FilteredObject->Initialize(RequiredEntryData, Emitter, GetStackEditorDataKey());
	}

	NewChildren.Add(FilteredObject);
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

TSharedPtr<IDetailTreeNode> GetSummarySectionsPropertyNode(const TArray<TSharedRef<IDetailTreeNode>>& Nodes)
{
	TArray<TSharedRef<IDetailTreeNode>> ChildrenToCheck;
	for (TSharedRef<IDetailTreeNode> Node : Nodes)
	{
		if (Node->GetNodeType() == EDetailNodeType::Item)
		{
			TSharedPtr<IPropertyHandle> NodePropertyHandle = Node->CreatePropertyHandle();
			if (NodePropertyHandle.IsValid() && NodePropertyHandle->GetProperty()->GetFName() == UNiagaraEmitterEditorData::PrivateMemberNames::SummarySections)
			{
				return Node;
			}
		}

		TArray<TSharedRef<IDetailTreeNode>> Children;
		Node->GetChildren(Children);
		ChildrenToCheck.Append(Children);
	}
	if (ChildrenToCheck.Num() == 0)
	{
		return nullptr;
	}
	return GetSummarySectionsPropertyNode(ChildrenToCheck);
}

void UNiagaraStackEmitterSummaryItem::SelectSummaryNodesFromEmitterEditorDataRootNodes(TArray<TSharedRef<IDetailTreeNode>> Source, TArray<TSharedRef<IDetailTreeNode>>* Selected)
{
	TSharedPtr<IDetailTreeNode> SummarySectionsNode = GetSummarySectionsPropertyNode(Source);
	if (SummarySectionsNode.IsValid())
	{
		Selected->Add(SummarySectionsNode.ToSharedRef());
	}
}

bool UNiagaraStackEmitterSummaryItem::GetEditModeIsActive() const
{ 
	return GetEmitterViewModel().IsValid() && GetEmitterViewModel()->GetSummaryIsInEditMode();
}

void UNiagaraStackEmitterSummaryItem::SetEditModeIsActive(bool bInEditModeIsActive)
{
	if (GetEmitterViewModel().IsValid())
	{
		if (GetEmitterViewModel()->GetSummaryIsInEditMode() != bInEditModeIsActive)
		{
			GetEmitterViewModel()->SetSummaryIsInEditMode(bInEditModeIsActive);
			RefreshChildren();
		}
	}
}

UNiagaraStackEmitterSummaryGroup::UNiagaraStackEmitterSummaryGroup()
	: SummaryItem(nullptr)
{
}

void UNiagaraStackEmitterSummaryGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	if (SummaryItem == nullptr)
	{
		SummaryItem = NewObject<UNiagaraStackEmitterSummaryItem>(this);
		SummaryItem->Initialize(CreateDefaultChildRequiredData());
	}
	NewChildren.Add(SummaryItem);

	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

FText UNiagaraStackEmitterSummaryGroup::GetIconText() const
{
	return FText::FromString(FString(TEXT("\xf0ca")/* fa-list-ul */));
}




void UNiagaraStackSummaryViewCollapseButton::Initialize(FRequiredEntryData InRequiredEntryData)
{
	Super::Initialize(InRequiredEntryData, TEXT("ShowAdvanced"));
}

FText UNiagaraStackSummaryViewCollapseButton::GetDisplayName() const
{
	return LOCTEXT("SummaryCollapseButtonDisplayName", "Show Advanced");
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackSummaryViewCollapseButton::GetStackRowStyle() const
{
	return EStackRowStyle::GroupHeader;
}

FText UNiagaraStackSummaryViewCollapseButton::GetTooltipText() const 
{
	return LOCTEXT("SummaryCollapseButtonTooltip", "Expand/Collapse detailed view.");
}

bool UNiagaraStackSummaryViewCollapseButton::GetIsEnabled() const
{
	return true;
}

void UNiagaraStackSummaryViewCollapseButton::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	
}








#undef LOCTEXT_NAMESPACE

