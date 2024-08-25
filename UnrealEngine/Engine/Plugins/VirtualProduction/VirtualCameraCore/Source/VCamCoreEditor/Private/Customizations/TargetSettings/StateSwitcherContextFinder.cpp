// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateSwitcherContextFinder.h"

#include "IPropertyUtilities.h"
#include "UI/VCamConnectionStructs.h"

#include "PropertyHandle.h"
#include "UI/Switcher/WidgetConnectionConfig.h"

namespace UE::VCamCoreEditor::Private::ConnectionTargetContextFinding
{
	struct FStateSwitcherSearch
	{
		/** = FVCamConnectionTargetSettings contained by FWidgetConnectionConfig::ConnectionTargets */
		const TSharedRef<IPropertyHandle>& ConnectionTargetSettingsStructHandle;
		IPropertyUtilities& PropertyUtils;
		TFunctionRef<void(const FVCamConnection& Connection)> ProcessWithContext;
		
		/** = FWidgetConnectionConfig::ConnectionTargets */
		TSharedPtr<IPropertyHandle> ParentMap;
		/** = FWidgetConnectionConfig */
		TSharedPtr<IPropertyHandle> ParentStruct;
		/** = FWidgetConnectionConfig::Child */
		TSharedPtr<IPropertyHandle> ChildWidget;
		bool InitPropertyHandles();
		
		const TMap<FName, FVCamConnectionTargetSettings>* ConnectionTargets;
		void* TargetSettingsAddress;
		bool InitTargetSettingsMapping();
		
		const UVCamWidget* RemappedWidget;
		bool FindReferencedVCamWidget();
		
		bool FindOverrideConnectionNameThenLookupWidgetConnections();
	};
	
	void FStateSwitcherContextFinder::FindAndProcessContext(
		const TSharedRef<IPropertyHandle>& ConnectionTargetSettingsStructHandle,
		IPropertyUtilities& PropertyUtils,
		TFunctionRef<void(const FVCamConnection& Connection)> ProcessWithContext,
		TFunctionRef<void()> ProcessWithoutContext)
	{
		FStateSwitcherSearch Search{ ConnectionTargetSettingsStructHandle, PropertyUtils, ProcessWithContext };
		const bool bSuccess =
			Search.InitPropertyHandles()
			&& Search.InitTargetSettingsMapping()
			&& Search.FindReferencedVCamWidget()
			&& Search.FindOverrideConnectionNameThenLookupWidgetConnections();
		if (!bSuccess)
		{
			ProcessWithoutContext();
		}
	}
	
	bool FStateSwitcherSearch::InitPropertyHandles()
	{
		// ConnectionTargetSettingsStructHandle = Entry in the ConnectionTargets TMap
		ParentMap = ConnectionTargetSettingsStructHandle->GetParentHandle();
		ParentStruct = ParentMap ? ParentMap->GetParentHandle() : nullptr;
		ChildWidget = ParentStruct ? ParentStruct->GetChildHandle(GET_MEMBER_NAME_CHECKED(FWidgetConnectionConfig, Widget)) : nullptr;
		const bool bHandlesAreValid = ParentMap && ParentStruct && ChildWidget
			&& ParentMap->IsValidHandle() && ParentStruct->IsValidHandle() && ChildWidget->IsValidHandle()
			&& ensure(ParentMap->AsMap()); 
		return bHandlesAreValid;
	}

	bool FStateSwitcherSearch::InitTargetSettingsMapping()
	{
		// Accesses the TMap data - yes, it is sketchy but that is the intended design 
		TArray<const void*> RawOwnerMapData;
		ParentMap->AccessRawData(RawOwnerMapData);
		check(!RawOwnerMapData.IsEmpty());
		ConnectionTargets = static_cast<const TMap<FName, FVCamConnectionTargetSettings>*>(RawOwnerMapData[0]);

		const bool bAccessedMapEntryValue = ConnectionTargetSettingsStructHandle->GetValueData(TargetSettingsAddress) == FPropertyAccess::Success;
		return bAccessedMapEntryValue;
	}
	
	bool FStateSwitcherSearch::FindReferencedVCamWidget()
	{
		// The selected object is expected to be the CDO ("Graph" UMG tab) or instance ("Designer" UMG tab)
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyUtils.GetSelectedObjects();
		UUserWidget* OwningWidget = SelectedObjects.Num() == 1 ? Cast<UUserWidget>(SelectedObjects[0].Get()) : nullptr;
		if (!OwningWidget)
		{
			return false;
		}

		void* ChildWidgetData;
		const bool bAccessedChildWidgetData = ChildWidget->GetValueData(ChildWidgetData) == FPropertyAccess::Success;
		const FVCamChildWidgetReference* ConnectionData = static_cast<FVCamChildWidgetReference*>(ChildWidgetData);
		RemappedWidget = bAccessedChildWidgetData ? ConnectionData->ResolveVCamWidget(*OwningWidget) : nullptr;
		return RemappedWidget != nullptr;
	}
	
	bool FStateSwitcherSearch::FindOverrideConnectionNameThenLookupWidgetConnections()
	{
		for (const TPair<FName, FVCamConnectionTargetSettings>& MapEntry : *ConnectionTargets)
		{
			const bool bFoundMapEntry = &MapEntry.Value == TargetSettingsAddress;
			if (!bFoundMapEntry)
			{
				continue;
			}

			const FName& ConnectionPoint = MapEntry.Key;
			const FVCamConnection* Connection = RemappedWidget->Connections.Find(ConnectionPoint);
			if (Connection)
			{
				ProcessWithContext(*Connection);
				return true;
			}
			return false;
		}
		
		return false;
	}
}
