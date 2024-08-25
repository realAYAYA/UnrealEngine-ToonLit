// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IDetailPropertyRow;
class UVCamComponent;

namespace UE::VCamCoreEditor::Private
{
	namespace ConnectionTargetContextFinding
	{
		class IContextFinderForConnectionTargetSettings;
	}
	
	/**
	 * If the user as selected an actor with a VCamComponent or a Blueprint with a VCamComponent in the content browser,
	 * this will create drop-downs for the modifier and connection point properties instead of just text boxes.
	 */
	class FConnectionTargetSettingsTypeCustomization : public IPropertyTypeCustomization
	{
		template <typename TObjectType, ESPMode Mode>
		friend class SharedPointerInternals::TIntrusiveReferenceController;
	public:
		
		static TSharedRef<IPropertyTypeCustomization> MakeInstance(TSharedRef<ConnectionTargetContextFinding::IContextFinderForConnectionTargetSettings> ContextFinder);

		//~ Begin IPropertyTypeCustomization Interface
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		//~ End IPropertyTypeCustomization Interface

	private:

		TSharedRef<ConnectionTargetContextFinding::IContextFinderForConnectionTargetSettings> ContextFinder;

		FConnectionTargetSettingsTypeCustomization(TSharedRef<ConnectionTargetContextFinding::IContextFinderForConnectionTargetSettings> ContextFinder);
		
		void AddScopeRow(IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils);

		void CustomizeModifier(
			TSharedRef<IPropertyHandle> ModifierHandle,
			IDetailPropertyRow& Row, 
			TSharedRef<IPropertyHandle> ConnectionTargetSettingsStructHandle,
			TSharedRef<IPropertyUtilities> PropertyUtils
			) const;
		void CustomizeConnectionPoint(
			TSharedRef<IPropertyHandle> ModifierHandle,
			TSharedRef<IPropertyHandle> ConnectionPointHandle,
			IDetailPropertyRow& Row,
			TSharedRef<IPropertyHandle> ConnectionTargetSettingsStructHandle,
			TSharedRef<IPropertyUtilities> PropertyUtils
			) const;

		enum class EComponentSource
		{
			None,
			ContentBrowser,
			LevelSelection
		};
		struct FSelectedComponentInfo
		{
			EComponentSource ComponentSource = EComponentSource::None;
			TWeakObjectPtr<UVCamComponent> Component;
		};
		/** Gets component from 1. Blueprint class selected in content browser or 2. selected actor in world. */
		static FSelectedComponentInfo GetUserFocusedConnectionPointSource();
		
		void CustomizeNameProperty(
			TSharedRef<IPropertyHandle> PropertyHandle,
			IDetailPropertyRow& Row,
			TAttribute<TArray<FName>> GetOptionsAttr,
			TAttribute<bool> HasDataSourceAttr
			) const;
	};
}


