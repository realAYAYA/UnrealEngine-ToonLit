// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackRenderItemGroup.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterEditorData.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraClipboard.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorSettings.h"

#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Styling/CoreStyle.h"
#include "ViewModels/NiagaraSystemSelectionViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraStackRenderItemGroup)

#define LOCTEXT_NAMESPACE "UNiagaraStackRenderItemGroup"

class FRenderItemGroupAddAction : public INiagaraStackItemGroupAddAction
{
public:
	FRenderItemGroupAddAction(FNiagaraRendererCreationInfo InRendererCreationInfo)
		: RendererCreationInfo(InRendererCreationInfo)
	{
	}

	virtual TArray<FString> GetCategories() const override
	{
		return {};
	}

	virtual FText GetDisplayName() const override
	{
		return RendererCreationInfo.DisplayName;
	}

	virtual FText GetDescription() const override
	{
		return RendererCreationInfo.Description;
	}

	virtual FText GetKeywords() const override
	{
		return FText();
	}

	const FNiagaraRendererCreationInfo& GetRendererCreationInfo() { return RendererCreationInfo; }

private:
	FNiagaraRendererCreationInfo RendererCreationInfo;
};

class FRenderItemGroupAddUtilities : public TNiagaraStackItemGroupAddUtilities<UNiagaraRendererProperties*>
{
public:
	FRenderItemGroupAddUtilities(TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, FRenderItemGroupAddUtilities::FOnItemAdded OnItemAdded = FRenderItemGroupAddUtilities::FOnItemAdded())
		: TNiagaraStackItemGroupAddUtilities(LOCTEXT("RenderGroupAddItemName", "Renderer"), EAddMode::AddFromAction, true, false, OnItemAdded)
		, EmitterViewModel(InEmitterViewModel)
	{
	}

	virtual void AddItemDirectly() override { unimplemented(); }

	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions, const FNiagaraStackItemGroupAddOptions& AddProperties) const override
	{
		const TArray<FNiagaraRendererCreationInfo>& RendererCreationInfos = FNiagaraEditorModule::Get().GetRendererCreationInfos();

		const UNiagaraEditorSettings* NiagaraEditorSettings = GetDefault<UNiagaraEditorSettings>();
		for (const FNiagaraRendererCreationInfo& RendererCreationInfo : RendererCreationInfos)
		{
			if (NiagaraEditorSettings->IsAllowedClassPath(RendererCreationInfo.RendererClassPath))
			{
				OutAddActions.Add(MakeShared<FRenderItemGroupAddAction>(RendererCreationInfo));
			}
		}
	}

	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) override
	{
		TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModelPinned = EmitterViewModel.Pin();
		if (EmitterViewModelPinned.IsValid() == false)
		{
			return;
		}

		TSharedRef<FRenderItemGroupAddAction> RenderAddAction = StaticCastSharedRef<FRenderItemGroupAddAction>(AddAction);

		FScopedTransaction ScopedTransaction(LOCTEXT("AddNewRendererTransaction", "Add new renderer"));

		FVersionedNiagaraEmitter VersionedEmitter = EmitterViewModelPinned->GetEmitter();
		UNiagaraRendererProperties* RendererProperties = RenderAddAction->GetRendererCreationInfo().RendererFactory.Execute(VersionedEmitter.Emitter);
		check(RendererProperties != nullptr);
		
		VersionedEmitter.Emitter->AddRenderer(RendererProperties, VersionedEmitter.Version);

		bool bVarsAdded = false;
		TArray<FNiagaraVariable> MissingAttributes = UNiagaraStackRendererItem::GetMissingVariables(RendererProperties, VersionedEmitter.GetEmitterData());
		for (int32 i = 0; i < MissingAttributes.Num(); i++)
		{
			if (UNiagaraStackRendererItem::AddMissingVariable(VersionedEmitter.GetEmitterData(), MissingAttributes[i]))
			{
				bVarsAdded = true;
			}
		}

		FNiagaraSystemUpdateContext SystemUpdate(VersionedEmitter, true);

		if (bVarsAdded)
		{
			FNotificationInfo Info(LOCTEXT("AddedVariables", "One or more variables have been added to the Spawn script to support the added renderer."));
			Info.ExpireDuration = 5.0f;
			Info.bFireAndForget = true;
			Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Info"));
			FSlateNotificationManager::Get().AddNotification(Info);
		}

		OnItemAdded.ExecuteIfBound(RendererProperties);
	}

private:
	TWeakPtr<FNiagaraEmitterViewModel> EmitterViewModel;
};

