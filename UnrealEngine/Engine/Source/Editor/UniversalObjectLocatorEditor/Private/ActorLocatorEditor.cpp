// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorLocatorEditor.h"
#include "Modules/ModuleManager.h"
#include "UniversalObjectLocatorFragmentTypeHandle.h"
#include "UniversalObjectLocator.h"
#include "UniversalObjectLocatorEditor.h"
#include "IUniversalObjectLocatorEditorModule.h"
#include "IUniversalObjectLocatorCustomization.h"

#include "SceneOutlinerDragDrop.h"
#include "DragAndDrop/ActorDragDropOp.h"

#include "PropertyCustomizationHelpers.h"

#include "GameFramework/Actor.h"


#define LOCTEXT_NAMESPACE "ActorLocatorEditor"

namespace UE::UniversalObjectLocator
{

bool FActorLocatorEditor::IsDragSupported(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const
{
	TSharedPtr<FActorDragDropOp> ActorDrag;

	if (DragOperation->IsOfType<FSceneOutlinerDragDropOp>())
	{
		FSceneOutlinerDragDropOp* SceneOutlinerOp = static_cast<FSceneOutlinerDragDropOp*>(DragOperation.Get());
		ActorDrag = SceneOutlinerOp->GetSubOp<FActorDragDropOp>();
	}
	else if (DragOperation->IsOfType<FActorDragDropOp>())
	{
		ActorDrag = StaticCastSharedPtr<FActorDragDropOp>(DragOperation);
	}		

	if (ActorDrag)
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : ActorDrag->Actors)
		{
			if (WeakActor.Get())
			{
				return true;
			}
		}
	}

	return false;
}

UObject* FActorLocatorEditor::ResolveDragOperation(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const
{
	TSharedPtr<FActorDragDropOp> ActorDrag;

	if (DragOperation->IsOfType<FSceneOutlinerDragDropOp>())
	{
		FSceneOutlinerDragDropOp* SceneOutlinerOp = static_cast<FSceneOutlinerDragDropOp*>(DragOperation.Get());
		ActorDrag = SceneOutlinerOp->GetSubOp<FActorDragDropOp>();
	}
	else if (DragOperation->IsOfType<FActorDragDropOp>())
	{
		ActorDrag = StaticCastSharedPtr<FActorDragDropOp>(DragOperation);
	}

	if (ActorDrag)
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : ActorDrag->Actors)
		{
			if (AActor* Actor = WeakActor.Get())
			{
				return Actor;
			}
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FActorLocatorEditor::MakeEditUI(TSharedPtr<IUniversalObjectLocatorCustomization> Customization)
{
	TSharedRef<SObjectPropertyEntryBox> EditWidget = SNew(SObjectPropertyEntryBox)
	.ObjectPath(Customization.ToSharedRef(), &IUniversalObjectLocatorCustomization::GetPathToObject)
	// Disabling this for now since it causes issues
	//.PropertyHandle(Customization->GetProperty())
	.AllowedClass(AActor::StaticClass())
	.OnObjectChanged(this, &FActorLocatorEditor::OnSetObject, TWeakPtr<IUniversalObjectLocatorCustomization>(Customization))
	.AllowClear(true)
	.DisplayUseSelected(true)
	.DisplayBrowse(true)
	.DisplayThumbnail(true);

	float MinWidth = 100.f;
	float MaxWidth = 500.f;
	EditWidget->GetDesiredWidth(MinWidth, MaxWidth);

	return SNew(SBox)
	.MinDesiredWidth(MinWidth)
	.MaxDesiredWidth(MaxWidth)
	[
		EditWidget
	];
}

FText FActorLocatorEditor::GetDisplayText() const
{
	return LOCTEXT("ExternalActorLocatorName", "Actor");
}

FText FActorLocatorEditor::GetDisplayTooltip() const
{
	return LOCTEXT("ExternalActorLocatorTooltip", "Change this to an actor reference");
}

FSlateIcon FActorLocatorEditor::GetDisplayIcon() const
{
	return FSlateIcon();
}

void FActorLocatorEditor::OnSetObject(const FAssetData& InNewObject, TWeakPtr<IUniversalObjectLocatorCustomization> WeakCustomization)
{
	TSharedPtr<IUniversalObjectLocatorCustomization> Customization = WeakCustomization.Pin();
	if (!Customization)
	{
		return;
	}

	UObject* Object = InNewObject.FastGetAsset(true);

	FUniversalObjectLocator NewRef(Object);
	Customization->SetValue(MoveTemp(NewRef));
}

} // namespace UE::UniversalObjectLocator

#undef LOCTEXT_NAMESPACE
