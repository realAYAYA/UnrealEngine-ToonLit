// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModelInputInfo.h"
#include "MLDeformerMorphModel.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerMorphModelInputInfo)

void UMLDeformerMorphModelInputInfo::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);

	// Strip editor only data.
#if WITH_EDITORONLY_DATA
	if (Archive.IsSaving() && Archive.IsCooking())
	{
		InputItemMaskBuffer.Empty();
	}
#endif
}

#if WITH_EDITORONLY_DATA
	TArray<float>& UMLDeformerMorphModelInputInfo::GetInputItemMaskBuffer()
	{ 
		return InputItemMaskBuffer;
	}

	const TArray<float>& UMLDeformerMorphModelInputInfo::GetInputItemMaskBuffer() const
	{ 
		return InputItemMaskBuffer;
	}

	const TArrayView<const float> UMLDeformerMorphModelInputInfo::GetMaskForItem(int32 MaskItemIndex) const
	{
		const UMLDeformerMorphModel* MorphModel = Cast<UMLDeformerMorphModel>(GetOuter());
		check(MorphModel);

		if (InputItemMaskBuffer.IsEmpty())
		{
			return TArrayView<const float>();
		}

		const int32 NumVerts = MorphModel->GetNumBaseMeshVerts();
		check(MaskItemIndex * NumVerts + NumVerts <= InputItemMaskBuffer.Num());
		return TArrayView<const float>(&InputItemMaskBuffer[MaskItemIndex * NumVerts], NumVerts);
	}
#endif
