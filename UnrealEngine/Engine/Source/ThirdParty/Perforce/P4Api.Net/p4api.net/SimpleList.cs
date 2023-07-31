using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{

    public class SimpleListItem<T>
    {
        public SimpleListItem(T item)
        {
            Item = item;
            Next = null;
        }
        public T Item { get; private set; }
        public SimpleListItem<T> Next { get; internal set; }
    }

    public class SimpleList<T>
    {
        public SimpleList() 
        {
            Count = 0;
            Head = null;
            Tail = null;
        }

        public SimpleListItem<T> Head { get; private set; }
        public SimpleListItem<T> Tail { get; private set; }

        public int Count { get; private set; }

        public int Add(T item)
        {
            if (Tail == null)
            {
                Head = new SimpleListItem<T>(item);
                Tail = Head;
            }
            else
            {
                Tail.Next = new SimpleListItem<T>(item);
                Tail = Tail.Next;
            }
            Count++;
            return Count;
        }

        public override string ToString()
        {
            if (Head == null)
            {
                return null;
            }

            StringBuilder sb = new StringBuilder(Count * 80);

            SimpleListItem<T> curItem = Head;
            int idx = 0;
            while ((curItem != null) && (idx < Count))
            {
                if (idx > 0)
                {
                    sb.Append("\r\n");
                }
                sb.Append(curItem.Item.ToString());
                curItem = curItem.Next;
            }
            return sb.ToString();
        }

        public T[] ToArray()
        {
            if (Head == null)
            {
                return null;
            }

            T[] value = new T[Count];

            SimpleListItem<T> curItem = Head;
            int idx = 0;
            while ((curItem != null) && (idx < Count))
            {
                value[idx++] = curItem.Item;
                curItem = curItem.Next;
            }
            return value;
        }

        public static explicit operator T[](SimpleList<T> l)
        {
            return l.ToArray();
        }

        public static explicit operator List<T>(SimpleList<T> l)
        {
            if (l.Head == null)
            {
                return null;
            }

            List<T> value = new List<T>(l.Count);

            SimpleListItem<T> curItem = l.Head;
            while (curItem != null)
            {
                value.Add(curItem.Item);
                curItem = curItem.Next;
            }
            return value;
        }
    }
}
