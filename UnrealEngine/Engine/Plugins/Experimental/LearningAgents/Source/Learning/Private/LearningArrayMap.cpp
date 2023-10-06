// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningArrayMap.h"

namespace UE::Learning
{
	FArrayMap::FArrayMap() {}
	FArrayMap::~FArrayMap() { Empty(); }

	void FArrayMap::Empty()
	{
		Handles.Empty();

		const int32 VariableNum = Arrays.Num();

		for (int32 VariableIdx = 0; VariableIdx < VariableNum; VariableIdx++)
		{
			if (Flags[VariableIdx] & FlagConstructed)
			{
				Destructors[VariableIdx](
					Arrays[VariableIdx].Data,
					DimNums[VariableIdx],
					Arrays[VariableIdx].Shape);
			}

			if (Flags[VariableIdx] & FlagAllocated)
			{
				FMemory::Free(Arrays[VariableIdx].Data);
			}
		}

		Arrays.Empty();
		Destructors.Empty();
		DimNums.Empty();
		Flags.Empty();
		TypeIds.Empty();
		TypeNames.Empty();
	}

	FArrayMapHandle FArrayMap::Lookup(const FArrayMapKey Key) const
	{
		return Handles[Key];
	}

	const FArrayMapHandle* FArrayMap::Find(const FArrayMapKey Key) const
	{
		return Handles.Find(Key);
	}

	int16 FArrayMap::TypeId(const FArrayMapKey Key) const
	{
		return TypeIds[Lookup(Key).Index];
	}

	int16 FArrayMap::TypeId(const FArrayMapHandle Variable) const
	{
		return TypeIds[Variable.Index];
	}

	const TCHAR* FArrayMap::TypeName(const FArrayMapKey Key) const
	{
		return TypeNames[Lookup(Key).Index];
	}

	const TCHAR* FArrayMap::TypeName(const FArrayMapHandle Variable) const
	{
		return TypeNames[Variable.Index];
	}

	uint8 FArrayMap::DimNum(const FArrayMapKey Key) const
	{
		return DimNums[Lookup(Key).Index];
	}

	uint8 FArrayMap::DimNum(const FArrayMapHandle Handle) const
	{
		return DimNums[Handle.Index];
	}

	bool FArrayMap::Contains(const FArrayMapKey Key) const
	{
		return Handles.Contains(Key);
	}

