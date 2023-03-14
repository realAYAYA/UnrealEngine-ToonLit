// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimBoneCompressionCodec.h"
#include "Animation/AnimBoneCompressionSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimBoneCompressionCodec)

UAnimBoneCompressionCodec::UAnimBoneCompressionCodec(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Description(TEXT("None"))
{
}

UAnimBoneCompressionCodec* UAnimBoneCompressionCodec::GetCodec(const FString& DDCHandle)
{
	const FString ThisHandle = GetCodecDDCHandle();
	return ThisHandle == DDCHandle ? this : nullptr;
}

FString UAnimBoneCompressionCodec::GetCodecDDCHandle() const
{
	// In the DDC, we store a handle to this codec. It must be unique within the parent settings asset
	// and all children/sibling codecs. Imagine we have a settings asset with codec A and B.
	// A sequence is compressed with it and selects codec B.
	// The settings asset is duplicated. It will have the same DDC key and the data will not re-compress.
	// When we attempt to load from the DDC, we will have a handle created with the original settings
	// asset pointing to codec B. We must be able to find this codec B in the duplicated asset as well.

	FString Handle;
	Handle.Reserve(128);

	GetFName().AppendString(Handle);

	const UObject* Obj = GetOuter();
	while (Obj != nullptr && !Obj->IsA<UAnimBoneCompressionSettings>())
	{
		Handle += TEXT(".");

		Obj->GetFName().AppendString(Handle);

		Obj = Obj->GetOuter();
	}

	return Handle;
}

#if WITH_EDITORONLY_DATA
void UAnimBoneCompressionCodec::PopulateDDCKey(const UE::Anim::Compression::FAnimDDCKeyArgs& KeyArgs, FArchive& Ar)
{
}

void UAnimBoneCompressionCodec::PopulateDDCKey(const UAnimSequenceBase& AnimSeq, FArchive& Ar)
{
}

void UAnimBoneCompressionCodec::PopulateDDCKey(FArchive& Ar)
{
}
#endif

