// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"
#include "Rendering/RenderingCommon.h"
#include "SGraphNode.h"
#include "SNodePanel.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Textures/SlateShaderResource.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FMaterialRenderProxy;
class FRHICommandListImmediate;
class FSlateRect;
class FWidgetStyle;
class SGraphPin;
class SOverlay;
class SVerticalBox;
class SWidget;
class UMaterialGraphNode;
struct FGeometry;
struct FSlateBrush;

typedef TSharedPtr<class FPreviewElement, ESPMode::ThreadSafe> FThreadSafePreviewPtr;

class FPreviewViewport : public ISlateViewport
{
public:
	FPreviewViewport(class UMaterialGraphNode* InNode);
	~FPreviewViewport();

	// ISlateViewport interface
	virtual void OnDrawViewport( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, class FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) override;
	virtual FIntPoint GetSize() const override;
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const override {return NULL;}
	virtual bool RequiresVsync() const override {return false;}

	/** Material node to get expression preview from */
	UMaterialGraphNode* MaterialNode;
	/** Custom Slate Element to display the preview */
	FThreadSafePreviewPtr PreviewElement;
private:
	/**
	 * Updates the expression preview render proxy from a graph node
	 */
	void UpdatePreviewNodeRenderProxy();
};

class FPreviewElement : public ICustomSlateElement
{
public:
	FPreviewElement();
	~FPreviewElement();

	/**
	 * Sets up the canvas for rendering
	 *
	 * @param	InCanvasRect			Size of the canvas tile
	 * @param	InClippingRect			How to clip the canvas tile
	 * @param	InGraphNode				The graph node for the material preview
	 * @param	bInIsRealtime			Whether preview is using realtime values
	 *
	 * @return	Whether there is anything to render
	 */
	bool BeginRenderingCanvas( const FIntRect& InCanvasRect, const FIntRect& InClippingRect, UMaterialGraphNode* InGraphNode, bool bInIsRealtime );

	/**
	 * Updates the expression preview render proxy from a graph node on the render thread
	 */
	void UpdateExpressionPreview(UMaterialGraphNode* PreviewNode);
private:
	/**
	 * ICustomSlateElement interface 
	 */
	virtual void Draw_RenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer, const FSlateCustomDrawParams& Params) override;

private:
	/** Render target that the canvas renders to */
	class FSlateMaterialPreviewRenderTarget* RenderTarget;
	/** Render proxy for the expression preview */
	FMaterialRenderProxy* ExpressionPreview;
	/** Whether preview is using realtime values */
	bool bIsRealtime;
};

class GRAPHEDITOR_API SGraphNodeMaterialBase : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeMaterialBase){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UMaterialGraphNode* InNode);

	// SGraphNode interface
	virtual void CreatePinWidgets() override;
	// End of SGraphNode interface

	// SNodePanel::SNode interface
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
	// End of SNodePanel::SNode interface

	UMaterialGraphNode* GetMaterialGraphNode() const {return MaterialNode;}

	/* Populate a meta data tag with information about this graph node */
	virtual void PopulateMetaTag(class FGraphNodeMetaData* TagMeta) const override;

protected:
	// SGraphNode interface
	virtual void AddPin( const TSharedRef<SGraphPin>& PinToAdd ) override;
	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override;
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	virtual void OnAdvancedViewChanged(const ECheckBoxState NewCheckedState) override;
	// End of SGraphNode interface

	/** Creates a preview viewport if necessary */
	TSharedRef<SWidget> CreatePreviewWidget();

	/** Returns visibility of Expression Preview viewport */
	EVisibility ExpressionPreviewVisibility() const;

	/** Returns text to over lay over the expression preview viewport */
	FText ExpressionPreviewOverlayText() const;

	/** Show/hide Expression Preview */
	void OnExpressionPreviewChanged( const ECheckBoxState NewCheckedState );

	/** hidden == unchecked, shown == checked */
	ECheckBoxState IsExpressionPreviewChecked() const;

	/** Up when shown, down when hidden */
	const FSlateBrush* GetExpressionPreviewArrow() const;

protected:
	/** Slate viewport for rendering preview via custom slate element */
	TSharedPtr<FPreviewViewport> PreviewViewport;

	/** Cached material graph node pointer to avoid casting */
	UMaterialGraphNode* MaterialNode;
};