	void FArrayMap::LinkKeys(const FArrayMapKey Src, const FArrayMapKey Dst)
	{
		const FArrayMapHandle* SrcVariable = Handles.Find(Src);
		const FArrayMapHandle* DstVariable = Handles.Find(Dst);

		UE_LEARNING_CHECKF(SrcVariable, TEXT("Does not contain an array called { \"%s\", \"%s\" }"),
			*Src.Namespace.ToString(), *Src.Variable.ToString());

		UE_LEARNING_CHECKF(DstVariable, TEXT("Does not contain an array called { \"%s\", \"%s\" }"),
			*Dst.Namespace.ToString(), *Dst.Variable.ToString());

		const int16 DstIndex = DstVariable->Index;
		const int16 SrcIndex = SrcVariable->Index;

		UE_LEARNING_CHECKF(!(Flags[DstIndex] & FlagLinked), TEXT("Destination array is already linked!"));

		UE_LEARNING_CHECKF(TypeIds[SrcIndex] == TypeIds[DstIndex],
			TEXT("Source array { \"%s\", \"%s\" } is of type %s, while destination array { \"%s\", \"%s\" } is of type %s"),
			*Src.Namespace.ToString(), *Src.Variable.ToString(), TypeNames[SrcIndex],
			*Dst.Namespace.ToString(), *Dst.Variable.ToString(), TypeNames[DstIndex]);

		UE_LEARNING_CHECKF(DimNums[SrcIndex] == DimNums[DstIndex],
			TEXT("Source array { \"%s\", \"%s\" } has %i dimensions, while destination array { \"%s\", \"%s\" } has %i dimensions"),
			*Src.Namespace.ToString(), *Src.Variable.ToString(), DimNums[SrcIndex],
			*Dst.Namespace.ToString(), *Dst.Variable.ToString(), DimNums[DstIndex]);

		const uint8 DimNum = DimNums[SrcIndex];

		for (uint8 DimIdx = 0; DimIdx < DimNum; DimIdx++)
		{
			UE_LEARNING_CHECKF(Arrays[SrcIndex].Shape[DimIdx] == Arrays[DstIndex].Shape[DimIdx],
				TEXT("Array Shapes don't match on dimension %i of %i ({ \"%s\", \"%s\" }: %i, { \"%s\", \"%s\" }: %i)"),
				DimIdx + 1, DimNum,
				*Src.Namespace.ToString(), *Src.Variable.ToString(), Arrays[SrcIndex].Shape[DimIdx],
				*Dst.Namespace.ToString(), *Dst.Variable.ToString(), Arrays[DstIndex].Shape[DimIdx]);
		}

		// Free the Destination array data
		if (Flags[DstIndex] & FlagConstructed)
		{
			Destructors[DstIndex](
				Arrays[DstIndex].Data,
				DimNums[DstIndex],
				Arrays[DstIndex].Shape);
		}

		if (Flags[DstIndex] & FlagAllocated)
		{
			FMemory::Free(Arrays[DstIndex].Data);
		}

		// Re-direct existing Links
		for (int16 ArrayIdx = 0; ArrayIdx < Arrays.Num(); ArrayIdx++)
		{
			if (Flags[ArrayIdx] == FlagLinked && Arrays[ArrayIdx].Data == Arrays[DstIndex].Data)
			{
				Arrays[ArrayIdx].Data = Arrays[SrcIndex].Data;
			}
		}

		// Insert the new Link
		Arrays[DstIndex].Data = Arrays[SrcIndex].Data;
		Flags[DstIndex] = FlagLinked;
	}

	void FArrayMap::LinkHandles(const FArrayMapHandle Src, const FArrayMapHandle Dst)
	{
		const int16 SrcIndex = Src.Index;
		const int16 DstIndex = Dst.Index;

		UE_LEARNING_CHECKF(!(Flags[DstIndex] & FlagLinked), TEXT("Destination array is already linked!"));

		UE_LEARNING_CHECKF(TypeIds[SrcIndex] == TypeIds[DstIndex],
			TEXT("Source array is of type %s, while destination array is of type %s"),
			TypeNames[SrcIndex], TypeNames[DstIndex]);

		UE_LEARNING_CHECKF(DimNums[SrcIndex] == DimNums[DstIndex],
			TEXT("Source array has %i dimensions, while destination array has %i dimensions"),
			DimNums[SrcIndex], DimNums[DstIndex]);

		const uint8 DimNum = DimNums[SrcIndex];

		for (uint8 DimIdx = 0; DimIdx < DimNum; DimIdx++)
		{
			UE_LEARNING_CHECKF(Arrays[SrcIndex].Shape[DimIdx] == Arrays[DstIndex].Shape[DimIdx],
				TEXT("Array Shapes don't match on dimension %i of %i (%i vs %i)"),
				DimIdx + 1, DimNum, Arrays[SrcIndex].Shape[DimIdx], Arrays[DstIndex].Shape[DimIdx]);
		}

		// Free the Destination array data
		if (Flags[DstIndex] & FlagConstructed)
		{
			Destructors[DstIndex](
				Arrays[DstIndex].Data,
				DimNums[DstIndex],
				Arrays[DstIndex].Shape);
		}

		if (Flags[DstIndex] & FlagAllocated)
		{
			FMemory::Free(Arrays[DstIndex].Data);
		}

		// Re-direct existing Links
		for (int16 ArrayIdx = 0; ArrayIdx < Arrays.Num(); ArrayIdx++)
		{
			if (Flags[ArrayIdx] == FlagLinked && Arrays[ArrayIdx].Data == Arrays[DstIndex].Data)
			{
				Arrays[ArrayIdx].Data = Arrays[SrcIndex].Data;
			}
		}

		// Insert the new Link
		Arrays[DstIndex].Data = Arrays[SrcIndex].Data;
		Flags[DstIndex] = FlagLinked;
	}

