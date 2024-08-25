// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomNodeBuilder.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Attribute.h"

class IPropertyTypeCustomizationUtils;
class IPropertyUtilities;

namespace UE::VCamCoreEditor::Private
{
	/**
	 * Customizes the FWidgetConnectionConfig::ConnectionTargets property.
	 * Makes every key widget be a drop-down to the connection exposed its corresponding VCamWidget.
	 */
	class FConnectionTargetNodeBuilder
		: public IDetailCustomNodeBuilder
		, public TSharedFromThis<FConnectionTargetNodeBuilder>
	{
	public:

		FConnectionTargetNodeBuilder(TSharedRef<IPropertyHandle> ConnectionTargets, TAttribute<TArray<FName>> ChooseableConnections, IPropertyTypeCustomizationUtils& CustomizationUtils);

		void InitDelegates();

		//~ Begin IDetailCustomNodeBuilder Interface
		virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
		virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
		virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
		virtual FName GetName() const override { return TEXT("Connection Targets"); }
		//~ End IDetailCustomNodeBuilder Interface

	private:
		
		/** Handle to FWidgetConnectionConfig::ConnectionTargets */
		const TSharedRef<IPropertyHandle> ConnectionTargets;

		/** Gets the list of connections on the VCamWidget */
		const TAttribute<TArray<FName>> ChooseableConnections;

		const FSlateFontInfo RegularFont;
		const TSharedPtr<IPropertyUtilities> PropertyUtilities;

		FSimpleDelegate OnRegenerateChildren;

		TArray<FString> GetChooseableConnectionsAsStringArray() const;

		void ScheduleChildRebuild();
		void RebuilChildren();
	};
}


