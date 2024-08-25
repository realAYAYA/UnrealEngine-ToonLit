// Copyright Epic Games, Inc. All Rights Reserved.

#include "GMEResourceListViewModel.h"

#include "Engine/Engine.h"
#include "GeometryMaskCanvasResource.h"
#include "GeometryMaskSubsystem.h"
#include "GeometryMaskWorldSubsystem.h"
#include "GMEResourceItemViewModel.h"

TSharedRef<FGMEResourceListViewModel> FGMEResourceListViewModel::Create()
{
	TSharedRef<FGMEResourceListViewModel> ViewModel = MakeShared<FGMEResourceListViewModel>(FPrivateToken{});
	ViewModel->Initialize();

	return ViewModel;
}

FGMEResourceListViewModel::~FGMEResourceListViewModel()
{
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		Subsystem->OnGeometryMaskResourceCreated().Remove(OnResourceCreatedHandle);
		OnResourceCreatedHandle.Reset();
		
		Subsystem->OnGeometryMaskResourceDestroyed().Remove(OnResourceDestroyedHandle);
		OnResourceDestroyedHandle.Reset();
	}
	
	ResourceItems.Reset();
}

void FGMEResourceListViewModel::Initialize()
{
	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		// Add currently utilized canvas resources
		ResourceItems.Reserve(4);
		
		for (const TObjectPtr<UGeometryMaskCanvasResource>& Resource : Subsystem->GetCanvasResources())
		{
			ResourceItems.Add(FGMEResourceItemViewModel::Create(Resource));
		}

		OnResourceCreatedHandle = Subsystem->OnGeometryMaskResourceCreated().AddRaw(this, &FGMEResourceListViewModel::OnResourceCreated);
		OnResourceDestroyedHandle = Subsystem->OnGeometryMaskResourceDestroyed().AddRaw(this, &FGMEResourceListViewModel::OnResourceDestroyed);
	}

	FGMEListViewModelBase::Initialize();
}

bool FGMEResourceListViewModel::RefreshItems()
{
	return true;
}

void FGMEResourceListViewModel::OnResourceCreated(const UGeometryMaskCanvasResource* InGeometryMaskResource)
{
	// Don't add if already in list
	if (ResourceItems.ContainsByPredicate([Id = InGeometryMaskResource->GetUniqueID()](const TSharedPtr<FGMEResourceItemViewModel>& InItemViewModel)
	{
		return InItemViewModel->GetId() == Id;
	}))
	{
		return;
	}

	ResourceItems.Add(FGMEResourceItemViewModel::Create(InGeometryMaskResource));

	OnChanged().Broadcast();
}

void FGMEResourceListViewModel::OnResourceDestroyed(const UGeometryMaskCanvasResource* InGeometryMaskResource)
{
	if (!InGeometryMaskResource)
	{
		return;
	}
	
	ResourceItems.RemoveAll([InGeometryMaskResource](const TSharedPtr<FGMEResourceItemViewModel>& InViewModel)
	{
		return InViewModel->GetId() == InGeometryMaskResource->GetUniqueID();		
	});

	OnChanged().Broadcast();
}

bool FGMEResourceListViewModel::GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren)
{
	const TArray<TSharedPtr<FGMEResourceItemViewModel>> Children = ResourceItems;
	OutChildren.Append(Children);
	return Children.Num() > 0;
}
