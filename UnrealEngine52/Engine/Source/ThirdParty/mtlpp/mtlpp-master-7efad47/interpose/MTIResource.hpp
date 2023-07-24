// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIResource_hpp
#define MTIResource_hpp

#include "imp_Resource.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIResourceTrace
{
	typedef IMPTableResource<id<MTLResource>> Super;
	
	static void SetLabelImpl(id, SEL, Super::SetLabelType::DefinedIMP, NSString* Ptr);
	static MTLPurgeableState SetPurgeableStateImpl(id, SEL, Super::SetPurgeableStateType::DefinedIMP, MTLPurgeableState);
	static void MakeAliasableImpl(id, SEL, Super::MakeAliasableType::DefinedIMP);
};

MTLPP_END

#endif /* MTIResource_hpp */
