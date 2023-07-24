// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"

struct ENGINE_API FSparseVolumeRawSourcePackedData;
enum class ESparseVolumePackedDataFormat : uint8;

struct FOpenVDBGridComponentInfo
{
	uint32 Index;
	uint32 ComponentIndex;
	FString Name;
	FString DisplayString; // Contains source file grid index, name and component (if it is a multi component type like Float3)
};

class SOpenVDBComponentPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOpenVDBComponentPicker)
		: _PackedData()
		, _ComponentIndex()
		, _OpenVDBGridComponentInfo()
	{}

		SLATE_ARGUMENT(FSparseVolumeRawSourcePackedData*, PackedData)
		SLATE_ARGUMENT(uint32, ComponentIndex)
		SLATE_ARGUMENT(TArray<TSharedPtr<FOpenVDBGridComponentInfo>>*, OpenVDBGridComponentInfo)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	void RefreshUIFromData();

private:
	FSparseVolumeRawSourcePackedData* PackedData;
	uint32 ComponentIndex;
	TArray<TSharedPtr<FOpenVDBGridComponentInfo>>* OpenVDBGridComponentInfo;
	TSharedPtr<SComboBox<TSharedPtr<FOpenVDBGridComponentInfo>>> GridComboBox;
};

class SOpenVDBPackedDataConfigurator : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOpenVDBPackedDataConfigurator)
		: _PackedData()
		, _OpenVDBGridComponentInfo()
		, _OpenVDBSupportedTargetFormats()
		, _PackedDataName()
	{}

		SLATE_ARGUMENT(FSparseVolumeRawSourcePackedData*, PackedData)
		SLATE_ARGUMENT(TArray<TSharedPtr<FOpenVDBGridComponentInfo>>*, OpenVDBGridComponentInfo)
		SLATE_ARGUMENT(TArray<TSharedPtr<ESparseVolumePackedDataFormat>>*, OpenVDBSupportedTargetFormats)
		SLATE_ARGUMENT(FText, PackedDataName)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	void RefreshUIFromData();

private:
	FSparseVolumeRawSourcePackedData* PackedData;
	TSharedPtr<SOpenVDBComponentPicker> ComponentPickers[4];
	TArray<TSharedPtr<ESparseVolumePackedDataFormat>>* OpenVDBSupportedTargetFormats;
	TSharedPtr<SComboBox<TSharedPtr<ESparseVolumePackedDataFormat>>>	FormatComboBox;
	TSharedPtr<SCheckBox> RemapUnormCheckBox;
};

class SOpenVDBImportWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOpenVDBImportWindow)
		: _PackedDataA()
		, _PackedDataB()
		, _OpenVDBGridComponentInfo()
		, _FileInfoString()
		, _OpenVDBSupportedTargetFormats()
		, _WidgetWindow()
		, _FullPath()
		, _MaxWindowHeight(0.0f)
		, _MaxWindowWidth(0.0f)
	{}

		SLATE_ARGUMENT(FSparseVolumeRawSourcePackedData*, PackedDataA)
		SLATE_ARGUMENT(FSparseVolumeRawSourcePackedData*, PackedDataB)
		SLATE_ARGUMENT(TArray<TSharedPtr<FOpenVDBGridComponentInfo>>*, OpenVDBGridComponentInfo)
		SLATE_ARGUMENT(FString, FileInfoString)
		SLATE_ARGUMENT(TArray<TSharedPtr<ESparseVolumePackedDataFormat>>*, OpenVDBSupportedTargetFormats)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(FText, FullPath)
		SLATE_ARGUMENT(float, MaxWindowHeight)
		SLATE_ARGUMENT(float, MaxWindowWidth)
	SLATE_END_ARGS()

public:
	virtual bool SupportsKeyboardFocus() const override { return true; }
	void Construct(const FArguments& InArgs);
	FReply OnImport();
	FReply OnCancel();
	bool ShouldImport() const;
	bool ShouldImportAsSequence() const;

private:
	FSparseVolumeRawSourcePackedData*					PackedDataA;
	FSparseVolumeRawSourcePackedData*					PackedDataB;
	TArray<TSharedPtr<FOpenVDBGridComponentInfo>>*		OpenVDBGridComponentInfo;
	TArray<TSharedPtr<ESparseVolumePackedDataFormat>>*	OpenVDBSupportedTargetFormats;
	TSharedPtr<SOpenVDBPackedDataConfigurator>			PackedDataAConfigurator;
	TSharedPtr<SOpenVDBPackedDataConfigurator>			PackedDataBConfigurator;
	TSharedPtr<SCheckBox>								ImportAsSequenceCheckBox;
	TSharedPtr<SButton>									ImportButton;
	TWeakPtr<SWindow>									WidgetWindow;
	bool												bShouldImport;

	EActiveTimerReturnType SetFocusPostConstruct(double InCurrentTime, float InDeltaTime);
	bool CanImport() const;
	FReply OnResetToDefaultClick();
	FText GetImportTypeDisplayText() const;
	void SetDefaultGridAssignment();
};