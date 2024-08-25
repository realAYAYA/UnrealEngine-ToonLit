// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Fonts/SlateFontInfo.h"
#include "UI/VCamConnectionStructs.h"

class IDetailGroup;
class UVCamWidget;
struct FVCamConnectionTargetSettings;

namespace UE::VCamCoreEditor
{
	DECLARE_DELEGATE_OneParam(FOnTargetSettingsChanged, const FVCamConnectionTargetSettings& NewSettings);
	/** Data for requesting to add connection target settings to details panel */
	struct FAddConnectionArgs
	{
		IDetailGroup& DetailGroup;

		/** Unique ID used for mapping data */
		FName ConnectionName;
		/** Contains the data to display */
		const FVCamConnection& ConnectionData;

		/** Called when the settings are changed. Copy the passed settings to your UPROPERTY(). */
		FOnTargetSettingsChanged OnTargetSettingsChangedDelegate;

		/** The font to use for displaying property texts. */
		FSlateFontInfo Font;

		FAddConnectionArgs(IDetailGroup& DetailGroup, FName ConnectionName, const FVCamConnection& ConnectionData, FOnTargetSettingsChanged OnTargetSettingsChangedDelegate, FSlateFontInfo Font)
			: DetailGroup(DetailGroup)
			, ConnectionName(ConnectionName)
			, ConnectionData(ConnectionData)
			, OnTargetSettingsChangedDelegate(MoveTemp(OnTargetSettingsChangedDelegate))
			, Font(MoveTemp(Font))
		{}
	};
	
	/** Passed to IConnectionTargetRemappingCustomizers to re-use functionality. */
	class VCAMCOREEDITOR_API IConnectionRemapUtils : public TSharedFromThis<IConnectionRemapUtils>
	{
	public:

		/** Adds a property row representing FVCamConnection::TargetSettings to DetailGroup. */
		virtual void AddConnection(FAddConnectionArgs Params) = 0;

		/** @return the font used for properties and details */
		virtual FSlateFontInfo GetRegularFont() const = 0;

		/**
		 * Refreshes the details view and regenerates all the customized layouts
		 * Use only when you need to remove or add complicated dynamic items
		 */
		virtual void ForceRefreshProperties() const = 0;
		
		virtual ~IConnectionRemapUtils() = default;
	};
}
