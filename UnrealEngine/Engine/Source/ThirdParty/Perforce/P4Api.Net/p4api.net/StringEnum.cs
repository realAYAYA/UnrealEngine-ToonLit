using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Defines the case for the StringEnum.
	/// </summary>
	[Flags]
	public enum StringEnumCase 
	{
		/// <summary>
		/// No case defined.
		/// </summary>
		None = 0x000,
		/// <summary>
		/// Lowercase.
		/// </summary>
		Lower = 0x001,
		/// <summary>
		/// Uppercase.
		/// </summary>
		Upper = 0x002
	}

	internal class StringEnum<T> // where T : IComparable<T>
	{
		protected T value;

        public StringEnum(T v)
        {
            if ((v is Enum) == false)
                throw new ArgumentException("StringEnum may only be used with objects that are of type enum");
            value = v;
        }

        public StringEnum(string s)
        {
            s = s.Replace(" ", string.Empty);
            s = s.Replace("/", string.Empty);
            value = (T)Enum.Parse(typeof(T), s);
        }

        public StringEnum(string s, bool safe)
        {
            try
            {
                s = s.Replace(" ", string.Empty);
                s = s.Replace("/", string.Empty);
                value = (T)Enum.Parse(typeof(T), s);
            }
            catch
            {
                if (safe)
                {
                    value = default(T);
                }
                else { throw; }
            }
        }

        public StringEnum(string s, bool safe, bool ignoreCase)
        {
            try
            {
                s = s.Replace(" ", string.Empty);
                s = s.Replace("/", string.Empty);
                value = (T)Enum.Parse(typeof(T), s, ignoreCase);
            }
            catch
            {
                if (safe)
                {
                    value = default(T);
                }
                else { throw; }
            }
        }

        public static implicit operator T(StringEnum<T> t) { return (t != null) ? t.value : default(T); }
		public static implicit operator StringEnum<T>(T t) { return new StringEnum<T>(t); }

		public static implicit operator StringEnum<T>(string s)
		{
			StringEnum<T> val = null;
			string v = s.Replace(" ", string.Empty);
			v = v.Replace("/", string.Empty);
			if (TryParse(v, true, ref val))
			{
				return val;
			}
			return new StringEnum<T>(default(T));
		}

		public static implicit operator string(StringEnum<T> v)
		{
			if (v != null)
			{
				return v.ToString();
			}
			return string.Empty;
		}

		public override bool Equals(object obj)
		{
			if (obj.GetType() == typeof(T))
			{
				return value.Equals((T)obj);
			}
			if (obj.GetType() == typeof(StringEnum<T>))
			{
				return value.Equals(((StringEnum<T>)obj).value);
			}
			return false;
		}

        public override int GetHashCode() { return base.GetHashCode(); }

        public static bool operator ==(StringEnum<T> t1, StringEnum<T> t2) 
		{
			if ((((object)t1) == null) || (((object)t2) == null))
			{
				return ((object)t1) == ((object)t2);
			}
			return (t1!=null)?t1.value.Equals(t2.value):t2==null; 
		}
		public static bool operator !=(StringEnum<T> t1, StringEnum<T> t2) 
		{
			if ((((object)t1) == null) || (((object)t2) == null))
			{
				return ((object)t1) != ((object)t2);
			}
			return (t1 != null) ? !t1.value.Equals(t2.value) : t2 != null; 
		}

		public static bool operator ==(T t1, StringEnum<T> t2) 
		{
			if (((object)t2) == null)
			{
				return false;
			}
			return t1.Equals(t2.value); 
		}
		public static bool operator !=(T t1, StringEnum<T> t2) 
		{
			if (((object)t2) == null)
			{
				return true;
			}
			return !t1.Equals(t2.value); 
		}

		public static bool operator ==(StringEnum<T> t1, T t2) 
		{ 
			if (((object)t1) == null)
			{
				return false;
			}
			return t1.value.Equals(t2);
		}
		public static bool operator !=(StringEnum<T> t1, T t2) 
		{
			if (((object)t1) == null)
			{
				return true;
			}
			return !t1.value.Equals(t2); 
		}

		public override string ToString()
		{
			return value.ToString();
		}

		public virtual string ToString(StringEnumCase c)
		{
			if (c == StringEnumCase.Lower)
				return ToString().ToLower(); ;
			if (c == StringEnumCase.Upper)
				return ToString().ToUpper(); ;
			return ToString();
		}

		public static bool TryParse(string str, ref StringEnum<T> val)
		{
			try
			{
				T v = (T)Enum.Parse(typeof(T), str);
				val = new StringEnum<T>(v);
				if (val == null)
				{
					return false;
				}
				return true;
			}
			catch (Exception ex) 
			{
				Debug.Trace(ex.Message);
			}
			return false;
		}

		public static bool TryParse(string str, bool ignoreCase, ref StringEnum<T> val)
		{
			try
			{
				T v = (T)Enum.Parse(typeof(T), str, ignoreCase);
				val = new StringEnum<T>(v);
				if (val == null)
				{
					return false;
				}
				return true;
			}
			catch (Exception ex)
			{
				Debug.Trace(ex.Message);
			}
			return false;
		}
	}

	internal class StringEnumList<T> : IList<T>
	{
		internal IList<StringEnum<T>> v;

		public StringEnumList()
		{
			v = new List<StringEnum<T>>();
		}

		public StringEnumList(IList<T> l)
		{
			v = new List<StringEnum<T>>(l.Count);

			for (int idx = 0; idx < l.Count; idx++)
			{
				v.Add(l[idx]);
			}
		}

		#region IList<T> Members

		public int IndexOf(T item)
		{
			return v.IndexOf(item);
		}

		public void Insert(int index, T item)
		{
			v.Insert(index, item);
		}

		public void RemoveAt(int index)
		{
			v.RemoveAt(index);
		}

		public T this[int index]
		{
			get
			{
				return v[index];
			}
			set
			{
				v[index] = value;
			}
		}

		#endregion

		#region ICollection<T> Members

		public void Add(T item)
		{
			v.Add(item);
		}

		public void Clear()
		{
			v.Clear();
		}

		public bool Contains(T item)
		{
			return v.Contains(item);
		}

		public void CopyTo(T[] array, int arrayIndex)
		{
			if (array == null)
			{
				throw new ArgumentNullException("array");
			}
			if (arrayIndex < 0)
			{
				throw new ArgumentOutOfRangeException("arrayIndex is less than zero");
			}
			if (arrayIndex + array.Length >  v.Count)
			{
				throw new ArgumentException("The number of elements in the source array is greater than the available space from arrayIndex to the end of the destination array.");
			}
			for (int idx = 0; idx < array.Length; idx++)
			{
				v[arrayIndex+idx] = array[idx];
			}
		}

		public int Count
		{
			get { return v.Count; }
		}

		public bool IsReadOnly
		{
			get { return v.IsReadOnly; }
		}

		public bool Remove(T item)
		{
			return v.Remove(item);
		}

		#endregion

		#region IEnumerable<T> Members

		public IEnumerator<T> GetEnumerator()
		{
			return new StringEnumListEnumerator<T>(v);
		}

		#endregion

		#region IEnumerable Members

		System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
		{
			return v.GetEnumerator();
		}

		#endregion
	}

	internal class StringEnumListEnumerator<T> : IEnumerator<T>
	{
		IEnumerator<StringEnum<T>> v;

		public StringEnumListEnumerator(IList<StringEnum<T>> l)
		{
			v = l.GetEnumerator();
		}

		#region IEnumerator<T> Members

		public T Current
		{
			get { return v.Current; }
		}

		#endregion

		#region IDisposable Members

		public void Dispose()
		{
			v.Dispose();
		}

		#endregion

		#region IEnumerator Members

		object System.Collections.IEnumerator.Current
		{
			get { return v.Current; }
		}

		public bool MoveNext()
		{
			return v.MoveNext();
		}

		public void Reset()
		{
			v.Reset();
		}

		#endregion
	}
}
