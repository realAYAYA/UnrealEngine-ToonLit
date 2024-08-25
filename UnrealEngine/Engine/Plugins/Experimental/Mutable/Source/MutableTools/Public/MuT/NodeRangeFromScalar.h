// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuT/NodeRange.h"

namespace mu
{

	// Forward definitions
    class NodeScalar;


	//! 
	//! \ingroup model
    class MUTABLETOOLS_API NodeRangeFromScalar : public NodeRange
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeRangeFromScalar();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeRangeFromScalar* pNode, OutputArchive& arch );
        static Ptr<NodeRangeFromScalar> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

        //!
        Ptr<NodeScalar> GetSize() const;
        void SetSize( const Ptr<NodeScalar>& );

        //!
		const FString& GetName() const;
		void SetName(const FString& Name);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeRangeFromScalar();

	private:

		Private* m_pD;

	};


}
