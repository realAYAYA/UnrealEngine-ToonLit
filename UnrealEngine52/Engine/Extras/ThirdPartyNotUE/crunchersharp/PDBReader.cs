using System;
using System.Runtime.InteropServices;

namespace PDBReader
{
	unsafe struct PDBHandle { };

	enum TypeRecordKind : ushort
	{
		Modifier = 0x1001,
		Pointer = 0x1002,
		Array = 0x001503,
		Structure = 0x001505,
		Class = 0x001504,
		Union = 0x001506,
		Enum = 0x001507,
		
		Member = 0x00150D,
	}

	[Flags]
	enum ClassFlags : ushort
	{
		Packed = 0x1,
		Ctor = 0x2,
		Ovlops = 0x4,
		IsNested = 0x8,
		CNested = 0x10,
		OpAssign = 0x20,
		OpCast = 0x40,
		FwdRef = 0x80,
		Scoped = 0x100,
		HasUniqueName = 0x200,
		Sealed = 0x400,
		//HFA = 0x1,
		Intrinsic = 0x1000,
		//mocom= 0x1,
	}

	[Flags]
	enum ModifierFlags : ushort
	{
		None = 0x0,
		Const = 0x1,
		Volatile = 0x2,
		Unaligned = 0x4,
	}

	[StructLayout(LayoutKind.Sequential)]
	struct TPIRecord_Class
	{
		public ushort Count;
		public ClassFlags Flags;
		public uint FieldIndex;
		public uint DerivedIndex;
		public uint VshapeIndex;
	}

	[StructLayout(LayoutKind.Sequential)]
	struct TPIRecord_Union
	{
		public ushort Count;
		public ClassFlags Flags;
		public uint FieldIndex;
	}

	[StructLayout(LayoutKind.Explicit)]
	struct TPIRecord
	{
		[FieldOffset(0)]
		public ushort Size;
		[FieldOffset(2)]
		public TypeRecordKind Kind;
		[FieldOffset(4)]
		public TPIRecord_Class Class;
		[FieldOffset(4)]
		public TPIRecord_Union Union;
	}

	[StructLayout(LayoutKind.Sequential)]
	unsafe struct RecordBasicInfo
	{
		public IntPtr Name;
		public ulong Size;
		// Kind == LF_CLASS || Kind == LF_STRUCTURE
		//	-> field list
		// Kind == LF_POINTER
		//	-> pointed-to type
		// Kind == LF_ARRAY
		//	-> element type
		// Kind == LF_MODIFIER
		//	-> type being modified 
		public uint RelatedTypeIndex;
		public ModifierFlags Modifiers;
		public TypeRecordKind Kind;
	}

	[StructLayout(LayoutKind.Sequential)]
	struct StructFieldCounts
	{
		public uint NumDataMembers;
	}

	[Flags]
	enum DataMemberFlags
	{
		None = 0x0,
		IsVtable = 0x1,
		IsBitfield = 0x2,
		IsBaseClass = 0x4,
	}

	[StructLayout(LayoutKind.Sequential)]
	struct DataMemberInfo
	{
		public IntPtr Name;
		public ulong Offset;
		public uint TypeIndex;
		public DataMemberFlags Flags;
		public uint BitPosition;
		public uint BitSize;

		public bool IsVtable { get { return (Flags & DataMemberFlags.IsVtable) != DataMemberFlags.None; } }
		public bool IsBitfield { get { return (Flags & DataMemberFlags.IsBitfield) != DataMemberFlags.None; } }
		public bool IsBaseClass { get { return (Flags & DataMemberFlags.IsBaseClass) != DataMemberFlags.None; } }
	}

	internal unsafe class PDBReader
	{
		const string LibraryName = nameof(PDBReader) + ".dll";

		[DllImport(LibraryName)]
		internal static extern PDBHandle* LoadPDB([MarshalAs(UnmanagedType.LPWStr)] string path);

		[DllImport(LibraryName)]
		internal static extern void ClosePDB(PDBHandle* handle);

		[DllImport(LibraryName)]
		internal static extern void GetTypeRecordsInfo(
			PDBHandle* handle,
			out TPIRecord** Records,
			out ulong RecordCount,
			out uint TypeIndexBegin,
			out uint TypeIndexEnd
			);

		[DllImport(LibraryName)]
		internal static extern void GetBasicTypeInfo(
			PDBHandle* handle,
			uint typeIndex,
			out RecordBasicInfo info
			);

		[DllImport(LibraryName)]
		internal static extern void CountNumFields(
			PDBHandle* handle,
			TPIRecord* fieldListRecord,
			out StructFieldCounts counts
			);

		[DllImport(LibraryName)]
		internal static extern void ReadDataMembers(
			PDBHandle* handle,
			TPIRecord* fieldListRecord,
			[In, Out, MarshalAs(UnmanagedType.LPArray)] DataMemberInfo[] members
			);
	}
}
