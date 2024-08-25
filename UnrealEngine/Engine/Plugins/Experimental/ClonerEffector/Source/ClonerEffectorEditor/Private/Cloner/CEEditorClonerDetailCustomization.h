// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

class FReply;

/* Used to customize cloner actor properties in details panel */
class FCEEditorClonerDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FCEEditorClonerDetailCustomization>();
	}

	explicit FCEEditorClonerDetailCustomization()
	{
		RegisterCustomSections();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;

protected:
	/** Execute ufunction with that name on selected objects */
	FReply OnExecuteFunction(FName InFunctionName);

	/** Register custom categories section in details panel */
	void RegisterCustomSections() const;

	TMap<FName, TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>> LayoutFunctionNames;
};
