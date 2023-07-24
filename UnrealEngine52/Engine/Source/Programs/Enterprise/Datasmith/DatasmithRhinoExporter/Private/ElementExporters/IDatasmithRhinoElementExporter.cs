// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.ExportContext;
using DatasmithRhino.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;

namespace DatasmithRhino.ElementExporters
{
	/// <summary>
	/// Abstract class used to unify the export and DirectLink logic of the IDatasmithElements
	/// </summary>
	/// <typeparam name="S">The type of the derived class</typeparam>
	/// <typeparam name="T">The type of the DatasmithInfoBase for the element we are exporting</typeparam>
	public abstract class IDatasmithRhinoElementExporter<S, T>
		where S : IDatasmithRhinoElementExporter<S, T>
		where T : DatasmithInfoBase
	{
		private static S Singleton = null;

		/// <summary>
		/// Singleton instance.
		/// </summary>
		public static S Instance
		{
			get
			{
				if(Singleton == null)
				{
					Singleton = (S)Activator.CreateInstance(typeof(S));
				}
				return Singleton;
			}
		}

		/// <summary>
		/// The DatasmithScene we are exporting to.
		/// </summary>
		protected FDatasmithFacadeScene DatasmithScene;

		/// <summary>
		/// The export context.
		/// </summary>
		protected DatasmithRhinoExportContext ExportContext;

		/// <summary>
		/// Overridable bool indicating if the element synchronization should be run on an async thread.
		/// </summary>
		protected virtual bool bShouldUseThreading => false;

		/// <summary>
		/// Function used to synchronize the IDatasmithElement to the IDatasmithScene.
		/// It applies the state of the DatasmithInfoBase to the IDatasmithElement, as such it takes care of element creation, modification and deletion.
		/// </summary>
		/// <param name="InDatasmithScene">The DatasmithScene we are syncing to.</param>
		/// <param name="InExportContext">The ExportContext holding the parsed Rhino document.</param>
		public void SynchronizeElements(FDatasmithFacadeScene InDatasmithScene, DatasmithRhinoExportContext InExportContext)
		{
			DatasmithScene = InDatasmithScene;
			ExportContext = InExportContext;

			if (bShouldUseThreading)
			{
				SynchronizeAsynchronously();
			}
			else
			{
				SynchronizeSynchronously();
			}
		}

		private void SynchronizeSynchronously()
		{
			int TotalNumberOfElements = GetElementsToSynchronizeCount();
			int ElementIndex = 0;

			foreach (T CurrentElementInfo in GetElementsToSynchronize())
			{
				DatasmithRhinoProgressManager.Instance.UpdateCurrentTaskProgress((float)(++ElementIndex) / TotalNumberOfElements);

				ValidateElement(CurrentElementInfo);

				switch (CurrentElementInfo.DirectLinkStatus)
				{
					case DirectLinkSynchronizationStatus.Created:
						FDatasmithFacadeElement CreatedElement = CreateElement(CurrentElementInfo);
						if (CreatedElement != null)
						{
							CurrentElementInfo.SetExportedElement(CreatedElement);
							AddElement(CurrentElementInfo);
							CurrentElementInfo.ApplySyncedStatus();
						}
						else
						{
							//#ueent_todo Log elements who could not be exported in Datasmith logging API.
						}
						break;
					case DirectLinkSynchronizationStatus.Modified:
						ModifyElement(CurrentElementInfo);
						CurrentElementInfo.ApplySyncedStatus();
						break;
					case DirectLinkSynchronizationStatus.PendingDeletion:
					case DirectLinkSynchronizationStatus.PendingHidding:
						if (CurrentElementInfo.ExportedElement != null)
						{
							DeleteElement(CurrentElementInfo);
						}
						CurrentElementInfo.ApplySyncedStatus();
						break;
					case DirectLinkSynchronizationStatus.Synced:
					case DirectLinkSynchronizationStatus.Hidden:
					case DirectLinkSynchronizationStatus.Deleted:
						//Element is synced, nothing to do.
						break;
					case DirectLinkSynchronizationStatus.None:
					default:
						Debug.Assert(false, string.Format("Invalid element info state: {0}", CurrentElementInfo.DirectLinkStatus));
						break;
				}
			}
		}

		private void SynchronizeAsynchronously()
		{
			const int InvalidIndex = -1;

			//List of Tuple for which: 
			// Item1: The DatasmithInfoBase of the element to Create or Modify asynchronously.
			// Item2: The remapping index of the Created elements for the CreatedElements array. For existing elements this value is set to InvalidIndex.
			List<Tuple<T,int>> ElementInfosToProcess = new List<Tuple<T, int>>();
			int NumberOfElementsToCreate = 0;

			// First gather all the elements on which we can do async operations, execute the non-async operations.
			foreach (T CurrentElementInfo in GetElementsToSynchronize())
			{
				ValidateElement(CurrentElementInfo);

				switch (CurrentElementInfo.DirectLinkStatus)
				{
					case DirectLinkSynchronizationStatus.Created:
						ElementInfosToProcess.Add(new Tuple<T, int>(CurrentElementInfo, NumberOfElementsToCreate++));
						break;
					case DirectLinkSynchronizationStatus.Modified:
						ElementInfosToProcess.Add(new Tuple<T, int>(CurrentElementInfo, InvalidIndex));
						break;
					case DirectLinkSynchronizationStatus.PendingDeletion:
					case DirectLinkSynchronizationStatus.PendingHidding:
						// Element deletion cannot be done asynchronously.
						if (CurrentElementInfo.ExportedElement != null)
						{
							DeleteElement(CurrentElementInfo);
						}
						CurrentElementInfo.ApplySyncedStatus();
						break;
					case DirectLinkSynchronizationStatus.Synced:
					case DirectLinkSynchronizationStatus.Hidden:
					case DirectLinkSynchronizationStatus.Deleted:
						//Element is synced, nothing to do.
						break;
					case DirectLinkSynchronizationStatus.None:
					default:
						Debug.Assert(false, string.Format("Invalid element info state: {0}", CurrentElementInfo.DirectLinkStatus));
						break;
				}
			}
			T[] CreatedElementInfos = new T[NumberOfElementsToCreate];

			Rhino.Runtime.HostUtils.DisplayOleAlerts(false);

			// Then execute the Create and Modify asynchronously.
			int CompletedElements = 0;
			bool bExportCancelled = false;
			Thread RhinoThread = Thread.CurrentThread;
			Parallel.For(0, ElementInfosToProcess.Count, (ElementIndex, LoopState) =>
			{
				T CurrentElementInfo = ElementInfosToProcess[ElementIndex].Item1;
				int RemappingIndex = ElementInfosToProcess[ElementIndex].Item2;
				if (CurrentElementInfo.DirectLinkStatus == DirectLinkSynchronizationStatus.Created)
				{
					FDatasmithFacadeElement CreatedElement = CreateElement(CurrentElementInfo);
					CreatedElementInfos[RemappingIndex] = CurrentElementInfo;
					if (CreatedElement != null)
					{
						CurrentElementInfo.SetExportedElement(CreatedElement);
					}
				}
				else
				{
					ModifyElement(CurrentElementInfo);
					CurrentElementInfo.ApplySyncedStatus();
				}

				// Update the progress, only on rhino's main thread.
				Interlocked.Increment(ref CompletedElements);
				if (Thread.CurrentThread == RhinoThread)
				{
					try
					{
						DatasmithRhinoProgressManager.Instance.UpdateCurrentTaskProgress((float)(CompletedElements) / ElementInfosToProcess.Count);
					}
					catch (DatasmithExportCancelledException)
					{
						bExportCancelled = true;
						LoopState.Break();
					}
				}
			});

			Rhino.Runtime.HostUtils.DisplayOleAlerts(true);

			if (bExportCancelled)
			{
				throw new DatasmithExportCancelledException();
			}

			// Finally, add the created elements to the scene.
			for (int ExportedElementIndex = 0; ExportedElementIndex < CreatedElementInfos.Length; ++ExportedElementIndex)
			{
				T CreatedElementInfo = CreatedElementInfos[ExportedElementIndex];
				if (CreatedElementInfo.ExportedElement != null)
				{
					AddElement(CreatedElementInfo);
					CreatedElementInfo.ApplySyncedStatus();
				}
				else
				{
					//#ueent_todo Log elements who could not be exported in Datasmith logging API.
				}
			}
		}

		private void ValidateElement(T ElementInfo)
		{
			// We only care of the element validity if it is flagged as Created or Modified, since all other statuses either don't export the element or delete it.
			bool bDoesElementNeedToBeParsed = (ElementInfo.DirectLinkStatus & (DirectLinkSynchronizationStatus.Created | DirectLinkSynchronizationStatus.Modified)) != DirectLinkSynchronizationStatus.None;
			bool bIsElementDisposed = ElementInfo.RhinoCommonObject != null && ElementInfo.RhinoCommonObject.Disposed;

			if (bDoesElementNeedToBeParsed && bIsElementDisposed)
			{
				Debug.Fail(string.Format("Trying to export a disposed rhino element: {0}", ElementInfo.UniqueLabel));
				ElementInfo.ApplyDeletedStatus();
			}
		}

		/// <summary>
		/// Returns the total number of elements to process. Used for progress update.
		/// </summary>
		/// <returns></returns>
		protected abstract int GetElementsToSynchronizeCount();

		/// <summary>
		/// Returns an enumerator of the derived type of DatasmithInfoBase of the elements to process.
		/// </summary>
		/// <returns></returns>
		protected abstract IEnumerable<T> GetElementsToSynchronize();

		/// <summary>
		/// Called before AddElement(), creates and returns a new FDatasmithFacadeElement to be added to the DatasmithScene.
		/// </summary>
		/// <returns></returns>
		protected abstract FDatasmithFacadeElement CreateElement(T ElementInfo);

		/// <summary>
		/// Called after CreateElement() to add the new element to the scene. The distinction between those 2 functions allows for safe asynchronous element creation.
		/// </summary>
		protected abstract void AddElement(T ElementInfo);

		/// <summary>
		/// Called when we need to update the state of an element already existing in the scene.
		/// </summary>
		protected abstract void ModifyElement(T ElementInfo);

		/// <summary>
		/// Called when we need to remove an element currently existing in the scene.
		/// </summary>
		protected abstract void DeleteElement(T ElementInfo);
	}
}