// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "SClassViewer.h"

namespace UE::DataInterfaceGraphEditor
{
	class DATAINTERFACEGRAPHEDITOR_API FDataInterfaceWidgetFactories 
	{
	public:
		static TMap<const UClass*, TFunction<void (const UObject* Object, FText& OutText)>> DataInterfaceTextConverter;
		static TMap<const UClass*, TFunction<TSharedRef<SWidget> (UObject* Object)>> DataInterfaceWidgetCreators;

		static TSharedPtr<SWidget> CreateDataInterfaceWidget(FName DataInterfaceTypeName, UObject* Value, const FOnClassPicked& CreateClassCallback, TSharedPtr<SBorder>* InnerWidget = nullptr);
		
		static void RegisterWidgets();
	};
}
