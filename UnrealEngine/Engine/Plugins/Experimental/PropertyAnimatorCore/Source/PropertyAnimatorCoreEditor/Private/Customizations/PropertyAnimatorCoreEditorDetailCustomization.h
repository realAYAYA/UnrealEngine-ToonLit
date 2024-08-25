// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class FReply;
class SWidget;
class UPropertyAnimatorCoreBase;
class UToolMenu;
enum class ECheckBoxState : uint8;

/** Details customization for UPropertyAnimatorCoreBase */
class FPropertyAnimatorCoreEditorDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FPropertyAnimatorCoreEditorDetailCustomization>();
	}

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	//~ End IDetailCustomization

protected:
	TSharedRef<SWidget> GenerateLinkMenu();
	static void FillLinkMenu(UToolMenu* InToolMenu);

	bool IsAnyPropertyLinked() const;
	ECheckBoxState IsPropertiesEnabled() const;
	void OnPropertiesEnabled(ECheckBoxState InNewState) const;

	FReply UnlinkProperties() const;

	TWeakObjectPtr<UPropertyAnimatorCoreBase> AnimatorWeak;
};
