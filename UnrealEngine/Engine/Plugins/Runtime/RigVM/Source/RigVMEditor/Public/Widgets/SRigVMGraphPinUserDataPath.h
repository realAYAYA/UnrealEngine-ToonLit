// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SButton.h"
#include "SGraphPin.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMBlueprint.h"
#include "RigVMCore/RigVMAssetUserData.h"
#include "IPropertyAccessEditor.h"

class RIGVMEDITOR_API SRigVMUserDataPath : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SRigVMUserDataPath)
	: _ModelPins()
	{}

	SLATE_ARGUMENT(TArray<URigVMPin*>, ModelPins)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:

	FString GetUserDataPath(URigVMPin* ModelPin) const;
	FString GetUserDataPath() const;
	FText GetUserDataPathText() const { return FText::FromString(GetUserDataPath()); }
	const FSlateBrush* GetUserDataIcon() const;
	FLinearColor GetUserDataColor() const;
	const FSlateBrush* GetUserDataIcon(const UNameSpacedUserData::FUserData* InUserData) const;
	FLinearColor GetUserDataColor(const UNameSpacedUserData::FUserData* InUserData) const;
	TSharedRef<SWidget> GetTopLevelMenuContent();
	void FillUserDataPathMenu( FMenuBuilder& InMenuBuilder, FString InParentPath );
	void HandleSetUserDataPath(FString InUserDataPath);

	URigVMBlueprint* GetBlueprint() const;
	FString GetUserDataNameSpace(URigVMPin* ModelPin) const;
	FString GetUserDataNameSpace() const;
	const UNameSpacedUserData* GetUserDataObject() const;
	const UNameSpacedUserData::FUserData* GetUserData() const;

	TArray<URigVMPin*> ModelPins;
	TSharedPtr<SMenuAnchor> MenuAnchor;
	bool bAllowUObjects = false;
};

class RIGVMEDITOR_API  SRigVMGraphPinUserDataPath : public SGraphPin
{
public:

	SLATE_BEGIN_ARGS(SRigVMGraphPinUserDataPath){}

		SLATE_ARGUMENT(TArray<URigVMPin*>, ModelPins)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	TArray<URigVMPin*> ModelPins;
};
