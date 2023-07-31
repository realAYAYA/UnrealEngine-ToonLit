// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.Utils;
using Rhino.DocObjects;
using Rhino.Geometry;
using System;
using System.Collections.Generic;
using System.Linq;

namespace DatasmithRhino.ExportContext
{
	public class DatasmithActorInfo : DatasmithInfoBase
	{
		public override DirectLinkSynchronizationStatus DirectLinkStatus
		{
			get
			{
				DirectLinkSynchronizationStatus Status = base.DirectLinkStatus;

				if (DefinitionNode != null)
				{
					DirectLinkSynchronizationStatus DefinitionStatus = DefinitionNode.DirectLinkStatus;

					// Use the Definition status when the instanced actor is synced.
					// Deleting a definition also takes priority on the instance status.
					if (Status == DirectLinkSynchronizationStatus.Synced || DefinitionStatus == DirectLinkSynchronizationStatus.PendingDeletion)
					{
						return DefinitionStatus;
					}
				}
				return Status;
			}
		}

		public FDatasmithFacadeActor DatasmithActor { get { return ExportedElement as FDatasmithFacadeActor; } }

		public virtual Guid RhinoObjectId
		{
			get
			{
				if (RhinoCommonObject is ModelComponent RhinoModelComponent)
				{
					return RhinoModelComponent.Id;
				}
				else if (RhinoCommonObject is ViewportInfo RhinoViewportInfo)
				{
					return RhinoViewportInfo.Id;
				}
				return Guid.Empty;
			}
		}

		public Transform WorldTransform { get; private set; } = Transform.Identity;
		public int MaterialIndex { get; set; } = -1;
		public bool bOverrideMaterial { get; private set; } = false;

		public bool bIsRoot { get; private set; } = true;
		public DatasmithActorInfo Parent { get; private set; } = null;
		private LinkedList<DatasmithActorInfo> ChildrenInternal = new LinkedList<DatasmithActorInfo>();
		public ICollection<DatasmithActorInfo> Children { get => ChildrenInternal; }

		public bool bIsInstanceDefinition { get; private set; } = false;
		public DatasmithActorInfo DefinitionNode { get; private set; } = null;
		private LinkedList<DatasmithActorInfo> InstanceNodesInternal = new LinkedList<DatasmithActorInfo>();
		public ICollection<DatasmithActorInfo> InstanceNodes { get => InstanceNodesInternal; }

		public HashSet<int> LayerIndices { get; private set; } = new HashSet<int>();
		/// <summary>
		/// Used to determine variation occurring in the Layer hierarchy for that node.
		/// </summary>
		private List<int> RelativeLayerIndices = new List<int>();
		public Layer VisibilityLayer { get; private set; } = null;

		public bool bIsVisible
		{
			get
			{
				bool bVisibleLocally = true;
				if (RhinoCommonObject is Layer RhinoLayer)
				{
					// This recursion ensure that only layer actors with an actual exported object under them are considered visible.
					// ie. Layers containing no mesh are not visible.
					return RhinoLayer.IsVisible
						&& ChildrenInternal.Any(Child => Child.bIsVisible);
				}
				else if (RhinoCommonObject is RhinoObject CurrentRhinoObject)
				{
					bVisibleLocally = CurrentRhinoObject.Visible;
				}
				else if (bIsRoot)
				{
					return true;
				}

				return bVisibleLocally
					&& (VisibilityLayer == null || VisibilityLayer.IsVisible)
					&& (DefinitionNode == null || DefinitionNode.bIsVisible);
			}
		}

		public DatasmithActorInfo(Transform NodeTransform, string InName, string InUniqueLabel, string InBaseLabel)
			: base(null, InName, InUniqueLabel, InBaseLabel)
		{
			WorldTransform = NodeTransform;
		}

		public DatasmithActorInfo(ModelComponent InModelComponent, string InName, string InUniqueLabel, string InBaseLabel, int InMaterialIndex, bool bInOverrideMaterial, Layer InVisibilityLayer, IEnumerable<int> InLayerIndices = null)
			: base(InModelComponent, InName, InUniqueLabel, InBaseLabel)
		{
			bIsInstanceDefinition = RhinoCommonObject is InstanceDefinition;
			MaterialIndex = InMaterialIndex;
			bOverrideMaterial = bInOverrideMaterial;
			VisibilityLayer = InVisibilityLayer;

			if (InLayerIndices != null)
			{
				LayerIndices.UnionWith(InLayerIndices);
				RelativeLayerIndices.AddRange(InLayerIndices);
			}

			WorldTransform = DatasmithRhinoUtilities.GetCommonObjectTransform(InModelComponent);
		}

