// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"
#include "RigVMCore/RigVMMemoryStorageStruct.h"

class URigVMPin;
class IDetailCategoryBuilder;
class UAnimNextGraph_EdGraphNode;

namespace UE::AnimNext::Editor
{

class FAnimNextGraph_EdGraphNodeCustomization : public IDetailCustomization
{
private:

	/** Called when details should be customized */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	FText GetName() const;
	void SetName(const FText& InNewText, ETextCommit::Type InCommitType);
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);

	static void GenerateMemoryStorage(const TArray<URigVMPin*>& ModelPinsToDisplay, FRigVMMemoryStorageStruct& MemoryStorage);
	static void PopulateCategory(IDetailCategoryBuilder& Category, const TArray<URigVMPin*>& ModelPinsToDisplay, FRigVMMemoryStorageStruct& MemoryStorage, UAnimNextGraph_EdGraphNode* EdGraphNode);


	FRigVMMemoryStorageStruct MemoryStorage;
};

}
