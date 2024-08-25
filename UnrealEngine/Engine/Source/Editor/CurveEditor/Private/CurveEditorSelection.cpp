// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveEditorSelection.h"

#include "Algo/BinarySearch.h"
#include "CurveEditor.h"
#include "CurveEditorTypes.h"
#include "CurveModel.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/Tuple.h"
#include "Templates/UniquePtr.h"

FCurveEditorSelection::FCurveEditorSelection()
{
	SerialNumber = 0;
}

FCurveEditorSelection::FCurveEditorSelection(TWeakPtr<FCurveEditor> InWeakCurveEditor)
{
	WeakCurveEditor = InWeakCurveEditor;
	SerialNumber = 0;
}

void FCurveEditorSelection::Clear()
{
	CurveToSelectedKeys.Reset();
	++SerialNumber;
}

const FKeyHandleSet* FCurveEditorSelection::FindForCurve(FCurveModelID InCurveID) const
{
	return CurveToSelectedKeys.Find(InCurveID);
}

int32 FCurveEditorSelection::Count() const
{
	int32 Num = 0;
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveToSelectedKeys)
	{
		Num += Pair.Value.Num();
	}
	return Num;
}

bool FCurveEditorSelection::IsSelected(FCurvePointHandle InHandle) const
{
	const FKeyHandleSet* SelectedKeys = CurveToSelectedKeys.Find(InHandle.CurveID);
	return Contains(InHandle.CurveID, InHandle.KeyHandle, InHandle.PointType);
}

bool FCurveEditorSelection::Contains(FCurveModelID CurveID, FKeyHandle KeyHandle, ECurvePointType PointType) const
{
	const FKeyHandleSet* SelectedKeys = CurveToSelectedKeys.Find(CurveID);
	return SelectedKeys && SelectedKeys->Contains(KeyHandle, PointType);
}

void FCurveEditorSelection::Add(FCurvePointHandle InHandle)
{
	Add(InHandle.CurveID, InHandle.PointType, InHandle.KeyHandle);
}

void FCurveEditorSelection::Add(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle)
{
	Add(CurveID, PointType, TArrayView<const FKeyHandle>(&KeyHandle, 1));
}

void FCurveEditorSelection::Add(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (Keys.Num() > 0 && CurveEditor)
	{
		const TUniquePtr<FCurveModel> *CurveModel = CurveEditor->GetCurves().Find(CurveID);
		if (CurveModel && CurveModel->IsValid() && !(*CurveModel)->IsReadOnly())
		{
			FKeyHandleSet& SelectedKeys = CurveToSelectedKeys.FindOrAdd(CurveID);
			for (FKeyHandle Key : Keys)
			{
				SelectedKeys.Add(Key, PointType);
			}

		}
	}

	++SerialNumber;
}

void FCurveEditorSelection::Toggle(FCurvePointHandle InHandle)
{
	Toggle(InHandle.CurveID, InHandle.PointType, InHandle.KeyHandle);
}

void FCurveEditorSelection::Toggle(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle)
{
	Toggle(CurveID, PointType, TArrayView<const FKeyHandle>(&KeyHandle, 1));
}

void FCurveEditorSelection::Toggle(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys)
{
	if (Keys.Num() > 0)
	{
		FKeyHandleSet& SelectedKeys = CurveToSelectedKeys.FindOrAdd(CurveID);
		for (FKeyHandle Key : Keys)
		{
			SelectedKeys.Toggle(Key, PointType);
		}

		if (SelectedKeys.Num() == 0)
		{
			CurveToSelectedKeys.Remove(CurveID);
		}
	}

	++SerialNumber;
}

void FCurveEditorSelection::Remove(FCurvePointHandle InHandle)
{
	Remove(InHandle.CurveID, InHandle.PointType, InHandle.KeyHandle);
}

void FCurveEditorSelection::Remove(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle)
{
	Remove(CurveID, PointType, TArrayView<const FKeyHandle>(&KeyHandle, 1));
}

void FCurveEditorSelection::Remove(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys)
{
	if (Keys.Num() > 0)
	{
		FKeyHandleSet& SelectedKeys = CurveToSelectedKeys.FindOrAdd(CurveID);
		for (FKeyHandle Key : Keys)
		{
			SelectedKeys.Remove(Key, PointType);
		}
	}

	++SerialNumber;
}

void FCurveEditorSelection::Remove(FCurveModelID InCurveID)
{
	CurveToSelectedKeys.Remove(InCurveID);
	++SerialNumber;
}

void FKeyHandleSet::Add(FKeyHandle Handle, ECurvePointType PointType)
{
	int32 ExistingIndex = Algo::LowerBound(SortedHandles, Handle);
	if (ExistingIndex >= SortedHandles.Num() || SortedHandles[ExistingIndex] != Handle)
	{
		SortedHandles.Insert(Handle, ExistingIndex);
	}

	EnumAddFlags(HandleToPointType.FindOrAdd(Handle), PointType);
}

void FKeyHandleSet::Toggle(FKeyHandle Handle, ECurvePointType PointType)
{
	int32 ExistingIndex = Algo::LowerBound(SortedHandles, Handle);
	if (ExistingIndex < SortedHandles.Num() && SortedHandles[ExistingIndex] == Handle)
	{
		if (HandleToPointType.Contains(Handle))
		{
			EnumRemoveFlags(HandleToPointType.FindChecked(Handle), PointType);

			if (HandleToPointType.FindChecked(Handle) == ECurvePointType::None)
			{
				HandleToPointType.FindAndRemoveChecked(Handle);		
				SortedHandles.RemoveAt(ExistingIndex, 1, EAllowShrinking::No);
			}
		}
	}
	else
	{
		SortedHandles.Insert(Handle, ExistingIndex);
			
		EnumAddFlags(HandleToPointType.FindOrAdd(Handle), PointType);
	}
}

void FKeyHandleSet::Remove(FKeyHandle Handle, ECurvePointType PointType)
{
	int32 ExistingIndex = Algo::LowerBound(SortedHandles, Handle);
	if (ExistingIndex < SortedHandles.Num() && SortedHandles[ExistingIndex] == Handle)
	{
		if (HandleToPointType.Contains(Handle))
		{
			EnumRemoveFlags(HandleToPointType.FindChecked(Handle), PointType);

			if (HandleToPointType.FindChecked(Handle) == ECurvePointType::None)
			{
				HandleToPointType.FindAndRemoveChecked(Handle);		
				SortedHandles.RemoveAt(ExistingIndex, 1, EAllowShrinking::No);
			}
		}
	}
}

bool FKeyHandleSet::Contains(FKeyHandle Handle, ECurvePointType PointType) const
{
	if (Algo::BinarySearch(SortedHandles, Handle) != INDEX_NONE && HandleToPointType.Contains(Handle))
	{
		return EnumHasAnyFlags(HandleToPointType.FindChecked(Handle), PointType);
	}
	
	return false;
}