		protected DatasmithActorInfo(Rhino.Runtime.CommonObject InModelComponent, string InName, string InUniqueLabel, string InBaseLabel)
			: base(InModelComponent, InName, InUniqueLabel, InBaseLabel)
		{ }

		public override void ApplyDiffs(DatasmithInfoBase OtherInfo)
		{
			base.ApplyDiffs(OtherInfo);

			if (OtherInfo is DatasmithActorInfo OtherActorInfo)
			{
				//Rhino "replaces" object instead of modifying them, we must update the our reference to the object.
				RhinoCommonObject = OtherActorInfo.RhinoCommonObject;

				MaterialIndex = OtherActorInfo.MaterialIndex;
				bOverrideMaterial = OtherActorInfo.bOverrideMaterial;
				RelativeLayerIndices = OtherActorInfo.RelativeLayerIndices;
				LayerIndices = OtherActorInfo.LayerIndices;
				VisibilityLayer = OtherActorInfo.VisibilityLayer;

				MaterialIndex = OtherActorInfo.MaterialIndex;
				bOverrideMaterial = OtherActorInfo.bOverrideMaterial;

				//Update world transform, if it has changed we need to modify transform of children as well.
				Transform NewWorldTransform = Parent != null
					? Transform.Multiply(Parent.WorldTransform, OtherActorInfo.WorldTransform)
					: OtherActorInfo.WorldTransform;
				if (NewWorldTransform != WorldTransform)
				{
					Transform InverseTransform;
					if (WorldTransform.TryGetInverse(out InverseTransform) || InverseTransform.IsValid)
					{
						Transform DiffTransform = NewWorldTransform * InverseTransform;
						ApplyTransform(DiffTransform);
					}
					else
					{
						WorldTransform = NewWorldTransform;
						foreach (DatasmithActorInfo Descendant in GetDescendantEnumerator())
						{
							Descendant.ApplyModifiedStatus();
							Descendant.WorldTransform = Transform.Multiply(Descendant.Parent.WorldTransform, DatasmithRhinoUtilities.GetCommonObjectTransform(Descendant.RhinoCommonObject));
						}
					}
				}
			}
			else
			{
				System.Diagnostics.Debug.Fail("OtherInfo does not derive from DatasmithActorInfo");
			}
		}

		public void UpdateRhinoObject(Rhino.Runtime.CommonObject InCommonObject)
		{
			// We are not flagging the info as modified, this will be done by other operations.
			// This is to allow the case to update a reference to an immuable object.
			RhinoCommonObject = InCommonObject;
		}

		public List<string> GetTags(DatasmithRhinoExportContext ExportContext)
		{
			List<string> Tags = new List<string>();

			if (RhinoCommonObject != null)
			{
				Tags.Add(string.Format("Rhino.ID: {0}", RhinoObjectId));
				string ComponentTypeString = DatasmithRhinoUniqueNameGenerator.GetDefaultTypeName(RhinoCommonObject);
				Tags.Add(string.Format("Rhino.Entity.Type: {0}", ComponentTypeString));

				//Add the groups this object belongs to.
				RhinoObject InRhinoObject = RhinoCommonObject as RhinoObject;
				if (InRhinoObject != null && InRhinoObject.GroupCount > 0)
				{
					int[] GroupIndices = InRhinoObject.GetGroupList();
					for (int GroupArrayIndex = 0; GroupArrayIndex < GroupIndices.Length; ++GroupArrayIndex)
					{
						string GroupName = ExportContext.GroupIndexToName[GroupIndices[GroupArrayIndex]];
						if (GroupName != null)
						{
							Tags.Add(GroupName);
						}
					}
				}
			}

			return Tags;
		}

