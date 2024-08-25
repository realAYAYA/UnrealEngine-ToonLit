// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "Delegates/IDelegateInstance.h"
#include "EditorUndoClient.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnailPool;
class FScopedTransaction;
class IDetailKeyframeHandler;
class IDetailTreeNode;
class IPropertyHandle;
class IPropertyRowGenerator;
class SBox;
class SDMSlot;
class SDMStage;
class SWidget;
class UDMMaterialComponent;
class UDMMaterialEffect;
class UDMMaterialEffectStack;
class UDMMaterialSlot;
class UDMMaterialStageExpressionTextureSample;
class UDMMaterialStageInputTextureUV;
class UDMMaterialStageInputThroughput;
class UDMMaterialStageInputValue;
class UDMMaterialStageSource;
class UDMMaterialStageThroughput;
class UDMMaterialValueFloat1;
class UDMTextureUV;
class UMaterial;
enum class ECheckBoxState : uint8;
struct FDMPropertyHandle;
struct FSlateIcon;

class DYNAMICMATERIALEDITOR_API SDMComponentEdit : public SCompoundWidget, public FSelfRegisteringEditorUndoClient
{
public:
	static ECheckBoxState IsInputPerChannelMapped(UDMMaterialStageThroughput* InThroughput, int32 InInputIdx);
	static FText GetInputChannelMapDescription(UDMMaterialStageThroughput* InThroughput, int32 InInputIdx, int32 InChannelIdx);
	static void CreateKeyFrame(TSharedPtr<IPropertyHandle> InPropertyHandle);
	static TSharedRef<SWidget> CreateExtensionButtons(const TSharedPtr<SWidget>& InPropertyOwner, UDMMaterialComponent* InComponent,
		const FName& InPropertyName, bool bInAllowKeyframe, FSimpleDelegate InOnResetDelegate);
	static TSharedRef<SWidget> CreateExtensionButtons(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
		const FName& InPropertyName, bool bInAllowKeyframe);

	SLATE_BEGIN_ARGS(SDMComponentEdit)
		{}
	SLATE_END_ARGS()

	SDMComponentEdit() = default;
	virtual ~SDMComponentEdit() override;

	void Construct(const FArguments& InArgs, UDMMaterialComponent* InComponent, const TWeakPtr<SDMSlot>& InSlotWidget);

	FORCEINLINE UDMMaterialComponent* GetComponent() const { return ComponentWeak.Get(); }
	FORCEINLINE TSharedPtr<SDMSlot> GetSlotWidget() const { return SlotWidgetWeak.Pin(); }

	TSharedPtr<SWidget> CreateSinglePropertyEditWidget(UDMMaterialComponent* InComponent, const FName& InPropertyName);

	//~ Begin FUndoClient
	virtual void PostUndo(bool bInSuccess) override { OnUndo(); }
	virtual void PostRedo(bool bInSuccess) override { OnUndo(); }
	//~ End FUndoClient

protected:
	TWeakObjectPtr<UDMMaterialComponent> ComponentWeak;
	TWeakPtr<SDMSlot> SlotWidgetWeak;

	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler;

	TSharedPtr<SBox> Container;

	bool bCreatedWithLinkedUVs;

	FDelegateHandle UpdateHandle;

	TSharedRef<SWidget> CreateEditWidget();

	TArray<FDMPropertyHandle> GetEditRows();

	TSharedRef<SWidget> CreateSourceTypeEditWidget();

	TSharedRef<SWidget> MakeSourceTypeEditWidgetMenuContent();
	FText GetSourceTypeEditWidgetText() const;

	void OnUndo();
};
