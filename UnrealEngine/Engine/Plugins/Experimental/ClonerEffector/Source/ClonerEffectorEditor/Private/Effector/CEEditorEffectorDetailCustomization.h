// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"

class IPropertyHandle;
class SWidget;
class UEnum;
struct FSlateBrush;

/* Used to customize effector actor properties in details panel */
class FCEEditorEffectorDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FCEEditorEffectorDetailCustomization>();
	}

	explicit FCEEditorEffectorDetailCustomization()
	{
		RegisterCustomSections();
	}

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization

protected:
	void RegisterCustomSections() const;

	void PopulateEasingInfos();
	FName GetCurrentEasingName() const;
	const FSlateBrush* GetEasingImage(FName InName) const;
	FText GetEasingText(FName InName) const;

	TSharedRef<SWidget> OnGenerateEasingEntry(FName InName) const;
	void OnSelectionChanged(FName InSelection, ESelectInfo::Type InSelectInfo) const;

	TWeakObjectPtr<UEnum> EasingEnum;
	TArray<FName> EasingNames;
	TSharedPtr<IPropertyHandle> EasingPropertyHandle;
};