		private void SetParent(DatasmithActorInfo InParent)
		{
			if (Parent != null)
			{
				// Reset the absolute transform and layer to their relative values.
				WorldTransform = DatasmithRhinoUtilities.GetCommonObjectTransform(RhinoCommonObject);
				LayerIndices = new HashSet<int>(RelativeLayerIndices);
			}

			bIsRoot = false;
			Parent = InParent;

			if (InParent != null)
			{
				bIsInstanceDefinition = InParent.bIsInstanceDefinition;
				WorldTransform = Transform.Multiply(InParent.WorldTransform, WorldTransform);
				LayerIndices.UnionWith(InParent.LayerIndices);
			}
		}

		public void ApplyTransform(Transform InTransform)
		{
			foreach (DatasmithActorInfo ActorInfo in GetEnumerator())
			{
				ActorInfo.ApplyModifiedStatus();
				ActorInfo.WorldTransform = InTransform * ActorInfo.WorldTransform;
			}
		}

		public void SetDefinitionNode(DatasmithActorInfo InDefinitionNode)
		{
			System.Diagnostics.Debug.Assert(InDefinitionNode.bIsInstanceDefinition, "Trying to create an instance from a node belonging in the root tree");
			DefinitionNode = InDefinitionNode;
			InDefinitionNode.InstanceNodesInternal.AddLast(this);
			LayerIndices.UnionWith(InDefinitionNode.LayerIndices);
		}

		public void AddChild(DatasmithActorInfo ChildHierarchyNodeInfo)
		{
			ChildHierarchyNodeInfo.SetParent(this);
			ChildrenInternal.AddLast(ChildHierarchyNodeInfo);
		}

		/// <summary>
		/// Returns the number of hierarchical descendants this nodes has.
		/// </summary>
		/// <returns></returns>
		public int GetDescendantsCount()
		{
			int DescendantCount = ChildrenInternal.Count;

			foreach (DatasmithActorInfo CurrentChild in ChildrenInternal)
			{
				DescendantCount += CurrentChild.GetDescendantsCount();
			}

			return DescendantCount;
		}

		public void Reparent(DatasmithActorInfo NewParent)
		{
			Parent.ChildrenInternal.Remove(this);
			NewParent.AddChild(this);
		}

		/// <summary>
		/// Remove all actor infos with the Deleted DirectLinkStatus from the hierarchy.
		/// </summary>
		public void PurgeDeleted()
		{
			LinkedListNode<DatasmithActorInfo> CurrentNode = ChildrenInternal.First;
			while (CurrentNode != null)
			{
				if (CurrentNode.Value.DirectLinkStatus != DirectLinkSynchronizationStatus.Deleted)
				{
					CurrentNode.Value.PurgeDeleted();
					CurrentNode = CurrentNode.Next;
				}
				else
				{
					LinkedListNode<DatasmithActorInfo> NodeToDelete = CurrentNode;
					CurrentNode = CurrentNode.Next;
					ChildrenInternal.Remove(NodeToDelete);
				}
			}

			CurrentNode = InstanceNodesInternal.First;
			while (CurrentNode != null)
			{
				if (CurrentNode.Value.DirectLinkStatus == DirectLinkSynchronizationStatus.Deleted)
				{
					LinkedListNode<DatasmithActorInfo> NodeToDelete = CurrentNode;
					CurrentNode = CurrentNode.Next;
					InstanceNodesInternal.Remove(NodeToDelete);
				}
				else
				{
					CurrentNode = CurrentNode.Next;
				}
			}
		}

		/// <summary>
		/// Custom enumerator implementation returning this Actor and all its descendant.
		/// </summary>
		/// <returns></returns>
		public IEnumerable<DatasmithActorInfo> GetEnumerator()
		{
			yield return this;

			foreach (var Child in ChildrenInternal)
			{
				foreach (var ChildEnumValue in Child.GetEnumerator())
				{
					yield return ChildEnumValue;
				}
			}
		}

		/// <summary>
		/// Custom enumerator implementation for returning all descendants of this Actor.
		/// </summary>
		/// <returns></returns>
		public IEnumerable<DatasmithActorInfo> GetDescendantEnumerator()
		{
			foreach (var Child in ChildrenInternal)
			{
				foreach (var ChildEnumValue in Child.GetEnumerator())
				{
					yield return ChildEnumValue;
				}
			}
		}
	}

}