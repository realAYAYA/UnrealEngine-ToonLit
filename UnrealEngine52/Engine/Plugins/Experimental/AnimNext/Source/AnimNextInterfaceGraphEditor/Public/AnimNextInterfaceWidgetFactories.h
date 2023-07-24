// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "SClassViewer.h"

namespace UE::AnimNext::InterfaceGraphEditor
{
	class ANIMNEXTINTERFACEGRAPHEDITOR_API FAnimNextInterfaceWidgetFactories 
	{
	public:
		static TMap<const UClass*, TFunction<void (const UObject* Object, FText& OutText)>> AnimNextInterfaceTextConverter;
		static TMap<const UClass*, TFunction<TSharedRef<SWidget> (UObject* Object)>> AnimNextInterfaceWidgetCreators;

		static TSharedPtr<SWidget> CreateAnimNextInterfaceWidget(FName AnimNextInterfaceTypeName, UObject* Value, const FOnClassPicked& CreateClassCallback, TSharedPtr<SBorder>* InnerWidget = nullptr);
		
		static void RegisterWidgets();
	};
}
