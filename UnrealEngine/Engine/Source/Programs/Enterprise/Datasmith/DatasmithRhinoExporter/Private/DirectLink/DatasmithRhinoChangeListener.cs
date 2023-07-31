// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.ExportContext;
using DatasmithRhino.Properties.Localization;

using Rhino;
using Rhino.DocObjects;
using Rhino.DocObjects.Tables;
using System;
using System.Collections.Generic;

namespace DatasmithRhino.DirectLink
{
	public class DatasmithRhinoChangeListener
	{
		private enum RhinoOngoingEventTypes
		{
			None,
			ReplacingActor,
			MovingActor,
		}

		public bool bIsListening { get => ExportContext != null; }
		private DatasmithRhinoExportContext ExportContext = null;

		/// <summary>
		/// The ModifyObjectAttributes event may fire recursively while parsing the info of a RhinoObject.
		/// We use this HashSet to avoid doing recursive parsing.
		/// </summary>
		private HashSet<Guid> RecursiveEventLocks = new HashSet<Guid>();

		/// <summary>
		/// Rhino rarely modify its object, instead it "replaces" them with a new object with the same ID.
		/// When that happens 3-4 events are fired in succession: (optional) BeforeTransformObjects, ReplaceRhinoObject, DeleteRhinoObject and AddRhinoObject (if undo is disabled UndeleteRhinoObject is called instead of AddRhinoObject).
		/// We use the event stack to determine if these events should be treated as a single "ongoing" event.
		/// </summary>
		private Stack<RhinoOngoingEventTypes> EventStack = new Stack<RhinoOngoingEventTypes>();
		
		/// <summary>
		/// Returns the current ongoing event in the event stack.
		/// If there is no ongoing event, the returned type is "None".
		/// </summary>
		private RhinoOngoingEventTypes OngoingEvent { get => EventStack.Count > 0 ? EventStack.Peek() : RhinoOngoingEventTypes.None; }

		public void StartListening(DatasmithRhinoExportContext Context)
		{
			if (Context != null)
			{
				if (!bIsListening)
				{
					RhinoDoc.BeforeTransformObjects += OnBeforeTransformObjects;
					RhinoDoc.ModifyObjectAttributes += OnModifyObjectAttributes;
					RhinoDoc.InstanceDefinitionTableEvent += OnInstanceDefinitionTableEvent;
					RhinoDoc.UndeleteRhinoObject += OnUndeleteRhinoObject;
					RhinoDoc.AddRhinoObject += OnAddRhinoObject;
					RhinoDoc.DeleteRhinoObject += OnDeleteRhinoObject;
					RhinoDoc.ReplaceRhinoObject += OnReplaceRhinoObject;
					RhinoDoc.LayerTableEvent += OnLayerTableEvent;
					RhinoDoc.GroupTableEvent += OnGroupTableEvent;
					RhinoDoc.MaterialTableEvent += OnMaterialTableEvent;
					RhinoDoc.RenderMaterialsTableEvent += OnRenderMaterialsTableEvent;
					RhinoDoc.TextureMappingEvent += OnTextureMappingEvent;
					RhinoDoc.DocumentPropertiesChanged += OnDocumentPropertiesChanged;

					//#ueent_todo Listen to the following events to update their associated DatasmithElement
					//RhinoDoc.DimensionStyleTableEvent;
					//RhinoDoc.LightTableEvent;
					//RhinoDoc.RenderEnvironmentTableEvent;
				}

				ExportContext = Context;
			}
		}

		public void StopListening()
		{
			ExportContext = null;
			EventStack.Clear();

			RhinoDoc.BeforeTransformObjects -= OnBeforeTransformObjects;
			RhinoDoc.ModifyObjectAttributes -= OnModifyObjectAttributes;
			RhinoDoc.InstanceDefinitionTableEvent -= OnInstanceDefinitionTableEvent;
			RhinoDoc.UndeleteRhinoObject -= OnUndeleteRhinoObject;
			RhinoDoc.AddRhinoObject -= OnAddRhinoObject;
			RhinoDoc.DeleteRhinoObject -= OnDeleteRhinoObject;
			RhinoDoc.ReplaceRhinoObject -= OnReplaceRhinoObject;
			RhinoDoc.LayerTableEvent -= OnLayerTableEvent;
			RhinoDoc.GroupTableEvent -= OnGroupTableEvent;
			RhinoDoc.MaterialTableEvent -= OnMaterialTableEvent;
			RhinoDoc.RenderMaterialsTableEvent -= OnRenderMaterialsTableEvent;
			RhinoDoc.TextureMappingEvent -= OnTextureMappingEvent;
			RhinoDoc.DocumentPropertiesChanged -= OnDocumentPropertiesChanged;
		}

