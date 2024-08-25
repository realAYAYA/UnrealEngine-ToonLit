// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "GeometryMaskCanvas.h"
#include "GMEViewModelShared.h"

class FGMEResourceItemViewModel;

class FGMEResourceListViewModel
	: public TSharedFromThis<FGMEResourceListViewModel>
	, public FGMEListViewModelBase
	, public FEditorUndoClient
	, public IGMETreeNodeViewModel
{
	using Super = FGMEListViewModelBase;

protected:
	// Private token only allows members or friends to call MakeShared
	struct FPrivateToken { explicit FPrivateToken() = default; };
	
public:
	static TSharedRef<FGMEResourceListViewModel> Create();
	
	explicit FGMEResourceListViewModel(FPrivateToken): Super(Super::FPrivateToken{}) { }
	virtual ~FGMEResourceListViewModel() override;

	// ~Begin IGMETreeNodeViewModel
	virtual bool GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren) override;
	// ~End IGMETreeNodeViewModel

protected:
	virtual void Initialize() override;
	virtual bool RefreshItems() override;

	void OnResourceCreated(const UGeometryMaskCanvasResource* InGeometryMaskResource);
	void OnResourceDestroyed(const UGeometryMaskCanvasResource* InGeometryMaskResource);

private:
	FDelegateHandle OnResourceCreatedHandle;
	FDelegateHandle OnResourceDestroyedHandle;

	/** Canvas ViewModels */
	TArray<TSharedPtr<FGMEResourceItemViewModel>> ResourceItems;
};
