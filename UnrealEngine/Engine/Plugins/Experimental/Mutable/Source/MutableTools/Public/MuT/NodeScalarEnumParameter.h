// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	// Forward definitions
	class NodeImage;
    using NodeImagePtr = Ptr<NodeImage>;
    using NodeImagePtrConst = Ptr<const NodeImage>;

	class NodeScalarEnumParameter;
    using NodeScalarEnumParameterPtr = Ptr<NodeScalarEnumParameter>;
    using NodeScalarEnumParameterPtrConst = Ptr<const NodeScalarEnumParameter>;

    class NodeRange;
    using NodeRangePtr = Ptr<NodeRange>;
    using NodeRangePtrConst = Ptr<const NodeRange>;


	//! Node that defines a scalar model parameter to be selected from a set of named values.
	//! \ingroup model
	class MUTABLETOOLS_API NodeScalarEnumParameter : public NodeScalar
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeScalarEnumParameter();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeScalarEnumParameter* pNode, OutputArchive& arch );
		static NodeScalarEnumParameterPtr StaticUnserialise( InputArchive& arch );


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

		//! Get the name of the parameter. It will be exposed in the final compiled data.
		const char* GetName() const;
		void SetName( const char* );

		//! Get the uid of the parameter. It will be exposed in the final compiled data.
		const char* GetUid() const;
		void SetUid( const char* );

		//! Get the index of the default value of the parameter.
		int GetDefaultValueIndex() const;
		void SetDefaultValueIndex( int v );

		//! Set the number of possible values.
		void SetValueCount( int i );

		//! Get the number of possible values.
		int GetValueCount() const;

		//! Set the data of one of the possible values of the parameter.
		void SetValue( int i, float value, const char* strName );

        //! Set the number of ranges (dimensions) for this parameter.
        //! By default a parameter has 0 ranges, meaning it only has one value.
        void SetRangeCount( int i );
        void SetRange( int i, NodeRangePtr pRange );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeScalarEnumParameter();

	private:

		Private* m_pD;

	};


}
