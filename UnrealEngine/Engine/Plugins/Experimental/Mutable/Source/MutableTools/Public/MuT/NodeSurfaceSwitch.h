// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeSurface.h"


namespace mu
{

	// Forward definitions
	class NodeScalar;
	typedef Ptr<NodeScalar> NodeScalarPtr;
	typedef Ptr<const NodeScalar> NodeScalarPtrConst;

	class NodeSurfaceSwitch;
	typedef Ptr<NodeSurfaceSwitch> NodeSurfaceSwitchPtr;
	typedef Ptr<const NodeSurfaceSwitch> NodeSurfaceSwitchPtrConst;


	//! This node selects an output Surface from a set of input Surfaces based on a parameter.
	//! \ingroup model
	class MUTABLETOOLS_API NodeSurfaceSwitch : public NodeSurface
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeSurfaceSwitch();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeSurfaceSwitch* pNode, OutputArchive& arch );
		static NodeSurfaceSwitchPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the node generating the parameter used to select the option.
		NodeScalarPtr GetParameter() const;
		void SetParameter( NodeScalarPtr );

		//! Set the number of option Surfaces. It will keep the currently set targets and initialise
		//! the new ones as null.
		void SetOptionCount( int32 );

		//! Get the node generating the t-th option Surface.
		NodeSurfacePtr GetOption( int32 t ) const;
		void SetOption( int32 t, NodeSurfacePtr );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeSurfaceSwitch();

	private:

		Private* m_pD;

	};


}
