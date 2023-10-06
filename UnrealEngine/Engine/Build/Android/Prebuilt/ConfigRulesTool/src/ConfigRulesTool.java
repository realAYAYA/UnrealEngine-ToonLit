// Copyright Epic Games, Inc. All Rights Reserved.

import java.io.*;
import java.util.*;
import java.util.zip.*;

import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.Path;

import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;
import javax.crypto.spec.SecretKeySpec;

public class ConfigRulesTool
{
	public static String CipherTransform = "AES/ECB/PKCS5Padding";
	
	public static void writeInt(FileOutputStream outStream, int value)
	{
		try
		{
			outStream.write((value >> 24) & 0xff);
			outStream.write((value >> 16) & 0xff);
			outStream.write((value >> 8) & 0xff);
			outStream.write(value & 0xff);
		}
		catch (Exception e)
		{
		}
	}
	
	public static SecretKey generateKey(String password)
	{
		byte[] salt = new byte[] { 0x23, 0x71, (byte)0xd3, (byte)0xa3, 0x30, 0x71, 0x63, (byte)0xe3 };
		try
		{
			SecretKey secret = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA1").generateSecret(new PBEKeySpec(password.toCharArray(), salt, 1000, 128));
			return new SecretKeySpec(secret.getEncoded(), "AES");
		}
		catch (Exception e)
		{
		}
		return new SecretKeySpec(salt, "AES");
	}
	
