// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageTransform.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageTransformPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageTransform::Private::s_type =
			FNodeType( "ImageTransform", NodeImage::GetStaticType() );

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageTransform, EType::Transform, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageTransform::GetBase() const
	{
		return m_pD->m_pBase;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetOffsetX() const
	{
		return m_pD->m_pOffsetX;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetOffsetY() const
	{
		return m_pD->m_pOffsetY;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetScaleX() const
	{
		return m_pD->m_pScaleX;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetScaleY() const
	{
		return m_pD->m_pScaleY;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetRotation() const
	{
		return m_pD->m_pRotation;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetOffsetX( NodeScalarPtr pNode )
	{
		m_pD->m_pOffsetX = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetOffsetY( NodeScalarPtr pNode )
	{
		m_pD->m_pOffsetY = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetScaleX( NodeScalarPtr pNode )
	{
		m_pD->m_pScaleX = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetScaleY( NodeScalarPtr pNode )
	{
		m_pD->m_pScaleY = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetRotation( NodeScalarPtr pNode )
	{
		m_pD->m_pRotation = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetAddressMode(EAddressMode AddressMode)
	{
		m_pD->AddressMode = AddressMode;
	}

	//---------------------------------------------------------------------------------------------
	EAddressMode NodeImageTransform::GetAddressMode() const
	{
		return m_pD->AddressMode;
	}

	//---------------------------------------------------------------------------------------------
	uint16 NodeImageTransform::GetSizeX() const
	{
		return m_pD->SizeX;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetSizeX(uint16 SizeX)
	{
		m_pD->SizeX = SizeX;
	}

	//---------------------------------------------------------------------------------------------
	uint16 NodeImageTransform::GetSizeY() const
	{
		return m_pD->SizeY;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetSizeY(uint16 SizeY)
	{
		m_pD->SizeY = SizeY;
	}

	//---------------------------------------------------------------------------------------------
	bool NodeImageTransform::GetKeepAspectRatio() const
	{
		return m_pD->bKeepAspectRatio;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetKeepAspectRatio(bool bKeepAspectRatio)
	{
		m_pD->bKeepAspectRatio = bKeepAspectRatio;
	}
}

