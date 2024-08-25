// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EditorUndoClient.h"
#include "GMEViewModelShared.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class UCanvasRenderTarget2D;
class UGeometryMaskCanvasResource;
class UGeometryMaskCanvas;
class UTexture;

class FGMEResourceItemViewModel
	: public TSharedFromThis<FGMEResourceItemViewModel>
	, public FGMETickableViewModelBase
	, public FEditorUndoClient
	, public IGMETreeNodeViewModel
{
protected:
	// Private token only allows members or friends to call MakeShared
	struct FPrivateToken { explicit FPrivateToken() = default; };
	
public:
	static TSharedRef<FGMEResourceItemViewModel> Create(const TWeakObjectPtr<const UGeometryMaskCanvasResource>& InResource);

	FGMEResourceItemViewModel(FPrivateToken, const TWeakObjectPtr<const UGeometryMaskCanvasResource>& InResource);
	virtual ~FGMEResourceItemViewModel() override = default;

	virtual bool Tick(const float InDeltaSeconds) override;
	
	// ~Begin IGMETreeNodeViewModel
	virtual bool GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren) override;
	// ~End IGMETreeNodeViewModel

	uint32 GetId() const { return UniqueId; }
	const FText& GetResourceInfo() const;
	const UCanvasRenderTarget2D* GetResourceTexture() const { return ResourceTextureWeak.Get(); }
	float GetMemoryUsage() const;
	FIntPoint GetDimensions() const;

private:
	void UpdateInfoText();

private:
	uint32 UniqueId;
	FText InfoText;
	TWeakObjectPtr<const UGeometryMaskCanvasResource> ResourceWeak;
	TWeakObjectPtr<UCanvasRenderTarget2D> ResourceTextureWeak;
};
