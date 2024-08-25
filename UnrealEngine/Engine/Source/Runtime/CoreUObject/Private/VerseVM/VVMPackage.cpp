// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMPackage.h"
#include "Containers/AnsiString.h"
#include "Containers/UnrealString.h"
#include "Containers/Utf8String.h"
#include "UObject/Package.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VPackage);
TGlobalTrivialEmergentTypePtr<&VPackage::StaticCppClassInfo> VPackage::GlobalTrivialEmergentType;

uint32 DeadPackageNameNumber = 0;

UPackage* VPackage::CreateUPackage(FAllocationContext Context)
{
	ensure(!AssociatedUPackage);
	UPackage* Package = CreatePackage(*GetUPackageName(EPackageStage::Temp));
	Package->SetFlags(RF_Transient); // We don't want this package to be saved
	AssociatedUPackage.Set(Context, Package);
	return Package;
}

FString VPackage::GetUPackageName(EPackageStage Stage) const
{
	return FString(FUtf8String::Printf(UTF8TEXT("/Script/VerseVM%s/%s%s"),
		Stage == EPackageStage::Temp ? "_TEMP" : (Stage == EPackageStage::Dead ? "_DEAD" : ""),
		PackageName->AsCString(),
		Stage == EPackageStage::Dead ? *FAnsiString::Printf("_%d", DeadPackageNameNumber++) : ""));
}

template <typename TVisitor>
void VPackage::VisitReferencesImpl(TVisitor& Visitor)
{
	Map.Visit(Visitor, TEXT("DefinitionMap"));
	Visitor.Visit(DigestCode[(int)EDigestVariant::PublicAndEpicInternal], TEXT("PublicAndEpicInternalDigest"));
	Visitor.Visit(DigestCode[(int)EDigestVariant::PublicOnly], TEXT("PublicOnlyDigest"));
	Visitor.Visit(PackageName, TEXT("PackageName"));
	Visitor.Visit(AssociatedUPackage, TEXT("AssociatedUPackage"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
