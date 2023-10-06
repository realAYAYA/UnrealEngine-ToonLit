// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
	public class FTriangle
	{
		private int[] Indices = new int[3];

		public int Index1 { get { return Indices[0]; } set { Indices[0] = value; } }
		public int Index2 { get { return Indices[1]; } set { Indices[1] = value; } }
		public int Index3 { get { return Indices[2]; } set { Indices[2] = value; } }

		public int this[int InWhich]
		{
			get { return Indices[InWhich]; }
			set { Indices[InWhich] = value; }
		}

		public int MaterialID { get; set; }

		public FTriangle(int InIdx0, int InIdx1, int InIdx2, int InMaterialID)
		{
			Indices[0] = InIdx0;
			Indices[1] = InIdx1;
			Indices[2] = InIdx2;
			MaterialID = InMaterialID;
		}

		public static bool operator ==(FTriangle A, FTriangle B)
		{
			if (ReferenceEquals(A, B))
			{
				return true;
			}

			if (A is null || B is null)
			{
				return false;
			}

			return A.Index1 == B.Index1 && A.Index2 == B.Index2 && A.Index3 == B.Index3 && A.MaterialID == B.MaterialID;
		}

		public static bool operator !=(FTriangle A, FTriangle B)
		{
			return !(A == B);
		}

		public override bool Equals(object Obj)
		{
			return Obj is FTriangle Other && this == Other;
		}

		public FTriangle Offset(int InOffset)
		{
			return new FTriangle(Indices[0] + InOffset, Indices[1] + InOffset, Indices[2] + InOffset, MaterialID);
		}

		public override int GetHashCode() 
		{
			int Hash = 7;

			foreach (int Index in Indices)
			{
				Hash = Hash * 17 + Index.GetHashCode();
			}

			return Hash;
		}

		public override string ToString()
		{
			return "" + Index1 + "," + Index2 + "," + Index3;
		}
	}
}
