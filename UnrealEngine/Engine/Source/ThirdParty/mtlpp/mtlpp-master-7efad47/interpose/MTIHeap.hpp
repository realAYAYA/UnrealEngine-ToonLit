// Copyright Epic Games, Inc. All Rights Reserved.
#ifndef MTIHeap_hpp
#define MTIHeap_hpp

#include "imp_Heap.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIHeapTrace : public IMPTable<id<MTLHeap>, MTIHeapTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLHeap>, MTIHeapTrace> Super;
	
	MTIHeapTrace()
	{
	}
	
	MTIHeapTrace(id<MTLHeap> C)
	: IMPTable<id<MTLHeap>, MTIHeapTrace>(object_getClass(C))
	{
	}
	
	static id<MTLHeap> Register(id<MTLHeap> Object);
	
	static void SetLabelImpl(id, SEL, Super::SetLabelType::DefinedIMP, NSString* Ptr);
	static MTLPurgeableState SetPurgeableStateImpl(id, SEL, Super::SetPurgeableStateType::DefinedIMP, MTLPurgeableState);
	static NSUInteger MaxAvailableSizeWithAlignmentImpl(id, SEL, Super::MaxAvailableSizeWithAlignmentType::DefinedIMP, NSUInteger);
	static id<MTLBuffer> NewBufferWithLengthImpl(id, SEL, Super::NewBufferWithLengthType::DefinedIMP, NSUInteger, MTLResourceOptions);
	static id<MTLTexture> NewTextureWithDescriptorImpl(id, SEL, Super::NewTextureWithDescriptorType::DefinedIMP, MTLTextureDescriptor*);
};

MTLPP_END

#endif /* MTIHeap_hpp */