		private void OnBeforeTransformObjects(object Sender, RhinoTransformObjectsEventArgs RhinoEventArgs)
		{
			//Copied object will call their own creation event.
			if (!RhinoEventArgs.ObjectsWillBeCopied)
			{
				if (OngoingEvent != RhinoOngoingEventTypes.None)
				{
					System.Diagnostics.Debug.Fail("Did not complete previous Object transform before starting a new one");
					EventStack.Clear();
				}

				for (int ObjectIndex = 0; ObjectIndex < RhinoEventArgs.ObjectCount; ++ObjectIndex)
				{
					RhinoObject CurrentObject = RhinoEventArgs.Objects[ObjectIndex];
					bool bIsALight = CurrentObject.ObjectType == ObjectType.Light;

					if (!bIsALight)
					{
						// All the types that are not lights have OnReplaceRhinoObject(), OnDeleteRhinoObject() and OnAddRhinoObject() events called on them after a move.
						// We consider that chain of event to be part of the same operation as the move.
						EventStack.Push(RhinoOngoingEventTypes.MovingActor);
					}

					TryCatchExecute(() => ExportContext.MoveActor(RhinoEventArgs.Objects[ObjectIndex], RhinoEventArgs.Transform));
				}
			}
		}

		private void OnModifyObjectAttributes(object Sender, RhinoModifyObjectAttributesEventArgs RhinoEventArgs)
		{
			bool bReparent = RhinoEventArgs.OldAttributes.LayerIndex != RhinoEventArgs.NewAttributes.LayerIndex;
			Guid ObjectId = RhinoEventArgs.RhinoObject.Id;

			if (RecursiveEventLocks.Add(ObjectId))
			{
				TryCatchExecute(() => ExportContext.ModifyActor(RhinoEventArgs.RhinoObject, bReparent));
				RecursiveEventLocks.Remove(ObjectId);
			}
		}

		private void OnInstanceDefinitionTableEvent(object Sender, InstanceDefinitionTableEventArgs RhinoEventArgs)
		{
			// Only the Modified event is relevant.
			// We don't care if a definition is added or modified since the same events will be called directly for the associated instances if needed.
			if (RhinoEventArgs.EventType == InstanceDefinitionTableEventType.Modified)
			{
				TryCatchExecute(() => ExportContext.UpdateDefinitionNode(RhinoEventArgs.NewState));
			}
		}

		private void OnUndeleteRhinoObject(object Sender, RhinoObjectEventArgs RhinoEventArgs)
		{
			AddActor(RhinoEventArgs.TheObject);
		}

		private void OnAddRhinoObject(object Sender, RhinoObjectEventArgs RhinoEventArgs)
		{
			AddActor(RhinoEventArgs.TheObject);
		}

		private void OnDeleteRhinoObject(object Sender, RhinoObjectEventArgs RhinoEventArgs)
		{
			// Replacing or moving an object (modifying it) involves deleting it first, then creating or "undeleting" a new object with the same ID.
			// Since with Datasmith we actually can (and want) to update the existing Elements, ignore the Deletion here.
			if (OngoingEvent == RhinoOngoingEventTypes.None)
			{
				TryCatchExecute(() => ExportContext.DeleteActor(RhinoEventArgs.TheObject));
			}
		}

		private void OnReplaceRhinoObject(object Sender, RhinoReplaceObjectEventArgs RhinoEventArgs)
		{
			if (OngoingEvent != RhinoOngoingEventTypes.MovingActor)
			{
				// Event will be completed at the end of the upcoming Delete-Add events.
				EventStack.Push(RhinoOngoingEventTypes.ReplacingActor);
			}
		}

		private void OnLayerTableEvent(object Sender, LayerTableEventArgs RhinoEventArgs)
		{
			switch (RhinoEventArgs.EventType)
			{
				case LayerTableEventType.Added:
				case LayerTableEventType.Undeleted:
					TryCatchExecute(() => ExportContext.AddActor(RhinoEventArgs.NewState));
					break;
				case LayerTableEventType.Modified:
					bool bReparent = RhinoEventArgs?.OldState.ParentLayerId != RhinoEventArgs?.NewState.ParentLayerId;
					TryCatchExecute(() => ExportContext.ModifyActor(RhinoEventArgs.NewState, bReparent));
					break;
				case LayerTableEventType.Deleted:
					TryCatchExecute(() => ExportContext.DeleteActor(RhinoEventArgs.NewState));
					break;
				case LayerTableEventType.Sorted:
				case LayerTableEventType.Current:
				default:
					break;
			}
		}

		private void OnGroupTableEvent(object Sender, GroupTableEventArgs RhinoEventArgs)
		{
			TryCatchExecute(() =>
			{
				if (RhinoEventArgs.EventType != GroupTableEventType.Sorted)
				{
					ExportContext.UpdateGroups(RhinoEventArgs.EventType, RhinoEventArgs.NewState);
				}
			});
		}

		private void OnMaterialTableEvent(object Sender, MaterialTableEventArgs RhinoEventArgs)
		{
			switch (RhinoEventArgs.EventType)
			{
				case MaterialTableEventType.Added:
				case MaterialTableEventType.Undeleted:
				case MaterialTableEventType.Modified:
					ExportContext.ModifyMaterial(RhinoEventArgs.Index);
					break;
				case MaterialTableEventType.Deleted:
					ExportContext.DeleteMaterial(RhinoEventArgs.Index);
					break;
				case MaterialTableEventType.Current:
				case MaterialTableEventType.Sorted:
				default:
					break;
			}
		}

