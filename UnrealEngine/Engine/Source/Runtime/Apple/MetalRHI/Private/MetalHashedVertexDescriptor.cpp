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
	, VertexDesc(nil)
{
	// void
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor(mtlpp::VertexDescriptor Desc, uint32 Hash)
	: VertexDescHash(Hash)
	, VertexDesc(Desc)
{
	// void
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor(FMetalHashedVertexDescriptor const& Other)
	: VertexDescHash(0)
	, VertexDesc(nil)
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
			if (VertexDesc.GetPtr() != Other.VertexDesc.GetPtr())
			{
				ns::Array<mtlpp::VertexBufferLayoutDescriptor> Layouts = VertexDesc.GetLayouts();
				ns::Array<mtlpp::VertexAttributeDescriptor> Attributes = VertexDesc.GetAttributes();

				ns::Array<mtlpp::VertexBufferLayoutDescriptor> OtherLayouts = Other.VertexDesc.GetLayouts();
				ns::Array<mtlpp::VertexAttributeDescriptor> OtherAttributes = Other.VertexDesc.GetAttributes();
				check(Layouts && Attributes && OtherLayouts && OtherAttributes);

				for (uint32 i = 0; bEqual && i < MaxVertexElementCount; i++)
				{
					mtlpp::VertexBufferLayoutDescriptor LayoutDesc = Layouts[(NSUInteger)i];
					mtlpp::VertexBufferLayoutDescriptor OtherLayoutDesc = OtherLayouts[(NSUInteger)i];

					bEqual &= ((LayoutDesc != nil) == (OtherLayoutDesc != nil));

					if (LayoutDesc && OtherLayoutDesc)
					{
						bEqual &= (LayoutDesc.GetStride() == OtherLayoutDesc.GetStride());
						bEqual &= (LayoutDesc.GetStepFunction() == OtherLayoutDesc.GetStepFunction());
						bEqual &= (LayoutDesc.GetStepRate() == OtherLayoutDesc.GetStepRate());
					}

					mtlpp::VertexAttributeDescriptor AttrDesc = Attributes[(NSUInteger)i];
					mtlpp::VertexAttributeDescriptor OtherAttrDesc = OtherAttributes[(NSUInteger)i];

					bEqual &= ((AttrDesc != nil) == (OtherAttrDesc != nil));

					if (AttrDesc && OtherAttrDesc)
					{
						bEqual &= (AttrDesc.GetFormat() == OtherAttrDesc.GetFormat());
						bEqual &= (AttrDesc.GetOffset() == OtherAttrDesc.GetOffset());
						bEqual &= (AttrDesc.GetBufferIndex() == OtherAttrDesc.GetBufferIndex());
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
