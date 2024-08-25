// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Serialisation.h"
#include "MuT/NodeImageConstantPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageConstant::Private::s_type =
		FNodeType("ImageConstant", NodeImage::GetStaticType());


	//---------------------------------------------------------------------------------------------
	MUTABLE_IMPLEMENT_NODE(NodeImageConstant, EType::Constant, Node, Node::EType::Image);


	//---------------------------------------------------------------------------------------------
	ImagePtrConst NodeImageConstant::GetValue() const
	{
		Ptr<const Image> pImage;
		if (m_pD->m_pProxy) pImage = m_pD->m_pProxy->Get();
		return pImage;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageConstant::SetValue(const Image* Value)
	{
		m_pD->m_pProxy = new ResourceProxyMemory<Image>(Value);
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageConstant::SetValue(Ptr<ResourceProxy<Image>> pImage)
	{
		m_pD->m_pProxy = pImage;
	}

}