	void FArrayMap::LinkHandlesFlat(const FArrayMapHandle Src, const FArrayMapHandle Dst)
	{
		const int16 SrcIndex = Src.Index;
		const int16 DstIndex = Dst.Index;

		UE_LEARNING_CHECKF(!(Flags[DstIndex] & FlagLinked), TEXT("Destination array is already linked!"));

		UE_LEARNING_CHECKF(TypeIds[SrcIndex] == TypeIds[DstIndex],
			TEXT("Source array is of type %s, while destination array is of type %s"),
			TypeNames[SrcIndex], TypeNames[DstIndex]);

		int32 SrcTotalSize = Arrays[SrcIndex].Shape[0];
		for (uint8 SrcDimIdx = 1; SrcDimIdx < DimNums[SrcIndex]; SrcDimIdx++)
		{
			SrcTotalSize *= Arrays[SrcIndex].Shape[SrcDimIdx];
		}

		int32 DstTotalSize = Arrays[DstIndex].Shape[0];
		for (uint8 DstDimIdx = 1; DstDimIdx < DimNums[DstIndex]; DstDimIdx++)
		{
			DstTotalSize *= Arrays[DstIndex].Shape[DstDimIdx];
		}

		UE_LEARNING_CHECKF(SrcTotalSize == DstTotalSize,
			TEXT("Array total sizes don't match (%i vs %i)"),
			SrcTotalSize, DstTotalSize);

		// Free the Destination array data
		if (Flags[DstIndex] & FlagConstructed)
		{
			Destructors[DstIndex](
				Arrays[DstIndex].Data,
				DimNums[DstIndex],
				Arrays[DstIndex].Shape);
		}

		if (Flags[DstIndex] & FlagAllocated)
		{
			FMemory::Free(Arrays[DstIndex].Data);
		}

		// Re-direct existing Links
		for (int16 ArrayIdx = 0; ArrayIdx < Arrays.Num(); ArrayIdx++)
		{
			if (Flags[ArrayIdx] == FlagLinked && Arrays[ArrayIdx].Data == Arrays[DstIndex].Data)
			{
				Arrays[ArrayIdx].Data = Arrays[SrcIndex].Data;
			}
		}

		// Insert the new Link
		Arrays[DstIndex].Data = Arrays[SrcIndex].Data;
		Flags[DstIndex] = FlagLinked;
	}

	bool FArrayMap::HasLink(const FArrayMapKey Dst) const
	{
		const FArrayMapHandle* DstVariable = Handles.Find(Dst);

		UE_LEARNING_CHECKF(DstVariable, TEXT("Does not contain an array called { \"%s\", \"%s\" }"),
			*Dst.Namespace.ToString(), *Dst.Variable.ToString());

		return HasLink(*DstVariable);
	}

	bool FArrayMap::HasLink(const FArrayMapHandle Dst) const
	{
		return Flags[Dst.Index] & FlagLinked;
	}

	bool FArrayMap::IsLinkedTo(const FArrayMapKey Src) const
	{
		const FArrayMapHandle* SrcVariable = Handles.Find(Src);

		UE_LEARNING_CHECKF(SrcVariable, TEXT("Does not contain an array called { \"%s\", \"%s\" }"),
			*Src.Namespace.ToString(), *Src.Variable.ToString());

		return IsLinkedTo(*SrcVariable);
	}

	bool FArrayMap::IsLinkedTo(const FArrayMapHandle Src) const
	{
		const int32 VariableNum = Arrays.Num();

		for (int32 VariableIdx = 0; VariableIdx < VariableNum; VariableIdx++)
		{
			if (Arrays[VariableIdx].Data == Arrays[Src.Index].Data && (Flags[VariableIdx] & FlagLinked))
			{
				return true;
			}
		}

		return false;
	}

	const FArrayMapKey* FArrayMap::FindKey(FArrayMapHandle Handle) const
	{
		return Handles.FindKey(Handle);
	}
}
