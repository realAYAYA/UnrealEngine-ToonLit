// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.IO;
using System.Xml.Serialization;

namespace MemoryProfiler2
{
    /// <summary> Encapsulates module information. </summary>
	public class FModuleInfo
    {
        public ulong BaseOfImage;
        public uint ImageSize;
        public uint TimeDateStamp;
        public string ModuleName;
        public string ImageName;
        public string LoadedImageName;
        public uint PdbSig;
        public uint PdbAge;

        /// <summary> Serializing constructor. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		public FModuleInfo(BinaryReader BinaryStream)
        {
            BaseOfImage = BinaryStream.ReadUInt64();
            ImageSize = BinaryStream.ReadUInt32();
            TimeDateStamp = BinaryStream.ReadUInt32();
            PdbSig = BinaryStream.ReadUInt32();
            PdbAge = BinaryStream.ReadUInt32();
            uint v;
            v = BinaryStream.ReadUInt32(); // data1
            v = BinaryStream.ReadUInt16(); // data2
            v = BinaryStream.ReadUInt16(); // data3
            v = BinaryStream.ReadUInt32(); // data4.0
            v = BinaryStream.ReadUInt32(); // data4.4
            ModuleName = FStreamParser.ReadString(BinaryStream);
            ImageName = FStreamParser.ReadString(BinaryStream);
            LoadedImageName = FStreamParser.ReadString(BinaryStream);

            Debug.WriteLine("Loaded Module:" + ModuleName);
            Debug.WriteLine("ImageName:" + ImageName);
            Debug.WriteLine("LoadedImageName:" + LoadedImageName);
            Debug.WriteLine("BaseOfImage:" + BaseOfImage.ToString());
            Debug.WriteLine("ImageSize:" + ImageSize.ToString());
        }
    }
}

