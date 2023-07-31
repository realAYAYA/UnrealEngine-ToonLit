using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Augment String List used mostly for passing of parameters to command.
	/// </summary>
	public class StringList : List<String>
	{
		/// <summary>
		/// Default constructer
		/// </summary>
		public StringList() : base() { }
		/// <summary>
		/// Construct a list with the specified capacity and preallocate the members
		/// </summary>
		/// <param name="capacity"></param>
		public StringList(int capacity)
			: base(capacity)
		{
			// allocate the members
			for (int idx = 0; idx < capacity; idx++)
				this.Add(null);
		}

		/// <summary>
		/// Create a list from a string array
		/// </summary>
		/// <param name="l"></param>
		public StringList(String[] l)
		
		{
			for (int idx = 0; idx < l.Length; idx++)
				this.Add(l[idx]);
		}

		/// <summary>
		/// Cast a string array to a string list
		/// </summary>
		/// <param name="l">The list to cast</param>
		/// <returns>New StringList representing the results of the cast</returns>
		public static implicit operator StringList(String[] l)
		{
			if (l == null)
				return null;
			return new StringList(l);
		}

		/// <summary>
		/// Cast a StringList to a String[]
		/// </summary>
		/// <param name="l">The StringList being cast</param>
		/// <returns>new String[]</returns>
		public static implicit operator String[](StringList l)
		{
			if (l == null)
				return null;
			String[] v = new String[l.Count];
			for (int idx = 0; idx < l.Count; idx++)
				v[idx] = l[idx];

			return v;
		}

		/// <summary>
		/// Copy elements from another StringList into this list
		/// </summary>
		/// <param name="src">The source StringList</param>
		/// <param name="destIdx">The index of the first element copied in the destination array</param>
		/// <param name="cnt">How many elements to copy</param>
		public void Copy(StringList src, int destIdx, int cnt)
		{
			// grow the list if needed
			if (Count < destIdx + cnt)
			{
				Capacity = destIdx + cnt;
				for (int idx = Count; idx < destIdx + cnt; idx++)
				{
					this.Add(null);
				}
			}
			for (int idx = 0; idx < cnt; idx++)
			{
				this[destIdx + idx] = src[idx];
			}
		}

		/// <summary>
		/// Test to see if an object is equal to this StringLis. An object is 
		/// equal if it is a StringArray (or can be cast as one and has the 
		/// same elements in the same order.
		/// </summary>
		/// <param name="obj">object to test</param>
		/// <returns>true if equal</returns>
		public override bool Equals(object obj)
		{
			StringList l = obj as StringList;

			if (l == null)
				return false; // can't equal, it's null or the wrong type

			if (Count != l.Count)
				return false;

			for (int idx = 0; idx < Count; idx++)
			{
				if (this[idx] != l[idx])
					return false;
			}
			return true;
		}

		/// <summary>
		/// Override to quell compilation warning
		/// </summary>
		/// <returns></returns>
		public override int GetHashCode()
		{
			return base.GetHashCode();
		}

		/// <summary>
		/// Test to see if two StringList are equal. They are equal if they
		/// have the same elements in the same order.
		/// </summary>
		/// <param name="l1">The first list</param>
		/// <param name="l2">The second list</param>
		/// <returns>true if equal</returns>
		public static bool operator ==(StringList l1, StringList l2)
		{
			// cast to object or we'll recurse
			if ((((object)l1) == null) && (((object)l2) == null))
				return true; // if both null, are equal
			if ((((object)l1) == null) || (((object)l2) == null))
				return false; // if only one is null, not equal

			if (l1.Count != l2.Count)
				return false;

			for (int idx = 0; idx < l1.Count; idx++)
			{
				if (l1[idx] != l2[idx])
					return false;
			}
			return true;
		}

		/// <summary>
		/// Test to see if to StringList are different (not equal)
		/// </summary>
		/// <param name="l1">The first list</param>
		/// <param name="l2">The second list</param>
		/// <returns>true if not equal</returns>
		public static bool operator !=(StringList l1, StringList l2)
		{
			// cast to object or we'll recurse
			if ((((object)l1) == null) && (((object)l2) == null))
				return false; // if both null, are equal
			if ((((object)l1) == null) || (((object)l2) == null))
				return true; // if only one is null, not equal

			if (l1.Count != l2.Count)
				return true;

			for (int idx = 0; idx < l1.Count; idx++)
			{
				if (l1[idx] != l2[idx])
					return true;
			}
			return false;
		}

		/// <summary>
		/// Convert the list to a single String. Each element is
		/// separated by a /r/n line separator.
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			String v = String.Empty;
			for (int idx = 0; idx < Count; idx++)
			{
				if (v.Length > 0)
					v += "/r/n";
				v += this[idx];
			}
			return v;
		}

		/// <summary>
		/// Add to string lists
		/// </summary>
		/// <param name="l">The left list</param>
		/// <param name="r">The right list</param>
		/// <returns>A new list consisting of the elements of the left list followed by the elements of the right list</returns>
		public static StringList operator +(StringList l, StringList r)
		{
			if ((l == null) && (r == null))
				return null;

			int lCnt = (l == null) ? 0 : l.Count;
			int rCnt = (r == null) ? 0 : r.Count;

			StringList v = new StringList(lCnt + rCnt);

			for (int i = 0; i < lCnt; i++)
				v[i] = l[i];

			for (int i = 0; i < rCnt; i++)
				v[lCnt + i] = r[i];

			return v;
		}

		/// <summary>
		/// Test to see if the StringList is null or empty (has no elements)
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		public static bool IsNullOrEmpy(StringList s)
		{
			return ((s == null) || (s.Count <= 0));
		}
	}
}
