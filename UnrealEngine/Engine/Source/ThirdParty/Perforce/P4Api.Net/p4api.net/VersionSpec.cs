using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Identifies a specific revision or revision range of a Perforce managed SCM resource.
	/// </summary>
	public  abstract class VersionSpec
	{
		public abstract override string ToString();

		public static HeadRevision Head = new HeadRevision();
		public static HaveRevision Have = new HaveRevision();
		public static NoneRevision None = new NoneRevision();
	}

	/// <summary>
	/// A revision range specifier consisting of a lower and upper revision.
	/// </summary>
	public class VersionRange : VersionSpec
	{
		/// <summary>
		/// A revision range specifier consisting of a lower and upper revision.
		/// </summary>
		///<param name="lower">version spec to get lower revision</param>
		///<param name="upper">version spec to get upper revision</param>
		public VersionRange(VersionSpec lower, VersionSpec upper)
		{
			if (lower == null)
			{
				throw new ArgumentNullException("lower");
			}
			if (upper == null)
			{
				throw new ArgumentNullException("upper");
			}
			Lower = lower;
			Upper = upper;
		}
		/// <summary>
		/// A revision range specifier consisting of a lower and upper revision.
		/// </summary>
		///<param name="lower">int to get lower revision</param>
		///<param name="upper">int to get upper revision</param>
		public VersionRange(int lower, int upper)
		{
			Lower = new Revision(lower);
			Upper = new Revision(upper);
		}
		/// <summary>
		/// Lower version revision.
		/// </summary>
		public VersionSpec Lower { get; set; }
		/// <summary>
		/// Upper version revision.
		/// </summary>
		public VersionSpec Upper { get; set; }

		///<summary>ToString method for for VersionRange</summary>
		/// <returns>String version range</returns>
		public override string ToString()
		{
			return String.Format( "{0},{1}", Lower, Upper);
		}
		///<summary>Equals method for for VersionRange</summary>
		///<param name="obj">object to get version range</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			VersionRange o = obj as VersionRange;

			if (o.Lower != null)
			{
				if (o.Lower.Equals(this.Lower) == false)
				{ return false; }
			}
			else
			{
				if (this.Lower != null)
				{ return false; }
			}
			if (o.Upper != null)
			{
				if (o.Upper.Equals(this.Upper) == false)
				{ return false; }
			}
			else
			{
				if (this.Upper != null)
				{ return false; }
			}

			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }

    /// <summary>
    /// A revision specifier #head.
    /// </summary>
    public class HeadRevision : VersionSpec
	{
		///<summary>ToString method for for HeadRevision</summary>
		/// <returns>#head</returns>
		public override string ToString()
		{
			return "#head";
		}
		///<summary>Equals method for for HeadRevision</summary>
		///<param name="obj">object to get head revision</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }

    /// <summary>
    /// A revision specifier #have.
    /// </summary>
    public class HaveRevision : VersionSpec
	{
		///<summary>ToString method for for HaveRevision</summary>
		/// <returns>#have</returns>
		public override string ToString()
		{
			return "#have";
		}
		///<summary>Equals method for for HaveRevision</summary>
		///<param name="obj">object to get have revision</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }

    /// <summary>
    /// A revision specifier #none.
    /// </summary>
    public class NoneRevision : VersionSpec
	{
		///<summary>ToString method for for NoneRevision</summary>
		/// <returns>#none</returns>
		public override string ToString()
		{
			return "#none";
		}
		///<summary>Equals method for for NoneRevision</summary>
		///<param name="obj">object to get none revision</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }
    /// <summary>
    /// A revision specifier that is a single revision.
    /// </summary>
    public class Revision : VersionSpec
	{
		/// <summary>
		/// A revision specifier that is a single revision.
		/// </summary>
		public Revision(int rev) { Rev = rev; }
		public int Rev { get; private set; }
		///<summary>ToString method for for Revision</summary>
		/// <returns>String client revision version</returns>
		public override string ToString()
		{
			if (Rev >= 0)
			{
				return String.Format("#{0}", Rev);
			}
			return string.Empty;
		}
		///<summary>Equals method for for Revision</summary>
		///<param name="obj">object to get revision</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			Revision o = obj as Revision;
			if (o.Rev != this.Rev)
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }
    /// <summary>
    /// A revision specifier that is a date and time.
    /// </summary>
    public class DateTimeVersion : VersionSpec
	{
		/// <param name="date">The date/time.</param>
		public DateTimeVersion(DateTime date)
		{
			Version = date;
		}
		///<summary>get Version as date/time</summary>
		public DateTime Version { get; private set; }
		///<summary>ToString method for for DateTimeVersion</summary>
		/// <returns>String date/time version</returns>
		public override string ToString()
		{
            return String.Format("@{0}", Version.ToString("yyyy/MM/dd:HH:mm:ss",
                System.Globalization.CultureInfo.InvariantCulture));
        }
		///<summary>Equals method for for DateTimeVersion</summary>
		///<param name="obj">object to get date/time</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			DateTimeVersion o = obj as DateTimeVersion;
			if (o.Version != this.Version)
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }

    /// <summary>
    /// A revision specifier that is a label name.
    /// </summary>
    public class LabelNameVersion : VersionSpec
	{
		/// <param name="LabelName">The label.</param>
		public LabelNameVersion(string LabelName)
		{
			Version = LabelName;
		}
		///<summary>get Version as label</summary>
		public string Version { get; private set; }
		///<summary>ToString method for for LabelNameVersion</summary>
		/// <returns>String label version</returns>
		public override string ToString()
		{
			return String.Format("@{0}",Version);
		}
		///<summary>Equals method for for LabelNameVersion</summary>
		///<param name="obj">object to get label</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			LabelNameVersion o = obj as LabelNameVersion;
			if (o.Version != this.Version)
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }

    }

    /// <summary>
    /// A revision specifier that is a changelist id.
    /// </summary>
    public class ChangelistIdVersion : VersionSpec
	{
		/// <param name="Changelist">The changelist.</param>
		public ChangelistIdVersion(int Changelist)
		{
			ChanglistId = Changelist;
		}
		///<summary>get Version as changelist</summary>
		public int ChanglistId { get; private set; }
		///<summary>ToString method for for ChangelistIdVersion</summary>
		/// <returns>String changelist version</returns>
		public override string ToString()
		{
			return String.Format("@{0}", ChanglistId);
		}
		///<summary>Equals method for for ChangelistIdVersion</summary>
		///<param name="obj">object to get changelist</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			ChangelistIdVersion o = obj as ChangelistIdVersion;
			if (o.ChanglistId != this.ChanglistId)
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }

    /// <summary>
    /// A revision specifier that is a client name.
    /// </summary>
    public class ClientNameVersion : VersionSpec
	{
		/// <param name="ClientName">The client name./// </param>
		public ClientNameVersion(string ClientName)
		{
			Version = ClientName;
		}
		///<summary>get Version as client name</summary>
		public string Version { get; private set; }
		///<summary>ToString method for for ClientNameVersion</summary>
		/// <returns>String client name version</returns>
		public override string ToString()
		{
			return String.Format("@{0}", Version);
		}
		///<summary>Equals method for for ClientNameVersion</summary>
		///<param name="obj">object to get client name</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			ClientNameVersion o = obj as ClientNameVersion;
			if (o.Version != this.Version)
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }
    /// <summary>
    /// A revision specifier that is a file action.
    /// </summary>
    public class ActionVersion : VersionSpec
	{
		/// <param name="Action">The file action./// </param>
		public ActionVersion(string Action)
		{
			Version = Action;
		}
		///<summary>get Version as action</summary>
		public string Version { get; private set; }
		///<summary>ToString method for for ActionVersion</summary>
		/// <returns>String action version</returns>
		public override string ToString()
		{
			return String.Format("@{0}", Version);
		}
		///<summary>Equals method for for ActionVersion</summary>
		///<param name="obj">object to get action type</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			ActionVersion o = obj as ActionVersion;
			if (o.Version != this.Version)
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }

    /// <summary>
    /// A revision specifier for a file that is shelved in a changelist id.
    /// </summary>
    public class ShelvedInChangelistIdVersion : VersionSpec
	{
		/// <param name="Changelist">The changelist.</param>
		public ShelvedInChangelistIdVersion(int Changelist)
		{
			ChanglistId = Changelist;
		}
		///<summary>get Version as changelist</summary>
		public int ChanglistId { get; private set; }
		///<summary>ToString method for for ChangelistIdVersion</summary>
		/// <returns>String changelist version</returns>
		public override string ToString()
		{
			return String.Format("@={0}", ChanglistId);
		}
		///<summary>Equals method for for ChangelistIdVersion</summary>
		///<param name="obj">object to get changelist</param>
		/// <returns>True/False</returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
			{ return false; }
			if (obj.GetType() != this.GetType())
			{
				return false;
			}
			ChangelistIdVersion o = obj as ChangelistIdVersion;
			if (o.ChanglistId != this.ChanglistId)
			{
				return false;
			}
			return true;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
    }
}
