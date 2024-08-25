// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Input/Reply.h"

enum class EAvaTextLength : uint8;
class SEditableTextBox;
class SMultiLineEditableTextBox;

class FAvaTextFieldCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	
	// BEGIN IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
	                             class FDetailWidgetRow& HeaderRow,
	                             IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	                               class IDetailChildrenBuilder& StructBuilder,
	                               IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// END IPropertyTypeCustomization interface
	
private:
	bool IsCharacterLimitEnabled() const;	
	EAvaTextLength GetCurrentTextLengthType() const;
	void UpdateTextFieldValidity(const FText& InText);	
	
	void Refresh(const FText& InText);
	void ResetText();
	
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType) const;
	bool OnVerifyTextChanged(const FText& InText, FText& OutErrorText);
	FReply OnKeyDownInTextField(const FGeometry& Geometry, const FKeyEvent& KeyEvent);
	void OnMaximumLengthEnumChanged();

	TSharedPtr<SMultiLineEditableTextBox> MultilineTextBox;
	
	TSharedPtr<IPropertyHandle> Text_Handle;
	TSharedPtr<IPropertyHandle> MaximumLengthEnum_Handle;
	TSharedPtr<IPropertyHandle> MaxTextCount_Handle;
	TSharedPtr<IPropertyHandle> TextCase_Handle;
	
	FText DefaultText;
	bool bTextFieldIsValid = true;
};
