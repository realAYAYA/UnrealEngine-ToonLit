// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshEditorUtils.h"

#include "ContextObjectStore.h"
#include "InteractiveToolsContext.h"
#include "IPersonaToolkit.h"
#include "ISkeletalMeshEditor.h"
#include "ISkeletonEditorModule.h"
#include "SkeletonModifier.h"
#include "SReferenceSkeletonTree.h"
#include "Widgets/Docking/SDockTab.h"
#include "Preferences/PersonaOptions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletalMeshEditorUtils)

bool UE::SkeletalMeshEditorUtils::RegisterEditorContextObject(UInteractiveToolsContext* ToolsContext)
{
	if (ensure(ToolsContext))
	{
		const USkeletalMeshEditorContextObject* Found = ToolsContext->ContextObjectStore->FindContext<USkeletalMeshEditorContextObject>();
		if (Found)
		{
			return true;
		}
		
		USkeletalMeshEditorContextObject* ContextObject = NewObject<USkeletalMeshEditorContextObject>(ToolsContext->ToolManager);
		if (ensure(ContextObject))
		{
			ContextObject->Register(ToolsContext->ToolManager);
			return true;
		}
	}
	return false;
}

bool UE::SkeletalMeshEditorUtils::UnregisterEditorContextObject(UInteractiveToolsContext* ToolsContext)
{
	if (ensure(ToolsContext))
	{
		USkeletalMeshEditorContextObject* Found = ToolsContext->ContextObjectStore->FindContext<USkeletalMeshEditorContextObject>();
		if (Found != nullptr)
		{
			Found->Unregister(ToolsContext->ToolManager);
			ToolsContext->ContextObjectStore->RemoveContextObject(Found);
		}
		return true;
	}
	return false;
}

USkeletalMeshEditorContextObject* UE::SkeletalMeshEditorUtils::GetEditorContextObject(UInteractiveToolsContext* ToolsContext)
{
	return ToolsContext->ContextObjectStore->FindContext<USkeletalMeshEditorContextObject>();
}

void USkeletalMeshEditorContextObject::Register(UInteractiveToolManager* InToolManager)
{
	if (ensure(!bRegistered) == false)
	{
		return;
	}

	InToolManager->GetContextObjectStore()->AddContextObject(this);
	bRegistered = true;
}

void USkeletalMeshEditorContextObject::Unregister(UInteractiveToolManager* InToolManager)
{
	ensure(bRegistered);
	
	InToolManager->GetContextObjectStore()->RemoveContextObject(this);

	EditorBindings.Reset();
	TreeBindings.Reset();
	
	bRegistered = false;
}

void USkeletalMeshEditorContextObject::Init(const TWeakPtr<ISkeletalMeshEditor>& InEditor)
{
	Editor = InEditor;
	EditorBindings.Reset();
	TreeBindings.Reset();
}

void USkeletalMeshEditorContextObject::HideSkeleton()
{
	if (!Editor.IsValid())
	{
		return;
	}

	const TSharedPtr<ISkeletalMeshEditor> SkeletalMeshEditor = Editor.Pin();
	UDebugSkelMeshComponent* SkeletalMeshComponent = SkeletalMeshEditor->GetPersonaToolkit()->GetPreviewMeshComponent();
	if (!SkeletalMeshComponent)
	{
		return;
	}
	
	SkeletonDrawMode = SkeletalMeshComponent->SkeletonDrawMode;
	SkeletalMeshComponent->SkeletonDrawMode = ESkeletonDrawMode::Hidden;
}

void USkeletalMeshEditorContextObject::ShowSkeleton()
{
	if (!Editor.IsValid())
	{
		return;
	}
	
	const TSharedPtr<ISkeletalMeshEditor> SkeletalMeshEditor = Editor.Pin();
	UDebugSkelMeshComponent* SkeletalMeshComponent = SkeletalMeshEditor->GetPersonaToolkit()->GetPreviewMeshComponent();
	if (!SkeletalMeshComponent)
	{
		return;
	}
	
	SkeletalMeshComponent->SkeletonDrawMode = SkeletonDrawMode;
	SkeletonDrawMode = ESkeletonDrawMode::Default;
}

void USkeletalMeshEditorContextObject::BindTo(ISkeletalMeshEditingInterface* InEditingInterface)
{
	if (!InEditingInterface)
	{
		return;
	}
	
	BindEditor(InEditingInterface);
	BindRefSkeletonTree(InEditingInterface);
}

void USkeletalMeshEditorContextObject::UnbindFrom(ISkeletalMeshEditingInterface* InEditingInterface)
{
	if (!InEditingInterface)
	{
		return;
	}

	UnbindEditor(InEditingInterface);
	UnbindRefSkeletonTree(InEditingInterface);
}

TPair<FDelegateHandle, FDelegateHandle> USkeletalMeshEditorContextObject::BindInterfaceTo(
	ISkeletalMeshEditingInterface* InInterface,
	ISkeletalMeshNotifier& InOtherNotifier)
{
	// connect external interface to tool (ie skeletal mesh editor -> tool)
	FDelegateHandle ToToolNotifierHandle = InOtherNotifier.Delegate().AddLambda(
	[InInterface](const TArray<FName>& BoneNames, const ESkeletalMeshNotifyType InNotifyType)
	{
		InInterface->GetNotifier().HandleNotification(BoneNames, InNotifyType);
	});

	// connect tool to external interface (ie tool -> skeletal mesh editor)
	FDelegateHandle FromToolNotifierHandle = InInterface->GetNotifier().Delegate().AddLambda(
		[&InOtherNotifier](const TArray<FName>& BoneNames, const ESkeletalMeshNotifyType InNotifyType)
	{
		InOtherNotifier.HandleNotification(BoneNames, InNotifyType);
	});

	return {ToToolNotifierHandle, FromToolNotifierHandle};
}

