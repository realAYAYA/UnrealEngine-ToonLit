// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"

#include "MergeProxyUtils/Utils.h"
#include "MergeProxyUtils/SMeshProxyCommonDialog.h"

class FMeshProxyTool;
class UMeshProxySettingsObject;
class UObject;

/*-----------------------------------------------------------------------------
SMeshProxyDialog  
-----------------------------------------------------------------------------*/

class SMeshProxyDialog : public SMeshProxyCommonDialog
{
public:
	SLATE_BEGIN_ARGS(SMeshProxyDialog)
	{
	}
	SLATE_END_ARGS()

public:
	/** **/
	SMeshProxyDialog();
	~SMeshProxyDialog();

	/** SWidget functions */
	void Construct(const FArguments& InArgs, FMeshProxyTool* InTool);

private:
	/** Owning mesh merging tool */
	FMeshProxyTool* Tool;

	/** Cached pointer to mesh merging setting singleton object */
	UMeshProxySettingsObject* ProxySettings;
};



class FThirdPartyMeshProxyTool;
/*-----------------------------------------------------------------------------
	SThirdPartyMeshProxyDialog  -- Used for Simplygon (third party tool) integration.
-----------------------------------------------------------------------------*/
class SThirdPartyMeshProxyDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SThirdPartyMeshProxyDialog)
	{
	}
	SLATE_END_ARGS()

public:
	/** SWidget functions */
	void Construct(const FArguments& InArgs, FThirdPartyMeshProxyTool* InTool);

protected:
	/** ScreenSize accessors */
	TOptional<int32> GetScreenSize() const;
	void ScreenSizeChanged(int32 NewValue);		//used with editable text block (Simplygon)

	/** Recalculate Normals accessors */
	ECheckBoxState GetRecalculateNormals() const;
	void SetRecalculateNormals(ECheckBoxState NewValue);

	/** Hard Angle Threshold accessors */
	TOptional<float> GetHardAngleThreshold() const;
	bool HardAngleThresholdEnabled() const;
	void HardAngleThresholdChanged(float NewValue);

	/** Hole filling accessors */
	TOptional<int32> GetMergeDistance() const;
	void MergeDistanceChanged(int32 NewValue);

	/** TextureResolution accessors */
	void SetTextureResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void SetLightMapResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Export material properties acessors **/
	ECheckBoxState GetExportNormalMap() const;
	void SetExportNormalMap(ECheckBoxState NewValue);
	ECheckBoxState GetExportMetallicMap() const;
	void SetExportMetallicMap(ECheckBoxState NewValue);
	ECheckBoxState GetExportRoughnessMap() const;
	void SetExportRoughnessMap(ECheckBoxState NewValue);
	ECheckBoxState GetExportSpecularMap() const;
	void SetExportSpecularMap(ECheckBoxState NewValue);

private:
	/** Creates the geometry mode controls */
	void CreateLayout();

	int32 FindTextureResolutionEntryIndex(int32 InResolution) const;
	FText GetPropertyToolTipText(const FName& PropertyName) const;

private:
	FThirdPartyMeshProxyTool* Tool;

	TArray< TSharedPtr<FString> >	CuttingPlaneOptions;
	TArray< TSharedPtr<FString> >	TextureResolutionOptions;
};

