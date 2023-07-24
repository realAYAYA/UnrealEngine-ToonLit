// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{
	class InputArchive;
	class OutputArchive;
	class NodeRange;

    //! Node that defines a Image model parameter.
	//! \ingroup model
    class MUTABLETOOLS_API NodeImageParameter : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeImageParameter();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageParameter* pNode, OutputArchive& arch );
        static Ptr<NodeImageParameter> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		int GetInputCount() const override;
		Node* GetInputNode( int i ) const override;
		void SetInputNode( int i, Ptr<Node> Node ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the name of the parameter. It will be exposed in the final compiled data.
		const char* GetName() const;

		//! Set the name of the parameter.
		void SetName( const char* );

		//! Get the uid of the parameter. It will be exposed in the final compiled data.
		const char* GetUid() const;

		//! Set the uid of the parameter.
		void SetUid( const char* );

		//! Set the number of ranges (dimensions) for this parameter.
		//! By default a parameter has 0 ranges, meaning it only has one value.
		void SetRangeCount(int Index);
		void SetRange(int Index, Ptr<NodeRange> Range);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeImageParameter();

	private:

		Private* m_pD;

	};


}
