// Copyright Epic Games, Inc. All Rights Reserved.

import java.io.*;
import java.util.*;

import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.Path;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;

import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;		

public class GenUniversalAPK
{
	public static boolean extractAPK(String zipFilename, String dest, String entryFilename)
	{
		try
		{
			File file = new File(zipFilename);
			FileInputStream fis = new FileInputStream(file);
			ZipInputStream zis = new ZipInputStream(new BufferedInputStream(fis));

			ZipEntry entry;
			while ((entry = zis.getNextEntry()) != null)
			{
				if (entry.isDirectory())
				{
					continue;
				}
				if (!entry.getName().equals(entryFilename))
				{
					continue;
				}

				File outfile = new File(dest);
				OutputStream outStream = new FileOutputStream(outfile);
				
				int fileLength = (int)entry.getSize();
				byte[] buffer = new byte[65536];
				int offset = 0;
				while (offset < fileLength)
				{
					int request = fileLength - offset;
					int size = zis.read(buffer, 0, request < 65536 ? request : 65536);
					outStream.write(buffer, 0, size);
					offset += size;
				}
				outStream.close();
				zis.close();
				fis.close();
				return true;
			}
			zis.close();
			fis.close();
		}
		catch (IOException e)
		{
			System.out.println("Exception: " + e);
		}
		return false;
	}
	
	public static void main(String[] args)
	{
		String apksFilename = "";
		String apkFilename = "";
	
		if (args.length < 2)
		{
			System.out.println("GenUniversalAPK v1.0\n");
			System.out.println("Usage: in_apks out_apk\n");
			System.exit(0);
		}

		apksFilename = args[0];
		apkFilename = args[1];

		Path apksPath = Paths.get(apksFilename);
		if (Files.notExists(apksPath))
		{
			System.out.println("ERROR: Unable to find AAB: " + apksFilename);
			System.exit(-1);
		}

		// extract the universal.apk if is an apks file
		System.out.println("Extracting universal.apk from " + apksFilename);
		if (!extractAPK(apksFilename, apkFilename, "universal.apk"))
		{
			System.out.println("ERROR: Failed to extract universal.apk from " + apksFilename);
			System.exit(-1);
		}
		
		System.out.println("APK successfully extracted.");
		System.exit(0);
	}
}