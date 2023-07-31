// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	// Forward definitions
    class NodeScalarArithmeticOperation;
    typedef Ptr<NodeScalarArithmeticOperation> NodeScalarArithmeticOperationPtr;
    typedef Ptr<const NodeScalarArithmeticOperation> NodeScalarArithmeticOperationPtrConst;


    //! Perform an arithmetic operation between two scalars.
	//! \ingroup model
    class MUTABLETOOLS_API NodeScalarArithmeticOperation : public NodeScalar
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeScalarArithmeticOperation();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeScalarArithmeticOperation* pNode, OutputArchive& arch );
        static NodeScalarArithmeticOperationPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        int GetInputCount() const override;
        Node* GetInputNode( int i ) const override;
        void SetInputNode( int i, NodePtr pNode ) override;


		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Possible arithmetic operations
		typedef enum
		{
            AO_ADD = 0,
			AO_SUBTRACT,
			AO_MULTIPLY,
            AO_DIVIDE,
            _AO_COUNT
		} OPERATION;

        static const char* s_opTypeName[ _AO_COUNT ];

		//! Get child selection type
		OPERATION GetOperation() const;

		//! Set the child selection type
		void SetOperation(OPERATION);


		//! Get the first operand
        NodeScalarPtr GetA() const;
        void SetA(NodeScalarPtr);

		// Get the second operand
        NodeScalarPtr GetB() const;
        void SetB(NodeScalarPtr);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeScalarArithmeticOperation();

	private:

		Private* m_pD;

	};


}
