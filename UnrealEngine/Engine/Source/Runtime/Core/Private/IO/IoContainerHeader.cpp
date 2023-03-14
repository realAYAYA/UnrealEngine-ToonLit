// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoContainerHeader.h"
#include "UObject/NameBatchSerialization.h"

FArchive& operator<<(FArchive& Ar, FIoContainerHeaderPackageRedirect& Redirect)
{
	Ar << Redirect.SourcePackageId;
	Ar << Redirect.TargetPackageId;
	Ar << Redirect.SourcePackageName;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FIoContainerHeaderLocalizedPackage& LocalizedPackage)
{
	Ar << LocalizedPackage.SourcePackageId;
	Ar << LocalizedPackage.SourcePackageName;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FIoContainerHeader& ContainerHeader)
{
	uint32 Signature = FIoContainerHeader::Signature;
	Ar << Signature;
	if (Ar.IsLoading() && Signature != FIoContainerHeader::Signature)
	{
		Ar.SetError();
		return Ar;
	}
	EIoContainerHeaderVersion Version = EIoContainerHeaderVersion::Latest;
	Ar << Version;
	if (Ar.IsLoading() && Version != EIoContainerHeaderVersion::Latest)
	{
		Ar.SetError();
		return Ar;
	}
	Ar << ContainerHeader.ContainerId;
	Ar << ContainerHeader.PackageIds;
	Ar << ContainerHeader.StoreEntries;
	Ar << ContainerHeader.OptionalSegmentPackageIds;
	Ar << ContainerHeader.OptionalSegmentStoreEntries;
	if (Ar.IsLoading())
	{
		ContainerHeader.RedirectsNameMap = LoadNameBatch(Ar);
	}
	else
	{
#if ALLOW_NAME_BATCH_SAVING
		SaveNameBatch(ContainerHeader.RedirectsNameMap, Ar);
#else
		check(false);
#endif
	}
	Ar << ContainerHeader.LocalizedPackages;
	Ar << ContainerHeader.PackageRedirects;

	return Ar;
}
