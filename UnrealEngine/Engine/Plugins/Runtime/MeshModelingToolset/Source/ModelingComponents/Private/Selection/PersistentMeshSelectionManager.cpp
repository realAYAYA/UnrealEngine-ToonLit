// Copyright Epic Games, Inc. All Rights Reserved.

#include "Selection/PersistentMeshSelectionManager.h"
#include "Selection/PersistentMeshSelection.h"

#include "InteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "Drawing/PreviewGeometryActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PersistentMeshSelectionManager)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UDEPRECATED_PersistentMeshSelectionManager"


PRAGMA_DISABLE_DEPRECATION_WARNINGS

void UDEPRECATED_PersistentMeshSelectionManager::Initialize(TObjectPtr<UInteractiveToolsContext> ToolsContext)
{
	ParentContext = ToolsContext;
}

void UDEPRECATED_PersistentMeshSelectionManager::Shutdown()
{
	if (SelectionDisplay != nullptr)
	{
		SelectionDisplay->Disconnect();
		SelectionDisplay = nullptr;
	}
}

bool UDEPRECATED_PersistentMeshSelectionManager::HasActiveSelection()
{
	return (ActiveSelection != nullptr);
}

UPersistentMeshSelection* UDEPRECATED_PersistentMeshSelectionManager::GetActiveSelection()
{
	return ActiveSelection;
}

void UDEPRECATED_PersistentMeshSelectionManager::SetNewActiveSelection(UPersistentMeshSelection* Selection)
{
	TUniquePtr<FPersistentMeshSelectionChange> SelectionChange = MakeUnique<FPersistentMeshSelectionChange>();
	if (ActiveSelection != nullptr)
	{
		SelectionChange->From = ActiveSelection->GetSelection();
	}
	if (Selection != nullptr)
	{
		SelectionChange->To = Selection->GetSelection();
	}
	ParentContext->ToolManager->GetContextTransactionsAPI()->AppendChange(this, MoveTemp(SelectionChange),
		LOCTEXT("SelectionChange", "Selection Change"));

	ActiveSelection = Selection;
	OnSelectionModified();
}


void UDEPRECATED_PersistentMeshSelectionManager::SetNewActiveSelectionInternal(UPersistentMeshSelection* Selection)
{
	ActiveSelection = Selection;
	OnSelectionModified();
}


void UDEPRECATED_PersistentMeshSelectionManager::ClearActiveSelection()
{
	if (ActiveSelection == nullptr)
	{
		return;
	}

	SetNewActiveSelection(nullptr);
}




void UDEPRECATED_PersistentMeshSelectionManager::OnSelectionModified()
{
	if (ActiveSelection != nullptr && SelectionDisplay == nullptr)
	{
		SelectionDisplay = NewObject<UPreviewGeometry>(ParentContext);
		SelectionDisplay->CreateInWorld(ActiveSelection->GetTargetComponent()->GetWorld(), FTransform::Identity);
	}
	if (SelectionDisplay == nullptr)
	{
		return;
	}

	const FGenericMeshSelection* SelectionData = (ActiveSelection != nullptr) ? &ActiveSelection->GetSelection() : nullptr;

	bool bShowLines = (SelectionData != nullptr) && (SelectionData->HasRenderableLines());

	if (bShowLines)
	{
		const FColor ROIBorderColor(240, 15, 240);
		const float ROIBorderThickness = 8.0f;
		//const float ROIBorderDepthBias = 0.1f * (float)(WorldBounds.DiagonalLength() * 0.01);
		const float ROIBorderDepthBias = 0.01f;

		FTransform3d Transform(ActiveSelection->GetTargetComponent()->GetComponentToWorld());

		const TArray<UE::Geometry::FSegment3d>& Lines = SelectionData->RenderEdges;
		SelectionDisplay->CreateOrUpdateLineSet(TEXT("SelectionEdges"),  Lines.Num(),
			[&](int32 Index, TArray<FRenderableLine>& LinesOut) 
			{
				const UE::Geometry::FSegment3d& Segment = Lines[Index];
				FVector3d A = Transform.TransformPosition(Segment.StartPoint());
				FVector3d B = Transform.TransformPosition(Segment.EndPoint());
				LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, ROIBorderColor, ROIBorderThickness, ROIBorderDepthBias));
			}, 1);

		SelectionDisplay->SetLineSetVisibility(TEXT("SelectionEdges"), true);
	}
	else
	{
		SelectionDisplay->SetLineSetVisibility(TEXT("SelectionEdges"), false);
	}

}




bool UE::Geometry::RegisterPersistentMeshSelectionManager(UInteractiveToolsContext* ToolsContext)
{
	if (ensure(ToolsContext))
	{
		UDEPRECATED_PersistentMeshSelectionManager* Found = ToolsContext->ContextObjectStore->FindContext<UDEPRECATED_PersistentMeshSelectionManager>();
		if (Found == nullptr)
		{
			UDEPRECATED_PersistentMeshSelectionManager* SelectionManager = NewObject<UDEPRECATED_PersistentMeshSelectionManager>(ToolsContext->ToolManager);
			if (ensure(SelectionManager))
			{
				SelectionManager->Initialize(ToolsContext);
				ToolsContext->ContextObjectStore->AddContextObject(SelectionManager);
				return true;
			}
			else
			{
				return false;
			}
		}
		return true;
	}
	return false;
}



bool UE::Geometry::DeregisterPersistentMeshSelectionManager(UInteractiveToolsContext* ToolsContext)
{
	if (ensure(ToolsContext))
	{
		UDEPRECATED_PersistentMeshSelectionManager* Found = ToolsContext->ContextObjectStore->FindContext<UDEPRECATED_PersistentMeshSelectionManager>();
		if (Found != nullptr)
		{
			Found->Shutdown();
			ToolsContext->ContextObjectStore->RemoveContextObject(Found);
		}
		return true;
	}
	return false;
}


UDEPRECATED_PersistentMeshSelectionManager* UE::Geometry::FindPersistentMeshSelectionManager(UInteractiveToolManager* ToolManager)
{
	if (ensure(ToolManager))
	{
		UDEPRECATED_PersistentMeshSelectionManager* Found = ToolManager->GetContextObjectStore()->FindContext<UDEPRECATED_PersistentMeshSelectionManager>();
		if (Found != nullptr)
		{
			return Found;
		}
	}
	return nullptr;
}




void FPersistentMeshSelectionChange::Apply(UObject* Object)
{
	UDEPRECATED_PersistentMeshSelectionManager* SelectionManager = Cast<UDEPRECATED_PersistentMeshSelectionManager>(Object);
	if (SelectionManager)
	{
		UPersistentMeshSelection* NewSelection = NewObject<UPersistentMeshSelection>(SelectionManager);
		NewSelection->SetSelection(To);
		SelectionManager->SetNewActiveSelectionInternal(NewSelection);
	}
}

void FPersistentMeshSelectionChange::Revert(UObject* Object)
{
	UDEPRECATED_PersistentMeshSelectionManager* SelectionManager = Cast<UDEPRECATED_PersistentMeshSelectionManager>(Object);
	if (SelectionManager)
	{
		UPersistentMeshSelection* NewSelection = NewObject<UPersistentMeshSelection>(SelectionManager);
		NewSelection->SetSelection(From);
		SelectionManager->SetNewActiveSelectionInternal(NewSelection);
	}
}

bool FPersistentMeshSelectionChange::HasExpired(UObject* Object) const
{
	return false;
}

FString FPersistentMeshSelectionChange::ToString() const
{
	return TEXT("PersistentMeshSelectionChange");
}



PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE