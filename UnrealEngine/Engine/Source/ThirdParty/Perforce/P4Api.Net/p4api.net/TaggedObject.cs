using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// The tagged output of a command.
	/// </summary>
	public class TaggedObject : Dictionary<String, String>
	{
		/// <summary>
		/// Basic constrictor
		/// </summary>
		public TaggedObject() : base() { }

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="obj">Source object</param>
		public TaggedObject(TaggedObject obj) : base(obj) { }
	};

	/// <summary>
	/// A list of tagged objects.
	/// </summary>
	public class TaggedObjectList : List<TaggedObject> 
    {
        /// <summary>
        /// Default Constructor
        /// </summary>
        public TaggedObjectList() : base() { }

        /// <summary>
        /// Constructor which specifies capacity
        /// </summary>
        /// <param name="capacity">number of objects supported</param>
        public TaggedObjectList(int capacity) : base(capacity) { }

        /// <summary>
        /// Create a TaggedObjectList from a SimpleList of tagged objects
        /// </summary>
        /// <param name="l">SimpleList containing tagged objects</param>
        /// <returns>TaggedObjectList containing tagged objects</returns>
        public static explicit operator TaggedObjectList(SimpleList<TaggedObject> l)
        {
            if (l.Head == null)
            {
                return null;
            }

            TaggedObjectList value = new TaggedObjectList(l.Count);

            SimpleListItem<TaggedObject> curItem = l.Head;
            while (curItem != null)
            {
                value.Add(curItem.Item);
                curItem = curItem.Next;
            }
            return value;
        }
    };

	///// <summary>
	///// List of info messages.
	///// </summary>
	//public class InfoList : List<InfoLine>
	//{
	//    /// <summary>
	//    /// Cast to a String[].
	//    /// </summary>
	//    /// <param name="l"></param>
	//    /// <returns></returns>
	//    public static implicit operator String[](InfoList l)
	//    {
	//        String[] r = new String[l.Count];
	//        int idx = 0;
	//        foreach (InfoLine i in l)
	//            r[idx++] = i.ToString();
	//        return r;
	//    }

	//    /// <summary>
	//    /// Cast to a String. Messages are separated by \r\n
	//    /// </summary>
	//    /// <param name="l"></param>
	//    /// <returns></returns>
	//    public static implicit operator String(InfoList l)
	//    {
	//        StringBuilder r = new StringBuilder(l.Count * 80);
	//        foreach (InfoLine i in l)
	//        {
	//            r.Append(i.ToString());
	//            r.Append("/r/n");
	//        }
	//        return r.ToString();
	//    }
	//}

	///// <summary>
	///// A single line of output from an 'info' message.
	///// </summary>
	//public class InfoLine
	//{
	//    /// <summary>
	//    /// The level of the message (0-9)
	//    /// </summary>
	//    public int Level;
	//    /// <summary>
	//    /// The level of the message (0-9)
	//    /// </summary>
	//    public uint CommandId;
	//    /// <summary>
	//    /// The message
	//    /// </summary>
	//    public String Info;

	//    /// <summary>
	//    /// Create a new InfoLine
	//    /// </summary>
	//    /// <param name="nLevel">Level of the message</param>
	//    /// <param name="nInfo">Message text.</param>
	//    public InfoLine(uint cmdId, int nLevel, String nInfo)
	//    {
	//        CommandId = cmdId;
	//        Level = nLevel;
	//        Info = nInfo;
	//    }

	//    /// <summary>
	//    /// Convert the info to text
	//    /// </summary>
	//    /// <returns>String representation</returns>
	//    public override string ToString()
	//    {
	//        String levelDots = String.Empty;
	//        for (int idx = 0; idx < Level; idx++)
	//            levelDots += ".";
	//        return String.Format("{0}{1}", levelDots, Info);
	//    }
	//}

	/// <summary>
	/// Base class for objects returned by a command as 'tagged' data.
	/// </summary>
	/// <remarks>
	/// Contains a Hashtable of the field values for the object.
	/// Derived object can provide properties to directly access
	/// their standard attributes.
	/// </remarks>
	public class TaggedInfoItem : TaggedObject
	{
		private String name;
		/// <summary>
		/// String that that is the field that identifies this object
		/// </summary>
		public String Name
		{
			get { return name; }
		}
		/// <summary>
		/// The raw data returned from the server
		/// </summary>
		public TaggedObject ItemData
		{
			get { return (TaggedObject)this; }
		}

		/// <summary>
		/// Default constructer
		/// </summary>
		public TaggedInfoItem()
		{
			name = String.Empty;
		}
	}
}