void UNiagaraStackRenderItemGroup::Initialize(FRequiredEntryData InRequiredEntryData)
{
	FText DisplayName = LOCTEXT("RenderGroupName", "Render");
	FText ToolTip = LOCTEXT("RendererGroupTooltip", "Describes how we should display/present each particle. Note that this doesn't have to be visual. Multiple renderers are supported. Order in this stack is not necessarily relevant to draw order.");
	AddUtilities = MakeShared<FRenderItemGroupAddUtilities>(InRequiredEntryData.EmitterViewModel.ToSharedRef(), FRenderItemGroupAddUtilities::FOnItemAdded::CreateUObject(this, &UNiagaraStackRenderItemGroup::OnRendererAdded));
	Super::Initialize(InRequiredEntryData, DisplayName, ToolTip, AddUtilities.Get());
	EmitterWeak = GetEmitterViewModel()->GetEmitter().ToWeakPtr();
	EmitterWeak.Emitter->OnRenderersChanged().AddUObject(this, &UNiagaraStackRenderItemGroup::EmitterRenderersChanged);
}

bool UNiagaraStackRenderItemGroup::TestCanPasteWithMessage(const UNiagaraClipboardContent* ClipboardContent, FText& OutMessage) const
{
	if (ClipboardContent->Renderers.Num() > 0)
	{
		OutMessage = LOCTEXT("PasteRenderers", "Paste renderers from the clipboard.");
		return true;
	}
	OutMessage = FText();
	return false;
}

FText UNiagaraStackRenderItemGroup::GetPasteTransactionText(const UNiagaraClipboardContent* ClipboardContent) const
{
	return LOCTEXT("PasteRenderersTransactionText", "Paste renderers");
}

void UNiagaraStackRenderItemGroup::Paste(const UNiagaraClipboardContent* ClipboardContent, FText& OutPasteWarning)
{
	if (EmitterWeak.IsValid())
	{
		UNiagaraEmitter* Emitter = EmitterWeak.Emitter.Get();
		for (const UNiagaraRendererProperties* ClipboardRenderer : ClipboardContent->Renderers)
		{
			if (ClipboardRenderer != nullptr)
			{
				EmitterWeak.Emitter->AddRenderer(ClipboardRenderer->StaticDuplicateWithNewMergeId(Emitter), EmitterWeak.Version);
			}
		}
	}
}

void UNiagaraStackRenderItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	int32 RendererIndex = 0;
	for (UNiagaraRendererProperties* RendererProperties : GetEmitterViewModel()->GetEmitter().GetEmitterData()->GetRenderers())
	{
		UNiagaraStackRendererItem* RendererItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackRendererItem>(CurrentChildren,
			[=](UNiagaraStackRendererItem* CurrentRendererItem) { return CurrentRendererItem->GetRendererProperties() == RendererProperties; });

		if (RendererItem == nullptr)
		{
			RendererItem = NewObject<UNiagaraStackRendererItem>(this);
			RendererItem->Initialize(CreateDefaultChildRequiredData(), RendererProperties);
			RendererItem->SetOnRequestCanPaste(UNiagaraStackRendererItem::FOnRequestCanPaste::CreateUObject(this, &UNiagaraStackRenderItemGroup::ChildRequestCanPaste));
			RendererItem->SetOnRequestPaste(UNiagaraStackRendererItem::FOnRequestPaste::CreateUObject(this, &UNiagaraStackRenderItemGroup::ChildRequestPaste));
		}

		NewChildren.Add(RendererItem);

		RendererIndex++;
	}

	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

void UNiagaraStackRenderItemGroup::EmitterRenderersChanged()
{
	if (IsFinalized() == false)
	{
		// With undo/redo sometimes it's not possible to unbind this delegate, so we have to check to insure safety in those cases.
		OnDataObjectModified().Broadcast(TArray<UObject*>(), ENiagaraDataObjectChange::Unknown);
		RefreshChildren();
	}
}

bool UNiagaraStackRenderItemGroup::ChildRequestCanPaste(const UNiagaraClipboardContent* ClipboardContent, FText& OutCanPasteMessage)
{
	return TestCanPasteWithMessage(ClipboardContent, OutCanPasteMessage);
}

void UNiagaraStackRenderItemGroup::ChildRequestPaste(const UNiagaraClipboardContent* ClipboardContent, int32 PasteIndex, FText& OutPasteWarning)
{
	Paste(ClipboardContent, OutPasteWarning);
}

void UNiagaraStackRenderItemGroup::OnRendererAdded(UNiagaraRendererProperties* RendererProperties) const
{
	GetSystemViewModel()->GetSelectionViewModel()->EmptySelection();
	GetSystemViewModel()->GetSelectionViewModel()->AddEntryToSelectionByDisplayedObjectKeyDeferred(FObjectKey(RendererProperties));
	GetSystemViewModel()->GetSystemStackViewModel()->RequestValidationUpdate();
}

void UNiagaraStackRenderItemGroup::FinalizeInternal()
{
	if (EmitterWeak.IsValid())
	{
		EmitterWeak.Emitter->OnRenderersChanged().RemoveAll(this);
	}
	Super::FinalizeInternal();
}

#undef LOCTEXT_NAMESPACE


