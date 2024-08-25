// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FEditorModeTools;
class IAvaDetailsProvider;
class IDetailKeyframeHandler;
class SOperatorStackEditorWidget;
class UPropertyAnimatorCoreBase;
class UObject;
class UActorModifierCoreBase;

/** Widget for the operator stack */
class SAvaOperatorStackTab : public SCompoundWidget
{
public:
	static inline const FName PanelTag = TEXT("AvaOperatorStackTab");
	
	SLATE_BEGIN_ARGS(SAvaOperatorStackTab) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<IAvaDetailsProvider>& InProvider);

	virtual ~SAvaOperatorStackTab() override;

private:
	void RefreshSelection(UObject* InSelectionObject) const;
	void OnModifierUpdated(UActorModifierCoreBase* InUpdatedItem) const;
	void OnControllerUpdated(UPropertyAnimatorCoreBase* InController) const;
	void RefreshCurrentSelection(const UObject* InObject) const;
	
	TWeakPtr<IAvaDetailsProvider> DetailsProviderWeak;

	TSharedPtr<SOperatorStackEditorWidget> OperatorStack;
};