// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/WeakObjectPtrTemplates.h"

#include "BaseLensTable.generated.h"

class ULensFile;
enum class ELensDataCategory : uint8;

/**
 * Extra information about linked points
 */
struct FLinkPointMetadata
{
	FLinkPointMetadata() = default;

	FLinkPointMetadata(const bool bInRemoveByDefault)
		: bRemoveByDefault(bInRemoveByDefault)
	{}

	/** Whether the linked point should be set to remove by default */ 
	bool bRemoveByDefault = true;
};

/**
 * Base focus point struct
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FBaseFocusPoint
{
	GENERATED_BODY()

public:
	virtual ~FBaseFocusPoint() = default;

	/** Returns focus value for this Focus Point */
	virtual float GetFocus() const PURE_VIRTUAL(FBaseLensTable::GetFocus, return 0.f;);

	/** Returns number of zoom points */
	virtual int32 GetNumPoints() const PURE_VIRTUAL(FBaseLensTable::GetNumPoints, return 0;);

	/** Returns zoom value for a given index */
	virtual float GetZoom(int32 Index) const PURE_VIRTUAL(FBaseLensTable::GetZoom, return 0.f;);
};

template<>
struct TStructOpsTypeTraits<FBaseFocusPoint> : public TStructOpsTypeTraitsBase2<FBaseFocusPoint>
{
	enum
	{
		WithPureVirtual = true,
	};
};

/**
 * Base data table struct
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FBaseLensTable
{
	GENERATED_BODY()

	friend ULensFile;

	/** Callback to get the base focus point reference */
	using FFocusPointCallback = TFunction<void(const FBaseFocusPoint& /*InFocusPoint*/)>;

	/** Callback to get the linked focus point reference */
	using FLinkedFocusPointCallback = TFunction<void(const FBaseFocusPoint& /*InFocusPoint*/, ELensDataCategory /*Category*/, FLinkPointMetadata /* InPointMeta*/)>;
	
protected:

	/** Returns the map of linked categories  */
	virtual TMap<ELensDataCategory, FLinkPointMetadata> GetLinkedCategories() const PURE_VIRTUAL(FBaseLensTable::GetLinkedCategories, return TMap<ELensDataCategory, FLinkPointMetadata>() ;);

	/**
	 * Whether the focus point exists
	 * @param InFocus focus value to check
	 * @return true if the point exists
	 */
	virtual bool DoesFocusPointExists(float InFocus) const PURE_VIRTUAL(FBaseLensTable::DoesFocusPointExists, return false; );

	/**
	 * Whether the zoom point exists
	 * @param InFocus focus value to check 
	 * @param InZoom zoom value to check
	 * @param InputTolerance Maximum allowed difference for considering them as 'nearly equal'
	 * @return true if zoom point exists
	 */
	virtual bool DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance = KINDA_SMALL_NUMBER) const PURE_VIRTUAL(FBaseLensTable::DoesZoomPointExists, return false; );

public:
	virtual ~FBaseLensTable() = default;

	/**
	 * Loop through all Focus Points
	 * @param InCallback Callback with Focus point reference
	 */
	virtual void ForEachPoint(FFocusPointCallback InCallback) const PURE_VIRTUAL(FBaseLensTable::ForEachPoint );

	/** Get number of Focus points for this data table */
	virtual int32 GetFocusPointNum() const PURE_VIRTUAL(FBaseLensTable::GetFocusPointNum, return INDEX_NONE; );

	/** Get total number of Zoom points for all Focus points of this data table */
	virtual int32 GetTotalPointNum() const PURE_VIRTUAL(FBaseLensTable::GetTotalPointNum, return INDEX_NONE; );

	/** Get the base focus point by given index */
	virtual const FBaseFocusPoint* GetBaseFocusPoint(int32 InIndex) const PURE_VIRTUAL(FBaseLensTable::GetBaseFocusPoint, return nullptr; );

	/** Get Struct class of this Data Table */
	virtual UScriptStruct* GetScriptStruct() const PURE_VIRTUAL(FBaseLensTable::GetFocusPointNum, return nullptr; );

	/** Get Names of this Data Point */
	static FName GetFriendlyPointName(ELensDataCategory InCategory);

	/**
	 * Loop through all Focus Points base on given focus value
	 * @param InCallback Callback with Focus point reference
	 * @param InFocus focus value to check 
	 * @param InputTolerance Maximum allowed difference for considering them as 'nearly equal'
	 */
	void ForEachFocusPoint(FFocusPointCallback InCallback, const float InFocus, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/**
	 * Loop through all linked Focus Points base on given focus value
	 * @param InCallback Callback with Focus point reference, category and link meta
	 * @param InFocus focus value to check 
	 * @param InputTolerance Maximum allowed difference for considering them as 'nearly equal'
	 */
	void ForEachLinkedFocusPoint(FLinkedFocusPointCallback InCallback, const float InFocus, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/**
	 * Whether the linkage exists for given focus value
	 * @param InFocus focus value to check 
	 * @param InputTolerance Maximum allowed difference for considering them as 'nearly equal'
	 * @return true if linkage exists
	 */
	bool HasLinkedFocusValues(const float InFocus, float InputTolerance = KINDA_SMALL_NUMBER) const;

	 /**
	 * Whether the linkage exists for given focus and zoom values
	 * @param InFocus focus value to check 
	 * @param InZoomPoint zoom value to check
	 * @param InputTolerance Maximum allowed difference for considering them as 'nearly equal'
	 * @return true if linkage exists
	 */
	bool HasLinkedZoomValues(const float InFocus, const float InZoomPoint, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/**
	 * Whether given value fit between Focus Point Neighbors
	 * @param InFocusPoint given focus point
	 * @param InFocusValueToEvaluate value to evaluate between focus point neighbors
	 * @return true if value fit between neighbors
	 */
	bool IsFocusBetweenNeighbor(const float InFocusPoint, const float InFocusValueToEvaluate) const;

	/** Get the pointer to owner lens file */
	ULensFile* GetLensFile() const { return LensFile.Get(); }
	
private:
	/**
	 * Lens file owner reference
	 */
	UPROPERTY()
	TWeakObjectPtr<ULensFile> LensFile;
};

template<>
struct TStructOpsTypeTraits<FBaseLensTable> : public TStructOpsTypeTraitsBase2<FBaseLensTable>
{
	enum
	{
		WithPureVirtual = true,
	};
};
