// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PersonaAssetEditorToolkit.h"
#include "IHasPersonaToolkit.h"
#include "Animation/SmartName.h"

class UAnimationAsset;
class IAnimationSequenceBrowser;
class UAnimSequenceBase;
struct FRichCurve;
class ITimeSliderController;
enum class ERawCurveTrackTypes : uint8;

class IAnimationEditor : public FPersonaAssetEditorToolkit, public IHasPersonaToolkit
{
public:
	/** Set the animation asset of the editor. */
	virtual void SetAnimationAsset(UAnimationAsset* AnimAsset) = 0;

	/** Get the asset browser we host */
	virtual IAnimationSequenceBrowser* GetAssetBrowser() const = 0;

	/** Support structure for EditCurves */
	struct FCurveEditInfo
	{
		FCurveEditInfo(const FText& InCurveDisplayName, const FLinearColor& InCurveColor, const FSmartName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex, FSimpleDelegate OnCurveModified = FSimpleDelegate())
			: CurveDisplayName(InCurveDisplayName)
			, CurveColor(InCurveColor)
			, Name(InName)
			, Type(InType)
			, CurveIndex(InCurveIndex)
			, OnCurveModified(OnCurveModified)
		{}

		FCurveEditInfo(const FSmartName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex)
			: Name(InName)
			, Type(InType)
			, CurveIndex(InCurveIndex)
		{}

		bool operator==(const FCurveEditInfo& InCurveEditInfo) const
		{
			return Name.UID == InCurveEditInfo.Name.UID && Type == InCurveEditInfo.Type && CurveIndex == InCurveEditInfo.CurveIndex;
		}

		FText CurveDisplayName;
		FLinearColor CurveColor;
		FSmartName Name;
		ERawCurveTrackTypes Type;
		int32 CurveIndex;
		FSimpleDelegate OnCurveModified;
	};

	/** Edit the specified curves on the specified sequence */
	virtual void EditCurves(UAnimSequenceBase* InAnimSequence, const TArray<FCurveEditInfo>& InCurveInfo, const TSharedPtr<ITimeSliderController>& InExternalTimeSliderController) = 0;

	/** Stop editing the specified curves */
	virtual void StopEditingCurves(const TArray<FCurveEditInfo>& InCurveInfo) = 0;
};
