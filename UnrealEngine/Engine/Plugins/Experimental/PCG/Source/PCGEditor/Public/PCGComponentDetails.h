// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtr.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class UPCGComponent;

class FPCGComponentDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ~Begin IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	virtual void PendingDelete() override;
	/** ~End IDetailCustomization interface */

protected:
	virtual void GatherPCGComponentsFromSelection(const TArray<TWeakObjectPtr<UObject>>& InObjectSelected);
	virtual bool AddDefaultProperties() const { return true; }

private:
	FReply OnGenerateClicked();
	FReply OnCleanupClicked();
	FReply OnClearPCGLinkClicked();
	void OnGraphChanged(UPCGComponent* InComponent);

protected:
	TArray<TWeakObjectPtr<UPCGComponent>> SelectedComponents;
};