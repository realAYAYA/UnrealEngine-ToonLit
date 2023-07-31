// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.CompilerServices;

namespace DatasmithRhino.ExportContext
{
	public enum DirectLinkSynchronizationStatus
	{
		None = 0,
		Created = 1 << 0,
		Modified = 1 << 1,
		Deleted = 1 << 2,
		Synced = 1 << 3,
		PendingDeletion = 1 << 4,
		PendingHidding = 1 << 5,
		//Same as Deleted, except we do not clean up the object afterward.
		Hidden = 1 << 6,
	}

	public abstract class DatasmithInfoBase
	{
		public Rhino.Runtime.CommonObject RhinoCommonObject { get; protected set; }

		/// <summary>
		/// Used as a unique ID corresponding to the IDatasmithElement::Name field.
		/// </summary>
		public string Name { get; private set; }

		/// <summary>
		/// Label corresponding to the IDatasmithElement::Label field. It is generated from the BaseLabel to ensure its unicity.
		/// </summary>
		public string UniqueLabel { get; private set; }

		/// <summary>
		/// BaseLabel is used to generate the UniqueLabel. Changes to the BaseLabel are reflected on the UniqueLabel.
		/// </summary>
		public string BaseLabel { get; private set; }

		private DirectLinkSynchronizationStatus InternalDirectLinkStatus = DirectLinkSynchronizationStatus.Created;
		private DirectLinkSynchronizationStatus PreviousDirectLinkStatus { get; set; } = DirectLinkSynchronizationStatus.None;
		public virtual DirectLinkSynchronizationStatus DirectLinkStatus
		{
			get => InternalDirectLinkStatus;
		}
		public FDatasmithFacadeElement ExportedElement { get; private set; } = null;

		public DatasmithInfoBase(Rhino.Runtime.CommonObject InRhinoObject, string InName, string InUniqueLabel, string InBaseLabel)
		{
			RhinoCommonObject = InRhinoObject;
			Name = InName;
			UniqueLabel = InUniqueLabel;
			BaseLabel = InBaseLabel;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void ApplyModifiedStatus()
		{
			SetDirectLinkStatus(DirectLinkSynchronizationStatus.Modified);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void ApplyHiddenStatus()
		{
			SetDirectLinkStatus(DirectLinkSynchronizationStatus.PendingHidding);
		}

		public bool HasHiddenStatus()
		{
			return DirectLinkStatus == DirectLinkSynchronizationStatus.PendingHidding || DirectLinkStatus == DirectLinkSynchronizationStatus.Hidden;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void ApplyDeletedStatus()
		{
			SetDirectLinkStatus(DirectLinkSynchronizationStatus.PendingDeletion);
		}

		public bool HasDeletedStatus()
		{
			return DirectLinkStatus == DirectLinkSynchronizationStatus.PendingDeletion || DirectLinkStatus == DirectLinkSynchronizationStatus.Deleted;
		}

		private void SetDirectLinkStatus(DirectLinkSynchronizationStatus Status)
		{
			// We ignore the "modified" status if the current status is "created", "deleted" or "hidden".
			// To "undelete" or unhide an element use RestorePreviousDirectLinkStatus().
			const DirectLinkSynchronizationStatus UnmodifiableStates = DirectLinkSynchronizationStatus.Created | DirectLinkSynchronizationStatus.Deleted
				| DirectLinkSynchronizationStatus.PendingDeletion | DirectLinkSynchronizationStatus.PendingHidding | DirectLinkSynchronizationStatus.Hidden;

			if (InternalDirectLinkStatus != Status
				&& (Status != DirectLinkSynchronizationStatus.Modified
				|| (InternalDirectLinkStatus & UnmodifiableStates) == DirectLinkSynchronizationStatus.None))
			{
				PreviousDirectLinkStatus = InternalDirectLinkStatus;
				InternalDirectLinkStatus = Status;
			}
		}

		public void ApplySyncedStatus()
		{
			// Since the Delete and Hide status have a pending phase, we should not override the PreviousDirectLinkStatus in those cases.
			if (InternalDirectLinkStatus == DirectLinkSynchronizationStatus.PendingDeletion)
			{
				InternalDirectLinkStatus = DirectLinkSynchronizationStatus.Deleted;
			}
			else if (InternalDirectLinkStatus == DirectLinkSynchronizationStatus.PendingHidding)
			{
				InternalDirectLinkStatus = DirectLinkSynchronizationStatus.Hidden;
			}
			//If we are not hidden or deleted
			else if ((InternalDirectLinkStatus & ~(DirectLinkSynchronizationStatus.Deleted | DirectLinkSynchronizationStatus.Hidden)) != DirectLinkSynchronizationStatus.None)
			{
				PreviousDirectLinkStatus = InternalDirectLinkStatus;
				InternalDirectLinkStatus = DirectLinkSynchronizationStatus.Synced;
			}
		}

		/// <summary>
		/// Used to undo the last change to the DirectLinkStatus property. It is intended to be used mainly for "undeleted" objects for which the deletion was not synced.
		/// </summary>
		/// <returns></returns>
		public bool RestorePreviousDirectLinkStatus()
		{
			if (InternalDirectLinkStatus == DirectLinkSynchronizationStatus.Hidden)
			{
				// We are in hidden state, that means if we want to restore the DatasmithElement it must be flagged as "created".
				InternalDirectLinkStatus = DirectLinkSynchronizationStatus.Created;
				PreviousDirectLinkStatus = DirectLinkSynchronizationStatus.None;
				return true;
			}
			else if (PreviousDirectLinkStatus != DirectLinkSynchronizationStatus.None)
			{
				InternalDirectLinkStatus = PreviousDirectLinkStatus;
				PreviousDirectLinkStatus = DirectLinkSynchronizationStatus.None;
				return true;
			}

			return false;
		}

		public void SetExportedElement(FDatasmithFacadeElement InExportedElement)
		{
			ExportedElement = InExportedElement;
		}

		public virtual void ApplyDiffs(DatasmithInfoBase OtherInfo)
		{
			SetDirectLinkStatus(DirectLinkSynchronizationStatus.Modified);

			if (BaseLabel != OtherInfo.BaseLabel)
			{
				BaseLabel = OtherInfo.BaseLabel;
				UniqueLabel = OtherInfo.UniqueLabel;
			}
		}
	}
}