	public static void main(String[] args)
	{
		String inFilename = "configrules.txt";
		String outFilename = "configrules.bin.png";
		String key = "";
		int headerSize = 2 + 4 + 4;
		
		if (args.length == 0)
		{
			System.out.println("ConfigRulesTool v1.0\n");
			System.out.println("Usage: [op] inputfile outputfile [key]\n");
			System.out.println("\tc = compress and encrypt if key provided");
			System.out.println("\td = decrypt and decompress");
			System.exit(0);
		}
		
		if (args.length > 1)
		{
			inFilename = args[1];
		}
		if (args.length > 2)
		{
			outFilename = args[2];
		}
		if (args.length > 3)
		{
			key = args[3];
		}
		
		if (args[0].equals("c"))
		{
			byte[] bytesToCompress = null;
			Path path = Paths.get(inFilename);
			try
			{
				bytesToCompress = Files.readAllBytes(path);
			}
			catch (IOException e)
			{
				System.out.println("Unable to read file: " + inFilename);
				System.exit(-1);
			}
			int sizeUncompressed = bytesToCompress.length;

			int version = -1;
			try
			{
				int versionBytes = sizeUncompressed < 80 ? sizeUncompressed : 80;
				String versionLine = new String(bytesToCompress, 0, versionBytes, "UTF-8");
				if (versionLine.startsWith("// version:"))
				{
					int eolIndex = versionLine.indexOf("\r");
					int newIndex = versionLine.indexOf("\n");
					eolIndex = (eolIndex < 0) ? 1000 : eolIndex;
					newIndex = (newIndex < 0) ? 1000 : newIndex;
					eolIndex = (eolIndex < newIndex) ? eolIndex : newIndex;
					try
					{
						version = Integer.parseInt(versionLine.substring(11, eolIndex));
					}
					catch (Exception e)
					{
						System.out.println("Unable to read version: " + inFilename);
						System.exit(-1);
					}
				}
			}
			catch (UnsupportedEncodingException e)
			{
				System.out.println("Unable to read version: " + inFilename);
				System.exit(-1);
			}
			if (version == -1)
			{
				System.out.println("Unable to read version: " + inFilename);
				System.exit(-1);
			}
			
			Deflater deflater = new Deflater();
			deflater.setInput(bytesToCompress);
			deflater.finish();

			byte[] bytesCompressed = new byte[sizeUncompressed * 3];
			int sizeCompressed = deflater.deflate(bytesCompressed);

			// encrypt if key provided
			if (!key.equals(""))
			{
				try
				{
					Cipher cipher = Cipher.getInstance(CipherTransform);
					cipher.init(Cipher.ENCRYPT_MODE, generateKey(key));
					byte[] encrypted = cipher.doFinal(bytesCompressed, 0, sizeCompressed);
					bytesCompressed = encrypted;
					sizeCompressed = bytesCompressed.length;
				}
				catch (Exception e)
				{
					System.out.println("Unable to encrypt input file: " + inFilename);
					System.out.println(e.toString());
					System.exit(-1);
				}
			}

			File outFile = new File(outFilename);
			FileOutputStream fileOutStream = null;
			try
			{
				fileOutStream = new FileOutputStream(outFile);
				byte[] signature = new byte[2];
				signature[0] = (byte)0x39;
				signature[1] = (byte)0xd8;
				fileOutStream.write(signature);
				writeInt(fileOutStream, version);
				writeInt(fileOutStream, sizeUncompressed);
				fileOutStream.write(bytesCompressed, 0, sizeCompressed);
			}
			catch (IOException e)
			{
				System.out.println("Error writing file: " + outFilename);
				System.exit(-1);
			}
			try
			{
				if (fileOutStream != null)
				{
					fileOutStream.close();
				}
			}
			catch (IOException e)
			{
				System.out.println("Error writing file: " + outFilename);
				System.exit(-1);
			}

			System.out.println("Version: " + Integer.toString(version) + ", Compressed from " + Integer.toString(sizeUncompressed) +
				" bytes to " + Integer.toString(sizeCompressed + headerSize) + " bytes" + (key.equals("") ? "." : " and encrypted."));

			CRC32 crc = new CRC32();
			path = Paths.get(outFilename);
			byte[] bytesToCRC = null;
			try
			{
				bytesToCRC = Files.readAllBytes(path);
			}
			catch (IOException e)
			{
				System.out.println("Unable to read file: " + outFilename);
				System.exit(-1);
			}
			crc.update(bytesToCRC);
			System.out.println(String.format("CRC32: %08X", crc.getValue()));

			System.exit(0);
		}
	
		if (args[0].equals("d"))
		{
			byte[] bytesToDecompress = null;
			Path path = Paths.get(inFilename);
			try
			{
				bytesToDecompress = Files.readAllBytes(path);
			}
			catch (IOException e)
			{
				System.out.println("Unable to read file: " + inFilename);
				System.exit(-1);
			}
			int sizeCompressed = bytesToDecompress.length - headerSize;
			if (bytesToDecompress.length < headerSize)
			{
				System.out.println("Input file is invalid: " + inFilename);
				System.exit(-1);
			}
			
			ByteBuffer buffer = ByteBuffer.wrap(bytesToDecompress);
			int signature = buffer.getShort();
			if (signature != 0x39d8)
			{
				System.out.println("Input file signature is invalid: " + inFilename + ", " + Integer.toString(signature));
				System.exit(-1);
			}

			int version = buffer.getInt();
			int sizeUncompressed = buffer.getInt();

			// decrypt if key provided
			if (!key.equals(""))
			{
				try
				{
					Cipher cipher = Cipher.getInstance(CipherTransform);
					cipher.init(Cipher.DECRYPT_MODE, generateKey(key));
					byte[] decrypted = cipher.doFinal(bytesToDecompress, headerSize, sizeCompressed);
					sizeCompressed = decrypted.length;
					System.arraycopy(decrypted, 0, bytesToDecompress, headerSize, sizeCompressed);
				}
				catch (Exception e)
				{
					System.out.println("Unable to decrypt input file: " + inFilename);
					System.out.println(e.toString());
					System.exit(-1);
				}
			}
			
			byte[] bytesDecompressed = new byte[sizeUncompressed];
			try
			{
				Inflater inflater = new Inflater();
				inflater.setInput(bytesToDecompress, headerSize, sizeCompressed);
				int resultLength = inflater.inflate(bytesDecompressed);
				inflater.end();
				if (resultLength != sizeUncompressed)
				{
					System.out.println("Error decompressing (size mismatch) file: " + inFilename);
					System.exit(-1);
				}
			}
			catch (Exception e)
			{
				System.out.println("Error decompressing file: " + inFilename);
				System.exit(-1);
			}
			System.out.println("Version: " + Integer.toString(version) + ", Uncompressed size: " + Integer.toString(sizeUncompressed));

			File outFile = new File(outFilename);
			FileOutputStream fileOutStream = null;
			try
			{
				fileOutStream = new FileOutputStream(outFile);
				fileOutStream.write(bytesDecompressed);
			}
			catch (IOException e)
			{
				System.out.println("Error writing file: " + outFilename);
				System.exit(-1);
			}
			try
			{
				if (fileOutStream != null)
				{
					fileOutStream.close();
				}
			}
			catch (IOException e)
			{
				System.out.println("Error writing file: " + outFilename);
				System.exit(-1);
			}

			System.out.println("Wrote file: " + outFilename);
			System.exit(0);
		}
		
		System.out.println("Unknown op: " + args[0]);
		System.exit(-1);
	}
}