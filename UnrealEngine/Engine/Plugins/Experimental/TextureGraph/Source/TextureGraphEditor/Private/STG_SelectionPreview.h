// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "2D/Tex.h"
#include "EdGraph/TG_EdGraphNode.h"
#include "CoreMinimal.h"
#include "SBlobTile.h"
#include "SBlobTileView.h"
#include "Widgets/SBoxPanel.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SCompoundWidget.h"
#include <Widgets/Input/SCheckBox.h>
#include <Widgets/Text/STextBlock.h>
#include "STextureHistogram.h"
#include "Widgets/STG_RGBAButtons.h"

class STG_TexturePreviewViewport;
class UTG_Script;
class UTexture;
class SImage;

DECLARE_DELEGATE_OneParam(FOnBlobSelectionChanged, BlobPtr Blob);

enum class ETSZoomMode : uint8
{
	Custom,
	Fit,
	Fill
};

class STG_SelectionPreview : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STG_SelectionPreview)
	{}
	SLATE_EVENT(FOnBlobSelectionChanged,OnBlobSelectionChanged)
	
	SLATE_END_ARGS()

public:
	
	void Construct(const FArguments& InArgs);
	ECheckBoxState IsPreviewLocked() const;
	FText GetLockPreviewToolTip() const;
	void OnCheckBoxStateChanged(ECheckBoxState NewState);
	ECheckBoxState GetCheckBoxState() const;
	void ConstructBlobView(BlobPtr InBlob = nullptr);
	FText GetOutputDetailsText() const;
	FString BufferToString(BufferDescriptor Desc);
	void UpdatePixelInfo();
	void OnTexturePreviewMouseHover();
	
	void OnSelectionChanged(UTG_EdGraphNode* InNode);
	void UpdateOutputDetailsText(BlobPtr InBlob);
	void UpdateOutputDetailsText(UTG_Pin* Pin);
	void ResetOutputDetailsText();
	void UpdatePreview();

	UTexture* GetTexture() const;
	void SetTexture(BlobPtr InBlob = nullptr);
	void SetTextureProperties();
	void CalculateTextureDimensions(int32& OutWidth, int32& OutHeight, int32& OutDepth, int32& OutArraySize, bool bInIncludeBorderSize) const;
	int32 GetExposureBias() const { return ExposureBias; }
	ESimpleElementBlendMode GetColourChannelBlendMode();

	void ZoomIn();
	void ZoomOut();
	double CalculateDisplayedZoomLevel() const;
	void SetCustomZoomLevel(double ZoomValue);
	double GetCustomZoomLevel() const;

	UTG_EdGraphNode* GetSelectedNode();
	void OnSelectedNodeDeleted();

	bool GetIsSingleChannel() { return IsSingleChannel; }
	bool GetIsSRGB() { return bSRGB; }

#if UE_BUILD_DEBUG
	FReply OnDumpClick();
#endif 

private:
	FOnBlobSelectionChanged OnBlobSelectionChanged;
	FReply HandleZoomPlusButtonClicked();
	FReply HandleZoomMinusButtonClicked();
	TSharedRef<SWidget> MakeZoomControlWidget();
	FText HandleZoomPercentageText() const;
	void HandleZoomMenuEntryClicked(double ZoomValue);
	void HandleZoomMenuFitClicked();
	bool IsZoomMenuFitChecked() const;
	void HandleZoomMenuFillClicked();
	bool IsZoomMenuFillChecked() const;

	TSharedRef<SWidget> OnGenerateOutputMenu();
	FText HandleOutputText() const;
	void HandleOutputChanged(FString OuputName);
	void OnLockChanged(ECheckBoxState NewCheckState);
	bool GetIsLockedNode() { return IsLocked && LockedNode != nullptr; }
	bool IsLockButtonEnable() { return SelectedNode || LockedNode; }
	
	const FSlateBrush* OnGetLockButtonImageResource() const;
	FReply OnLockClick();
	
	bool UseSpecifiedOutput(UTG_EdGraphNode* Node);
	int GetSelectedPinIndex(UTG_EdGraphNode* InNode, const UEdGraphPin* SelectedPin);
	UTexture* GetTextureFromBuffer(DeviceBufferPtr Buffer);
	void GetTextureToUse() const;
	
	TSharedPtr<SBlobTileView> TileView;
	TSharedPtr<SWidget> TileViewScaleBox;
	TSharedPtr<STextBlock> HeadingText;
	TSharedPtr<SVerticalBox> VerticalBox;
	TSharedPtr<SCheckBox> CheckBox;
	TSharedPtr<STextBlock> OutputTextBlock;
	TSharedPtr<SWidget> LockButton;
	TSharedPtr<STG_RGBAButtons> RGBAButtons;
	bool ShowTileView;
	FMargin Margin;
	UTG_EdGraphNode* SelectedNode;
	UTG_EdGraphNode* LockedNode;
	/** Which output should be shown */
	FString SpecifiedOutputName = "Output";
	FString OutputDetailsText = "";
	FString PixelInfo = "";
	int32 MaxOutputs;
	bool IsLocked = false;
	bool IsSingleChannel = false;
	bool bSRGB = false;

	UTexture* Texture = nullptr;
	BlobPtr PreviewBlob;
	/** Viewport */
	TSharedPtr<STG_TexturePreviewViewport> TextureViewport;
	FSlateRoundedBoxBrush* CheckedBrush;
	FSlateRoundedBoxBrush* UncheckedBrush;

	/** The maximum width/height at which the texture will render in the preview window */
	int32 PreviewEffectiveTextureWidth;
	int32 PreviewEffectiveTextureHeight;

	/** The texture's zoom factor. */
	double Zoom;
	/** This toolkit's current zoom mode **/
	ETSZoomMode ZoomMode;
	// Which exposure level should be used, in FStop e.g. 0:original, -1:half as bright, 1:2x as bright, 2:4x as bright.
	int32 ExposureBias;

	const double MaxZoom = 16.0;
	const double MinZoom = 1.0 / 64;
	// ZoomFactor is multiplicative such that an integer number of steps will give a power of two zoom (50% or 200%)
	const int ZoomFactorLogSteps = 8;
	const double ZoomFactor = pow(2.0, 1.0 / ZoomFactorLogSteps);

};
