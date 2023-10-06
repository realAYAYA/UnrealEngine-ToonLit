// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UObject;
class UUserWidget;

namespace UE::UMG::Editor
{

	class UMGEDITOR_API FPreviewMode
	{
	public:
		void SetSelectedObject(TArray<TWeakObjectPtr<UObject>> Objects);
		void SetSelectedObject(TArrayView<UObject*> Objects);
		TArray<UObject*> GetSelectedObjectList() const;

		FSimpleMulticastDelegate& OnSelectedObjectChanged()
		{
			return SelectedObjectChangedDelegate;
		}

		void SetPreviewWidget(UUserWidget* Widget);

		UUserWidget* GetPreviewWidget() const
		{
			return PreviewWidget.Get();
		}

		FSimpleMulticastDelegate& OnPreviewWidgetChanged()
		{
			return PreviewWidgetChangedDelegate;
		}

	private:
		TArray<TWeakObjectPtr<UObject>> SelectedObjects;
		TWeakObjectPtr<UUserWidget> PreviewWidget;
		FSimpleMulticastDelegate SelectedObjectChangedDelegate;
		FSimpleMulticastDelegate PreviewWidgetChangedDelegate;
	};

} // namespace UE::UMG