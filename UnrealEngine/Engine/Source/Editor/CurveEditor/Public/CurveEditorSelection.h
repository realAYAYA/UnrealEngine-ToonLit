// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "CoreTypes.h"
#include "CurveEditorTypes.h"
#include "Curves/KeyHandle.h"
#include "HAL/PlatformCrt.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

class FCurveEditor;
struct FCurvePointHandle;

/**
 * A set of key handles implemented as a sorted array for transparent passing to TArrayView<> APIs.
 * Lookup is achieved via binary search: O(log(n)).
 */
struct CURVEEDITOR_API FKeyHandleSet
{
	/**
	 * Add a new key handle to this set
	 */
	void Add(FKeyHandle Handle, ECurvePointType PointType);

	/**
	 * Remove a handle from this set if it already exists, otherwise add it to the set
	 */
	void Toggle(FKeyHandle Handle, ECurvePointType PointType);

	/**
	 * Remove a handle from this set
	 */
	void Remove(FKeyHandle Handle, ECurvePointType PointType);

	/**
	 * Check whether the specified handle exists in this set
	 */
	bool Contains(FKeyHandle Handle, ECurvePointType PointType) const;

	/**
	 * Retrieve the number of handles in this set
	 */
	FORCEINLINE int32 Num() const { return SortedHandles.Num(); }

	/**
	 * Retrieve a constant view of this set as an array
	 */
	FORCEINLINE TArrayView<const FKeyHandle> AsArray() const { return SortedHandles; }

	/**
	 *  Retrieve the point type for this handle
	 */
	ECurvePointType PointType(FKeyHandle Handle) const { return HandleToPointType.FindChecked(Handle); }

private:

	/** Sorted array of key handles */
	TArray<FKeyHandle, TInlineAllocator<1>> SortedHandles;

	/** Map of handle to point type (point, left, or right tangent) */
	TMap<FKeyHandle, ECurvePointType> HandleToPointType;
};


/**
 * Class responsible for tracking selections of keys.
 * Only one type of point selection is supported at a time (key, arrive tangent, or leave tangent)
 */
struct CURVEEDITOR_API FCurveEditorSelection
{
	/**
	 * Default constructor
	 */
	FCurveEditorSelection();

	/**
	 * Constructor which takes a reference to the curve editor, 
	 * which is used to find if a model is read only
	 */
	FCurveEditorSelection(TWeakPtr<FCurveEditor> InWeakCurveEditor);

	/**
	 * Retrieve this selection's serial number. Incremented whenever a change is made to the selection.
	 */
	FORCEINLINE uint32 GetSerialNumber() const { return SerialNumber; }

	/**
	 * Check whether the selection is empty
	 */
	FORCEINLINE bool IsEmpty() const { return CurveToSelectedKeys.Num() == 0; }

	/**
	 * Retrieve all selected key handles, organized by curve ID
	 */
	FORCEINLINE const TMap<FCurveModelID, FKeyHandleSet>& GetAll() const { return CurveToSelectedKeys; }

	/**
	 * Retrieve a set of selected key handles for the specified curve
	 */
	const FKeyHandleSet* FindForCurve(FCurveModelID InCurveID) const;

	/**
	 * Count the total number of selected keys by accumulating the number of selected keys for each curve
	 */
	int32 Count() const;

	/**
	 * Check whether the specified handle is selected
	 */
	bool IsSelected(FCurvePointHandle InHandle) const;

	/**
	 * Check whether the specified handle and curve ID is contained in this selection.
	 */
	bool Contains(FCurveModelID CurveID, FKeyHandle KeyHandle, ECurvePointType PointType) const;

public:

	/**
	 * Add a point handle to this selection, changing the selection type if necessary.
	 */
	void Add(FCurvePointHandle InHandle);

	/**
	 * Add a key handle to this selection, changing the selection type if necessary.
	 */
	void Add(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle);

	/**
	 * Add key handles to this selection, changing the selection type if necessary.
	 */
	void Add(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys);
public:

	/**
	 * Toggle the selection of the specified point handle, changing the selection type if necessary.
	 */
	void Toggle(FCurvePointHandle InHandle);

	/**
	 * Toggle the selection of the specified key handle, changing the selection type if necessary.
	 */
	void Toggle(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle);

	/**
	 * Toggle the selection of the specified key handles, changing the selection type if necessary.
	 */
	void Toggle(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys);

public:

	/**
	 * Remove the specified point handle from the selection
	 */
	void Remove(FCurvePointHandle InHandle);

	/**
	 * Remove the specified key handle from the selection
	 */
	void Remove(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle);

	/**
	 * Remove the specified key handles from the selection
	 */
	void Remove(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys);

	/**
	 * Remove all key handles associated with the specified curve ID from the selection
	 */
	void Remove(FCurveModelID InCurveID);

	/**
	 * Clear the selection entirely
	 */
	void Clear();

private:

	/** Weak reference to the curve editor to check whether keys are locked or not */
	TWeakPtr<FCurveEditor> WeakCurveEditor;

	/** A serial number that increments every time a change is made to the selection */
	uint32 SerialNumber;

	/** A map of selected handles stored by curve ID */
	TMap<FCurveModelID, FKeyHandleSet> CurveToSelectedKeys;
};