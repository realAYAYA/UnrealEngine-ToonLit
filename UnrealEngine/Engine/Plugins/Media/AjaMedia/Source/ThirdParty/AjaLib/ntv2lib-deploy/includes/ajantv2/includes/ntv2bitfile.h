/* SPDX-License-Identifier: MIT */
/**
	@file		ntv2bitfile.h
	@brief		Declares the CNTV2Bitfile class.
	@copyright	(C) 2010-2021 AJA Video Systems, Inc.    All rights reserved.
**/

#ifndef NTV2BITFILE_H
#define NTV2BITFILE_H

#include <fstream>
#ifdef AJALinux
	#include <stdint.h>
	#include <stdlib.h>
#endif
#include "ajatypes.h"
#include "ajaexport.h"
#include "ntv2enums.h"
#include "ntv2publicinterface.h"
#include "ntv2utils.h"


/**
	@brief	Knows how to extract information from a bitfile header.
**/
class AJAExport NTV2BitfileHeaderParser
{
	public:
		explicit					NTV2BitfileHeaderParser	()	{Clear();}
		bool						ParseHeader			(const NTV2_POINTER & inHdrBuffer,
														std::ostream & outMsgs);
		void						Clear				(void);

		inline ULWord				UserID				(void) const		{return mUserID;}
		inline const std::string &	Date				(void) const		{return mDate;}
		inline const std::string &	Time				(void) const		{return mTime;}
		inline const std::string &	PartName			(void) const		{return mPartName;}
		inline const std::string &	RawDesign			(void) const		{return mRawDesignName;}
		std::string					DesignName			(void) const;

		inline ULWord				DesignID			(void) const		{return mDesignID;}
		inline ULWord				DesignVersion		(void) const		{return mDesignVersion;}
		inline ULWord				BitfileID			(void) const		{return mBitfileID;}
		inline ULWord				BitfileVersion		(void) const		{return mBitfileVersion;}
		inline ULWord				ProgramOffsetBytes	(void) const		{return mProgOffsetBytes;}
		inline ULWord				ProgramSizeBytes	(void) const		{return mProgSizeBytes;}
		inline bool					IsValid				(void) const		{return mValid;}
		inline bool					IsTandem			(void) const		{return RawDesign().find("TANDEM=TRUE")   != std::string::npos;}
		inline bool					IsPartial			(void) const		{return RawDesign().find("PARTIAL=TRUE")  != std::string::npos;}
		inline bool					IsClear				(void) const		{return RawDesign().find("CLEAR=TRUE")    != std::string::npos;}
		inline bool					IsCompress			(void) const		{return RawDesign().find("COMPRESS=TRUE") != std::string::npos;}

	public:	//	Class Methods
		static inline ULWord		GetDesignID			(const ULWord userID)		{ return (userID & 0xff000000) >> 24; }
		static inline ULWord		GetDesignVersion	(const ULWord userID)		{ return (userID & 0x00ff0000) >> 16; }
		static inline ULWord		GetBitfileID		(const ULWord userID)		{ return (userID & 0x0000ff00) >> 8; }
		static inline ULWord		GetBitfileVersion	(const ULWord userID)		{ return (userID & 0x000000ff) >> 0; }

	protected:
		bool						SetRawDesign			(const std::string & inStr, std::ostream & oss);
		bool						SetDate					(const std::string & inStr, std::ostream & oss);
		bool						SetTime					(const std::string & inStr, std::ostream & oss);
		inline void					SetPartName				(const std::string & inStr)		{mPartName = inStr;}
		bool						SetProgramOffsetBytes	(const ULWord inValue, std::ostream & oss);
		bool						SetProgramSizeBytes		(const ULWord inValue, std::ostream & oss);

	private:
		std::string		mDate;				//	Compile/build date
		std::string		mTime;				//	Compile/build time
		std::string		mPartName;			//	Part name
		std::string		mRawDesignName;		//	Untruncated, unmodified design string
		ULWord			mUserID;			//	User ID
		ULWord			mProgOffsetBytes;	//	Program offset, in bytes
		ULWord			mProgSizeBytes;		//	Program length, in bytes
		ULWord			mDesignID;			//	Design ID
		ULWord			mDesignVersion;		//	Design version
		ULWord			mBitfileID;			//	Bitfile ID
		ULWord			mBitfileVersion;	//	Bitfile version
		bool			mValid;				//	Is valid?
};	//	NTV2BitfileHeaderParser


/**
	@brief	Instances of me can parse a bitfile.
**/
class AJAExport CNTV2Bitfile
{
	public:
		/**
			@brief		My constructor.
		**/
											CNTV2Bitfile ();

		/**
			@brief		My destructor.
		**/
		virtual								~CNTV2Bitfile ();

		/**
			@brief		Opens the bitfile at the given path, then parses its header.
			@param[in]	inBitfilePath	Specifies the path name of the bitfile to be parsed.
			@return		True if open & parse succeeds; otherwise false.
		**/
		virtual bool						Open (const std::string & inBitfilePath);

		#if !defined (NTV2_DEPRECATE)
			virtual NTV2_DEPRECATED_f(bool	Open (const char * const & inBitfilePath));	///< @deprecated	Use the std::string version of Open instead.
		#endif	//	!defined (NTV2_DEPRECATE)

		/**
			@brief	Closes bitfile (if open) and resets me.
		**/
		virtual void						Close (void);

		/**
			@brief		Parse a bitfile header that's stored in a buffer.
			@param[in]	inBitfileBuffer	Specifies the buffer of the bitfile to be parsed.
			@param[in]	inBufferSize	Specifies the size of the buffer to be parsed.
			@return		A std::string containing parsing errors. It will be empty if successful.
		**/
		virtual std::string					ParseHeaderFromBuffer (const uint8_t* inBitfileBuffer, const size_t inBufferSize);

