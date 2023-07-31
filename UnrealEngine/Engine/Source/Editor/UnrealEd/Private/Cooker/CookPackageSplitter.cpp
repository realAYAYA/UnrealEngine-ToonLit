// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookPackageSplitter.h"

#if WITH_EDITOR

namespace UE::Cook::Private
{

static TLinkedList<FRegisteredCookPackageSplitter*>* GRegisteredCookPackageSplitterList = nullptr;

FRegisteredCookPackageSplitter::FRegisteredCookPackageSplitter()
: GlobalListLink(this)
{
	GlobalListLink.LinkHead(GetRegisteredList());
}

FRegisteredCookPackageSplitter::~FRegisteredCookPackageSplitter()
{
	GlobalListLink.Unlink();
}

TLinkedList<FRegisteredCookPackageSplitter*>*& FRegisteredCookPackageSplitter::GetRegisteredList()
{
	return GRegisteredCookPackageSplitterList;
}

void FRegisteredCookPackageSplitter::ForEach(TFunctionRef<void(FRegisteredCookPackageSplitter*)> Func)
{
	for (TLinkedList<FRegisteredCookPackageSplitter*>::TIterator It(GetRegisteredList()); It; It.Next())
	{
		Func(*It);
	}
}

}

#endif