void USkeletalMeshEditorContextObject::UnbindInterfaceFrom(
	ISkeletalMeshEditingInterface* InInterface,
	ISkeletalMeshNotifier& InOtherNotifier,
	const FBindData& InOutBindData)
{
	if (InOutBindData.ToToolNotifierHandle.IsValid())
	{
		InOtherNotifier.Delegate().Remove(InOutBindData.ToToolNotifierHandle);
	}

	if (InOutBindData.FromToolNotifierHandle.IsValid())
	{
		InInterface->GetNotifier().Delegate().Remove(InOutBindData.FromToolNotifierHandle);
	}
}

void USkeletalMeshEditorContextObject::BindEditor(ISkeletalMeshEditingInterface* InEditingInterface)
{
	if (!InEditingInterface || !Editor.IsValid())
	{
		return;
	}
	
	if (EditorBindings.Contains(InEditingInterface))
	{
		return;
	}

	TSharedPtr<ISkeletalMeshEditorBinding> Binding = Editor.Pin()->GetBinding();
	if (!Binding.IsValid())
	{
		return;
	}

	InEditingInterface->BindTo(Binding);

	FBindData BindData;
	Tie(BindData.ToToolNotifierHandle, BindData.FromToolNotifierHandle) = BindInterfaceTo(InEditingInterface, Binding->GetNotifier());
	EditorBindings.Emplace(InEditingInterface, BindData);

	InEditingInterface->GetNotifier().HandleNotification(Binding->GetSelectedBones(), ESkeletalMeshNotifyType::BonesSelected);
}

void USkeletalMeshEditorContextObject::UnbindEditor(ISkeletalMeshEditingInterface* InEditingInterface)
{
	if (const FBindData* BindData = EditorBindings.Find(InEditingInterface))
	{
		if (BindData->FromToolNotifierHandle.IsValid())
		{
			InEditingInterface->Unbind();
		}

		if (Editor.IsValid())
		{
			const TSharedPtr<ISkeletalMeshEditorBinding> Binding = Editor.Pin()->GetBinding();
			UnbindInterfaceFrom(InEditingInterface, Binding->GetNotifier(), *BindData);
		}
	
		EditorBindings.Remove(InEditingInterface);
	}
}

void USkeletalMeshEditorContextObject::BindRefSkeletonTree(ISkeletalMeshEditingInterface* InEditingInterface)
{
	if (!InEditingInterface || !Editor.IsValid())
	{
		return;
	}
	
	const TWeakObjectPtr<USkeletonModifier> Modifier = InEditingInterface->GetModifier();
	if (!Modifier.IsValid())
	{
		return;
	}
	
	const TSharedPtr<FTabManager> TabManager = Editor.Pin()->GetAssociatedTabManager();
	TSharedPtr<SDockTab> SkeletonTab = TabManager->FindExistingLiveTab(GetSkeletonTreeTabId());
	if (SkeletonTab.IsValid())
	{
		DefaultSkeletonWidget = SkeletonTab->GetContent();
	}
	else
	{
		SkeletonTab = TabManager->TryInvokeTab(GetSkeletonTreeTabId());
	}

	check(SkeletonTab.IsValid());

	SAssignNew(RefSkeletonWidget, SBorder)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
	[
		SAssignNew(RefSkeletonTree, SReferenceSkeletonTree)
			.Modifier(Modifier)
	];

	SkeletonTab->SetContent(RefSkeletonWidget.ToSharedRef());

	FBindData BindData;
	Tie(BindData.ToToolNotifierHandle, BindData.FromToolNotifierHandle) = BindInterfaceTo(InEditingInterface, RefSkeletonTree->GetNotifier());
	TreeBindings.Emplace(InEditingInterface, BindData);

	if(const TSharedPtr<ISkeletalMeshEditorBinding> Binding = Editor.Pin()->GetBinding())
	{
		RefSkeletonTree->GetNotifier().HandleNotification(Binding->GetSelectedBones(), ESkeletalMeshNotifyType::BonesSelected);
	}
}

void USkeletalMeshEditorContextObject::UnbindRefSkeletonTree(ISkeletalMeshEditingInterface* InEditingInterface)
{
	if (const FBindData* BindData = TreeBindings.Find(InEditingInterface))
	{
		if (RefSkeletonTree.IsValid())
		{
			UnbindInterfaceFrom(InEditingInterface, RefSkeletonTree->GetNotifier(), *BindData);
		}

		if (Editor.IsValid())
		{
			const TSharedPtr<FTabManager> TabManager = Editor.Pin()->GetAssociatedTabManager();
			const TSharedPtr<SDockTab> SkeletonTab = TabManager->FindExistingLiveTab(GetSkeletonTreeTabId());
			if (SkeletonTab.IsValid())
			{
				if (SkeletonTab->GetContent() == RefSkeletonWidget)
				{
					SkeletonTab->SetContent(DefaultSkeletonWidget.IsValid() ? DefaultSkeletonWidget.ToSharedRef() : SNullWidget::NullWidget);
				}
			}
		}
		
		RefSkeletonTree.Reset();
		RefSkeletonWidget.Reset();
		
		TreeBindings.Remove(InEditingInterface);
	}
}

const FName& USkeletalMeshEditorContextObject::GetSkeletonTreeTabId()
{
	static const FName SkeletonTreeId(TEXT("SkeletonTreeView"));
	return SkeletonTreeId;
}

TSharedPtr<ISkeletalMeshEditorBinding> USkeletalMeshEditorContextObject::GetBinding() const
{
	return Editor.IsValid() ? Editor.Pin()->GetBinding() : nullptr;
}