		/**
			@brief		Parse a bitfile header that's stored in a buffer.
			@param[in]	inBitfileBuffer	Specifies the buffer of the bitfile to be parsed.
			@return		A std::string containing parsing errors. It will be empty if successful.
		**/
		virtual std::string					ParseHeaderFromBuffer (const NTV2_POINTER & inBitfileBuffer);

		/**
			@return		A string containing the extracted bitfile build date.
		**/
		virtual inline const std::string &	GetDate (void) const			{return mHeaderParser.Date();}

		/**
			@return		A string containing the extracted bitfile build time.
		**/
		virtual inline const std::string &	GetTime (void) const			{return mHeaderParser.Time();}

		/**
			@return		A string containing the extracted bitfile design name.
		**/
		virtual inline std::string			GetDesignName (void) const		{return mHeaderParser.DesignName();}

		/**
			@return		A string containing the extracted bitfile part name.
		**/
		virtual inline const std::string &	GetPartName (void) const		{return mHeaderParser.PartName();}

		/**
			@return		A string containing the error message, if any, from the last function that could fail.
		**/
		virtual inline const std::string &	GetLastError (void) const		{ return mLastError; }

		/**
			@return		True if the bitfile header includes tandem flag; otherwise false.
		**/
		virtual inline bool		IsTandem (void) const						{return mHeaderParser.IsTandem();}

		/**
			@return		True if the bitfile header includes partial flag; otherwise false.
		**/
		virtual inline bool		IsPartial (void) const						{return mHeaderParser.IsPartial();}

		/**
			@return		True if the bitfile header includes clear flag; otherwise false.
		**/
		virtual inline bool		IsClear (void) const						{return mHeaderParser.IsClear();}

		/**
			@return		True if the bitfile header includes compress flag; otherwise false.
		**/
		virtual inline bool		IsCompress (void) const						{return mHeaderParser.IsCompress();}

		/**
			@return		A ULWord containing the design design ID as extracted from the bitfile.
		**/
		virtual inline ULWord	GetDesignID (void) const					{return mHeaderParser.DesignID();}

		/**
			@return		A ULWord containing the design version as extracted from the bitfile.
		**/
		virtual inline ULWord	GetDesignVersion (void) const				{return mHeaderParser.DesignVersion();}

		/**
			@return		A ULWord containing the design ID as extracted from the bitfile.
		**/
		virtual inline ULWord	GetBitfileID (void) const					{return mHeaderParser.BitfileID();}

		/**
			@return		A ULWord containing the design version as extracted from the bitfile.
		**/
		virtual inline ULWord	GetBitfileVersion (void) const				{return mHeaderParser.BitfileVersion();}

		/**
			@brief		Answers with the design user ID, as extracted from the bitfile.
			@return		A ULWord containing the design user ID.
		**/
		virtual inline ULWord	GetUserID (void) const						{return mHeaderParser.UserID();}

		/**
			@return		True if the bitfile can be flashed onto the device; otherwise false.
		**/
		virtual bool			CanFlashDevice (const NTV2DeviceID inDeviceID) const;

		/**
			@return		My instrinsic NTV2DeviceID.
		**/
		virtual NTV2DeviceID	GetDeviceID (void) const;

		/**
			@return		Program stream length in bytes, or zero if error/invalid.
		**/
		virtual inline size_t	GetProgramStreamLength (void) const		{return mReady ? size_t(mHeaderParser.ProgramSizeBytes()) : 0;}

		/**
			@return		File stream length in bytes, or zero if error/invalid.
		**/
		virtual inline size_t	GetFileStreamLength (void) const		{return mReady ? mFileSize : 0;}
		
		/**
			@brief		Retrieves the program bitstream.
			@param[out]	outBuffer	Specifies the buffer that will receive the data.  This method will Allocate
									storage if IsNULL or IsAllocatedBySDK.
			@return		Number of bytes copied to the outBuffer, or zero upon failure.
		**/
		virtual size_t			GetProgramByteStream (NTV2_POINTER & outBuffer);

		/**
			@brief		Retrieves the file bitstream.
			@param[out]	outBuffer	Specifies the buffer that will receive the data.  This method will Allocate
									storage if IsNULL or IsAllocatedBySDK.
			@return		Number of bytes copied to the outBuffer, or zero upon failure.
		**/
		virtual size_t			GetFileByteStream (NTV2_POINTER & outBuffer);

	public:	//	Class Methods
		static NTV2DeviceID		ConvertToDeviceID	(const ULWord inDesignID, const ULWord inBitfileID);
		static ULWord			ConvertToDesignID	(const NTV2DeviceID inDeviceID);
		static ULWord			ConvertToBitfileID	(const NTV2DeviceID inDeviceID);
		static std::string		GetPrimaryHardwareDesignName (const NTV2DeviceID inDeviceID);
		static NTV2DeviceID		GetDeviceIDFromHardwareDesignName (const std::string & inDesignName);

	protected:	//	Protected Methods
		virtual void			SetLastError (const std::string & inStr, const bool inAppend = false);

	private:	//	Private Member Data
		std::ifstream			mFileStream;	//	Binary input filestream
		NTV2_POINTER			mHeaderBuffer;	//	Header buffer in use
		NTV2BitfileHeaderParser	mHeaderParser;	//	Header parser (and state info)
		std::string				mLastError;		//	Last error message
		size_t					mFileSize;		//	Bitfile size, in bytes
		bool					mReady;			//	Ready?
};	//	CNTV2Bitfile

#endif // NTV2BITFILE_H
