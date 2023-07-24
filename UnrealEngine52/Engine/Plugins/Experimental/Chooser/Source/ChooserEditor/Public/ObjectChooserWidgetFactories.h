// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "StructViewerModule.h"

namespace UE::ChooserEditor
{
	typedef TFunction<TSharedRef<SWidget>(UObject* TransactionObject, void* Value, UClass* ContextClass)> FChooserWidgetCreator;

	class CHOOSEREDITOR_API FObjectChooserWidgetFactories
	{
	public:
		static TMap<const UStruct*, FChooserWidgetCreator> ChooserWidgetCreators;

		static TSharedPtr<SWidget> CreateWidget(UObject* TransactionObject, void* Value, const UStruct* ValueType, UClass* ContextClass);
		static TSharedPtr<SWidget> CreateWidget(UObject* TransactionObject, const UScriptStruct* BaseType, void* Value, const UStruct* ValueType, UClass* ContextClass, const FOnStructPicked& CreateClassCallback, TSharedPtr<SBorder>* InnerWidget = nullptr);
		
		static void RegisterWidgets();
	};
}
