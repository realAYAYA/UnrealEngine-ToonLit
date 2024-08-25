// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalHashedVertexDescriptor.cpp: Metal RHI Hashed Vertex Descriptor.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "MetalHashedVertexDescriptor.h"


//------------------------------------------------------------------------------

#pragma mark - Metal Hashed Vertex Descriptor


FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor()
	: VertexDescHash(0)
{
	// void
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor(MTLVertexDescriptorPtr Desc, uint32 Hash)
	: VertexDescHash(Hash)
	, VertexDesc(Desc)
{
	// void
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor(FMetalHashedVertexDescriptor const& Other)
	: VertexDescHash(0)
{
	operator=(Other);
}

FMetalHashedVertexDescriptor::~FMetalHashedVertexDescriptor()
{
	// void
}

FMetalHashedVertexDescriptor& FMetalHashedVertexDescriptor::operator=(FMetalHashedVertexDescriptor const& Other)
{
	if (this != &Other)
	{
		VertexDescHash = Other.VertexDescHash;
		VertexDesc = Other.VertexDesc;
	}
	return *this;
}

bool FMetalHashedVertexDescriptor::operator==(FMetalHashedVertexDescriptor const& Other) const
{
	bool bEqual = false;

	if (this != &Other)
	{
		if (VertexDescHash == Other.VertexDescHash)
		{
			bEqual = true;
			if (VertexDesc != Other.VertexDesc)
			{
                MTL::VertexBufferLayoutDescriptorArray* Layouts = VertexDesc->layouts();
				MTL::VertexAttributeDescriptorArray* Attributes = VertexDesc->attributes();

				MTL::VertexBufferLayoutDescriptorArray* OtherLayouts = Other.VertexDesc->layouts();
				MTL::VertexAttributeDescriptorArray* OtherAttributes = Other.VertexDesc->attributes();
				check(Layouts && Attributes && OtherLayouts && OtherAttributes);

				for (uint32 i = 0; bEqual && i < MaxVertexElementCount; i++)
				{
					MTL::VertexBufferLayoutDescriptor* LayoutDesc = Layouts->object(i);
                    MTL::VertexBufferLayoutDescriptor* OtherLayoutDesc = OtherLayouts->object(i);

					bEqual &= ((LayoutDesc != nullptr) == (OtherLayoutDesc != nullptr));

					if (LayoutDesc && OtherLayoutDesc)
					{
						bEqual &= (LayoutDesc->stride() == OtherLayoutDesc->stride());
						bEqual &= (LayoutDesc->stepFunction() == OtherLayoutDesc->stepFunction());
						bEqual &= (LayoutDesc->stepRate() == OtherLayoutDesc->stepRate());
					}

					MTL::VertexAttributeDescriptor* AttrDesc = Attributes->object(i);
                    MTL::VertexAttributeDescriptor* OtherAttrDesc = OtherAttributes->object(i);

					bEqual &= ((AttrDesc != nullptr) == (OtherAttrDesc != nullptr));

					if (AttrDesc && OtherAttrDesc)
					{
						bEqual &= (AttrDesc->format() == OtherAttrDesc->format());
						bEqual &= (AttrDesc->offset() == OtherAttrDesc->offset());
						bEqual &= (AttrDesc->bufferIndex() == OtherAttrDesc->bufferIndex());
					}
				}
			}
		}
	}
	else
	{
		bEqual = true;
	}

	return bEqual;
}
