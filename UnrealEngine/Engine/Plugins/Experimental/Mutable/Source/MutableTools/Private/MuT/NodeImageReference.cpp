// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageReference.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Serialisation.h"
#include "MuT/NodeImageReferencePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageReference::Private::s_type = NODE_TYPE( "ImageReference", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	MUTABLE_IMPLEMENT_NODE( NodeImageReference, EType::Reference, Node, Node::EType::Image);


	//---------------------------------------------------------------------------------------------
	int NodeImageReference::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeImageReference::GetInputNode( int ) const
	{
		check( false );
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageReference::SetInputNode( int, NodePtr )
	{
		check( false );
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageReference::SetImageReference( uint32 ID)
	{
		m_pD->ImageReferenceID = ID;
	}


}
