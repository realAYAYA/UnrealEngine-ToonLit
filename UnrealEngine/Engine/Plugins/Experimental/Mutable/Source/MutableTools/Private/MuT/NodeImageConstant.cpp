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
	NODE_TYPE NodeImageConstant::Private::s_type =
			NODE_TYPE( "ImageConstant", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	MUTABLE_IMPLEMENT_NODE( NodeImageConstant, EType::Constant, Node, Node::EType::Image);


	//---------------------------------------------------------------------------------------------
	int NodeImageConstant::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeImageConstant::GetInputNode( int ) const
	{
		check( false );
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageConstant::SetInputNode( int, NodePtr )
	{
		check( false );
	}


	//---------------------------------------------------------------------------------------------
    ImagePtrConst NodeImageConstant::GetValue() const
	{
        Ptr<const Image> pImage;
        if (m_pD->m_pProxy) pImage = m_pD->m_pProxy->Get();
        return pImage;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageConstant::SetValue( Ptr<const Image> pValue )
	{
        m_pD->m_pProxy = new ResourceProxyMemory<Image>( pValue );
	}


    //---------------------------------------------------------------------------------------------
    void NodeImageConstant::SetValue( Ptr<ResourceProxy<Image>> pImage )
    {
        m_pD->m_pProxy = pImage;
    }

}
