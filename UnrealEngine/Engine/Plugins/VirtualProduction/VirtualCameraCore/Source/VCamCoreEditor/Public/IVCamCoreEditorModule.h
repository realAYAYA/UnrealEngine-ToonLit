// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SubclassOf.h"

class UVCamWidget;

namespace UE::VCamCoreEditor
{
	class IConnectionRemapCustomization;
	
	DECLARE_DELEGATE_RetVal(TSharedRef<IConnectionRemapCustomization>, FGetConnectionRemappingCustomization)
	
	/** Passed to IConnectionTargetRemappingCustomizers to re-use functionality. */
	class VCAMCOREEDITOR_API IVCamCoreEditorModule : public IModuleInterface
	{
	public:

		static IVCamCoreEditorModule& Get()
		{
			return FModuleManager::Get().GetModuleChecked<IVCamCoreEditorModule>("VCamCoreEditor");
		}

		/**
		 * Registers a specific remapping customization for a class. At most one customization is used per VCamWidget class.
		 * To determine a widget's customization, the class's hierarchy is navigated recursively until a customization is found that is registered with a parent class.
		 * Cannot override already registered customizations. You'd have to call UnregisterConnectionTargetRemappingCustomizer first.
		 * 
		 * @param Class The subclass to register for - cannot be UVCamWidget and
		 * @param GetterDelegate The factory to invoke whenever a widget is customized. 
		 */
		virtual void RegisterConnectionRemapCustomization(TSubclassOf<UVCamWidget> Class, FGetConnectionRemappingCustomization GetterDelegate) = 0;
		virtual void UnregisterConnectionRemapCustomization(TSubclassOf<UVCamWidget> Class) = 0;

		/** Gets the asset category that assets are grouped under. */
		virtual uint32 GetAdvancedAssetCategoryForVCam() const = 0;
		
		virtual ~IVCamCoreEditorModule() override = default;
	};
}