		private void OnRenderMaterialsTableEvent(object Sender, RhinoDoc.RenderContentTableEventArgs RhinoEventArgs)
		{
			switch (RhinoEventArgs.EventType)
			{
				case RhinoDoc.RenderContentTableEventType.MaterialAssignmentChanged:
					if(RhinoEventArgs is RhinoDoc.RenderMaterialAssignmentChangedEventArgs MaterialAssignmentChangedArgs)
					{
						RhinoDoc Document = MaterialAssignmentChangedArgs.Document;
						Guid ObjectId = MaterialAssignmentChangedArgs.IsLayer
							? MaterialAssignmentChangedArgs.LayerId
							: MaterialAssignmentChangedArgs.ObjectId;

						if (ExportContext.ObjectIdToHierarchyActorNodeDictionary.TryGetValue(ObjectId, out DatasmithActorInfo ActorInfo))
						{
							ExportContext.UpdateChildActorsMaterialIndex(ActorInfo);
						}
					}
					break;
				case RhinoDoc.RenderContentTableEventType.Loaded:
				case RhinoDoc.RenderContentTableEventType.Cleared:
				case RhinoDoc.RenderContentTableEventType.Clearing:
				default:
					break;
			}
		}

		private void OnTextureMappingEvent(object Sender, RhinoDoc.TextureMappingEventArgs RhinoEventArgs)
		{
			// The TextureMappingEvent is really unpractical in Rhino, the only piece of information we get to determine
			// which object changed, is the ID of the TextureMapping. We can cache known ID to object mapping and use that cache
			// when an existing TextureMapping is modified. But when the user creates a new TextureMapping, we can't
			// know for sure which object is affected.

			if (RhinoEventArgs.EventType != RhinoDoc.TextureMappingEventType.Modified)
			{
				// The OnModifyObjectAttributes() is already fired for every event type except for TextureMappingEventType.Modified,
				// so we really just need to handle that specific case. This is also preferable, since we can't know for sure which
				// object is affected during the Added, Deleted and Undeleted event types.
				return;
			}

			const bool bReparent = false;
			RhinoObject ChangedObject;
			if(ExportContext.TextureMappindIdToRhinoObject.TryGetValue(RhinoEventArgs.NewMapping.Id, out ChangedObject))
			{
				TryCatchExecute(() => ExportContext.ModifyActor(ChangedObject, bReparent));
			}
			else
			{
				// If for some reason the mapped object is not in the cache, manually search for it in the document. 
				foreach (RhinoObject CurrentObject in RhinoEventArgs.Document.Objects)
				{
					foreach (int ChannelId in CurrentObject.GetTextureChannels())
					{
						Rhino.Render.TextureMapping Mapping = CurrentObject.GetTextureMapping(ChannelId);
						if (Mapping.Id == RhinoEventArgs.NewMapping.Id)
						{
							TryCatchExecute(() => ExportContext.ModifyActor(ChangedObject, bReparent));

							// Testing showed that it's not possible to bulk change multiple TextureMapping at the same time.
							// It should be safe to return after the first object match.
							return;
						}
					}
				}
			}
		}

		private void OnDocumentPropertiesChanged(object Sender, DocumentEventArgs RhinoEventArgs)
		{
			if (RhinoEventArgs.Document != null && ExportContext.ExportOptions.ModelUnitSystem != RhinoEventArgs.Document.ModelUnitSystem)
			{
				ExportContext.OnModelUnitChange();
			}
			else
			{
				ExportContext.bDocumentPropertiesChanged = true;
			}
		}

		public void AddActor(RhinoObject InObject)
		{
			if (OngoingEvent == RhinoOngoingEventTypes.MovingActor)
			{
				// Make sure to only update referenced object, as the mesh has already been moved.
				TryCatchExecute(() => ExportContext.UpdateActorObject(InObject));
				EventStack.Pop();
			}
			else if (OngoingEvent == RhinoOngoingEventTypes.ReplacingActor)
			{
				TryCatchExecute(() => ExportContext.ModifyActor(InObject, /*bReparent=*/false));
				EventStack.Pop();
			}
			else if (!InObject.IsInstanceDefinitionGeometry)
			{
				//Only add the object to the scene if it's not part of an instance definition. 
				//If it part of a definition it will be added when the actual instance will be created.
				TryCatchExecute(() => ExportContext.AddActor(InObject));
			}
		}

		/// <summary>
		/// Helper function used to catch any error the DatasmithExporter may trigger, otherwise it would fail silently.
		/// </summary>
		/// <param name="UpdateAction"></param>
		private void TryCatchExecute(Action UpdateAction)
		{
			try
			{
				UpdateAction();
			}
			catch (Exception e)
			{
				RhinoApp.WriteLine(Resources.UnexpectedError);
				RhinoApp.WriteLine(e.ToString());
			}
		}
	}
}
