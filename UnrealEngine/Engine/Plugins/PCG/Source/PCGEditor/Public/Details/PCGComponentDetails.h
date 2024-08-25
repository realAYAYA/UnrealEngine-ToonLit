// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FReply;
struct EVisibility;

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
	EVisibility GenerateButtonVisible() const;
	EVisibility CancelButtonVisible() const;
	EVisibility CleanupButtonVisible() const;
	EVisibility RefreshButtonVisible() const;
	FReply OnGenerateClicked();
	FReply OnCancelClicked();
	FReply OnCleanupClicked();
	FReply OnRefreshClicked();
	FReply OnClearPCGLinkClicked();
	void OnGraphChanged(UPCGComponent* InComponent);

protected:
	TArray<TWeakObjectPtr<UPCGComponent>> SelectedComponents;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "UObject/WeakObjectPtr.h"
#endif
