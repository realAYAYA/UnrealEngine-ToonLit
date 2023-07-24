// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConnectionRemapUtilsImpl.h"
#include "IDetailCustomization.h"
#include "Customization/IConnectionRemapCustomization.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IDetailGroup;
class IPropertyUtilities;
class UVCamWidget;
class UVCamOutputProviderBase;

namespace UE::VCamCoreEditor::Private
{
	/** Exposes all VCam widgets' connection settings for as long as the output provider has a valid widget. */
	class FOutputProviderLayoutCustomization
		: public IDetailCustomization
	{
	public:

		static TSharedRef<IDetailCustomization> MakeInstance();
		virtual ~FOutputProviderLayoutCustomization() override;

		//~ Begin IDetailCustomization Interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;
		//~ End IDetailCustomization Interface

	private:

		/** Exactly 1 object can be customized at a time. */
		TWeakObjectPtr<UVCamOutputProviderBase> CustomizedOutputProvider;

		TWeakPtr<IDetailLayoutBuilder> WeakDetailBuilder;
		
		/** Handle to UVCamOutputProviderBase::OnActivatedDelegate */
		FDelegateHandle OnActivatedDelegateHandle;
		
		struct FWidgetData
		{
			TSharedPtr<IConnectionRemapCustomization> Customization;
			
			/** Utils for IConnectionRemapCustomizations */
			TSharedPtr<FConnectionRemapUtilsImpl> RemapUtils;
		};
		/** The widgets added to the details panel */
		TMap<TWeakObjectPtr<UVCamWidget>, FWidgetData> EditableWidgets;
		
		FDetailWidgetRow ExtendWidgetsRow(IDetailLayoutBuilder& DetailBuilder, IDetailGroup& WidgetGroup);
		void RebuildWidgetData();
		void GenerateWidgetRows(IDetailGroup& RootWidgetGroup, IDetailLayoutBuilder& DetailBuilder);
		TSharedRef<SHorizontalBox> CreateControlWidgets(const TWeakObjectPtr<UVCamWidget>& Widget) const;
		
		void OnActivationChanged(bool bNewIsActivated) const;
		void ForceRefreshDetailsIfSafe() const;
		
		static void ClearWidgetData(TMap<TWeakObjectPtr<UVCamWidget>, FWidgetData>& InEditableWidgets);
	};
}

