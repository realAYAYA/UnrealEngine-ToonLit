// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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
	class FOutputProviderLayoutCustomization : public IDetailCustomization
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

		/**
		 * Whether a rebuild was already requested this tick.
		 * The output provider may activate & deactivate multiple times in a single tick so we combine them all.
		 * By deferring the update not only do we save useless updates, we can also detect whether the owning object was destroyed as part of an undo.
		 */
		bool bRequestedRefresh = false;
		
		FDetailWidgetRow ExtendWidgetsRow(IDetailLayoutBuilder& DetailBuilder, IDetailGroup& WidgetGroup);
		void RebuildWidgetData();
		void GenerateWidgetRows(IDetailGroup& RootWidgetGroup, IDetailLayoutBuilder& DetailBuilder);
		TSharedRef<SHorizontalBox> CreateControlWidgets(const TWeakObjectPtr<UVCamWidget>& Widget) const;
		
		void OnActivationChanged(bool bNewIsActivated);
		void ForceRefreshDetailsIfSafe() const;
		
		static void ClearWidgetData(TMap<TWeakObjectPtr<UVCamWidget>, FWidgetData>& InEditableWidgets);
	};
}

