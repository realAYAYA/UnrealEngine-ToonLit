// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	// Forward definitions
	class NodeScalarConstant;
	typedef Ptr<NodeScalarConstant> NodeScalarConstantPtr;
	typedef Ptr<const NodeScalarConstant> NodeScalarConstantPtrConst;


	//! Node returning a scalar constant value.
	//! \ingroup model
	class MUTABLETOOLS_API NodeScalarConstant : public NodeScalar
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeScalarConstant();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeScalarConstant* pNode, OutputArchive& arch );
		static NodeScalarConstantPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the value to be returned by the node.
		float GetValue() const;

		//! Set the value to be returned by the node.
		void SetValue( float v );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeScalarConstant();

	private:

		Private* m_pD;

	};



}
