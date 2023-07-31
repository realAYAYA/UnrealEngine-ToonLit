// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
    class NodeMeshClipMorphPlane;
    typedef Ptr<NodeMeshClipMorphPlane> NodeMeshClipMorphPlanePtr;
    typedef Ptr<const NodeMeshClipMorphPlane> NodeMeshClipMorphPlaneConst;


    //! This node applies a geometric transform represented by a 4x4 matrix to a mesh
	//! \ingroup model
    class MUTABLETOOLS_API NodeMeshClipMorphPlane : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshClipMorphPlane();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshClipMorphPlane* pNode, OutputArchive& arch );
        static NodeMeshClipMorphPlanePtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		virtual const NODE_TYPE* GetType() const;
		static const NODE_TYPE* GetStaticType();

		virtual int GetInputCount() const;
		virtual const char* GetInputName( int i ) const;
		virtual const NODE_TYPE* GetInputType( int i ) const;
		virtual Node* GetInputNode( int i ) const;
		virtual void SetInputNode( int i, NodePtr pNode );

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Source mesh to be clipped and morphed
        NodeMeshPtr GetSource() const;
		void SetSource( NodeMesh* );

        //! \param 
		void SetPlane(float centerX, float centerY, float centerZ, float normalX, float normalY, float normalZ);
		void SetParams(float dist, float factor);
		void SetMorphEllipse(float radius1, float radius2, float rotation);

		//! Define an axis-aligned box that will select the vertices to be morphed.
		//! If this is not set, or the size of the box is 0, all vertices will be affected.
		//! Only one of Box or Bone Hierarchy can be used (the last one set)
		void SetVertexSelectionBox(float centerX, float centerY, float centerZ, float radiusX, float radiusY, float radiusZ);

		//! Define the root bone of the subhierarchy of the mesh that will be affected.
		//! Only one of Box or Bone Hierarchy can be used (the last one set)
		void SetVertexSelectionBone(const char* strBoneName, float maxEffectRadius);

		//! Add a tag to the clip morph operation, which will only affect surfaces with the same tag
		void AddTag(const char* tagName);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
		Node::Private* GetBasePrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeMeshClipMorphPlane();

	private:

		Private* m_pD;

	};


}
