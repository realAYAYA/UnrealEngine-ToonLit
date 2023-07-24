/* SPDX-License-Identifier: MIT */
/**
	@file		ntv2firmwareinstallerthread.h
	@brief		Declaration of CNTV2FirmwareInstallerThread class.
	@copyright	(C) 2014-2021 AJA Video Systems, Inc.  All rights reserved.
**/
#ifndef __NTV2FIRMWAREINSTALLERTHREAD_H__
#define __NTV2FIRMWAREINSTALLERTHREAD_H__

#include "ntv2card.h"
#include "ntv2devicescanner.h"
#include "ajabase/system/thread.h"


const int		kFirmwareUpdateCheckFailed	(999);


/**
	@brief		Compares the on-device firmware with the device's corresponding on-disk bitfile firmware, and indicates
				if an update is required or not.
	@note		This function looks in the host-specific "well known location" for the device's firmware file.
	@param[in]	inDeviceInfo	A constant reference to a valid NTV2DeviceInfo struct that represents an attached and
								powered-on device.
	@return		Zero (false) if the on-device firmware date exactly matches the on-disk bitfile firmware date.
				+1 if the on-device firmware date is newer than the on-disk bitfile firmware date.
				-1 if the on-device firmware date is older than the on-disk bitfile firmware date.
				kFirmwareUpdateCheckFailed if unable to compare due to an error.
**/
int NeedsFirmwareUpdate (const NTV2DeviceInfo & inDeviceInfo);


/**
	@brief		Compares the on-device firmware with the device's corresponding on-disk bitfile firmware, and indicates
				if an update is required or not.
	@note		This function looks in the host-specific "well known location" for the device's firmware file.
	@param[in]	inDeviceInfo	A constant reference to a valid NTV2DeviceInfo struct that represents an attached and
								powered-on device.
	@param[out]	outInfo			Receives a human-readable string that explains why the result was non-zero.
	@return		Zero (false) if the on-device firmware date exactly matches the on-disk bitfile firmware date.
				+1 if the on-device firmware date is newer than the on-disk bitfile firmware date.
				-1 if the on-device firmware date is older than the on-disk bitfile firmware date.
				kFirmwareUpdateCheckFailed if unable to compare due to an error.
**/
int NeedsFirmwareUpdate (const NTV2DeviceInfo & inDeviceInfo, std::string & outInfo);


/**
	@brief		I am an AJAThread that installs firmware from a given bitfile on the local host into a specific AJA device.
	@details	To use, simply construct with an NTV2DeviceInfo that identifies the target device, and a string containing
				a path (relative or absolute) to the bitfile, then call ThreadRun. You can periodically poll it by calling
				Active(), or ask for a human-readable status string by calling GetStatusString(). When finished, call
				IsUpdateSuccessful() to find out if the installation succeeded.
**/
class CNTV2FirmwareInstallerThread : public AJAThread
{
	//	Instance Methods
	public:
		/**
			@brief		Constructs me for a given device and bitfile.
			@param[in]	inDeviceInfo	Specifies the device to be flashed.
			@param[in]	inBitfilePath	Specifies the path (relative or absolute) to the firmware bitfile to install.
			@param[in]	inVerbose		If true (the default), emit detailed messages to the standard output stream when
										flashing has started and when it has successfully completed; otherwise, don't.
		**/
		explicit									CNTV2FirmwareInstallerThread (const NTV2DeviceInfo & inDeviceInfo,
																				  const std::string & inBitfilePath,
																				  const bool inVerbose = true,
																				  const bool inForce = false);
	
		explicit									CNTV2FirmwareInstallerThread (const NTV2DeviceInfo & inDeviceInfo,
																					const std::string & inDRFilesPath,
																					const NTV2DeviceID inDesiredID,
																					const bool inVerbose);
	
		virtual inline								~CNTV2FirmwareInstallerThread ()				{}

		/**
			@brief		Starts the thread to erase, flash and verify the firmware update.
			@return		AJA_STATUS_SUCCESS if successful; otherwise other values upon failure.
		**/
		virtual AJAStatus							ThreadRun (void);

		//	Accessors
		/**
			@return		A constant reference to a string containing the bitfile path that was specified when I was constructed.
		**/
		virtual inline const std::string &			GetBitfilePath (void) const				{return m_bitfilePath;}

		/**
			@return		True only if the device was successfully flashed.
		**/
		virtual inline bool							IsUpdateSuccessful (void) const			{return m_updateSuccessful;}

		/**
			@return		A string containing a human-readable status message based on the current installation state.
		**/
		virtual std::string							GetStatusString (void) const;

		/**
			@return		An integer value representing the current progress of the current in-progress installation.
						Divide this value by the result of GetProgressMax() to obtain a percentage.
			@note		This function should only be called if the thread is still active.
		**/
		virtual uint32_t							GetProgressValue (void) const;

		/**
			@return		An integer value representing the maximum progress value of the current in-progress installation.
						Divide this value into the result of GetProgressValue() to obtain a percentage.
			@note		This function should only be called if the thread is still active.
		**/
		virtual uint32_t							GetProgressMax (void) const;

	protected:
		virtual void								InternalUpdateStatus (void) const;
		/**
			@return		True if the MCS bitfile can be flashed onto the IP device; otherwise false.
		**/
		virtual bool								ShouldUpdateIPDevice (const NTV2DeviceID inDeviceID, const std::string & designName) const;

	private:
													CNTV2FirmwareInstallerThread ();											//	hidden
													CNTV2FirmwareInstallerThread (const CNTV2FirmwareInstallerThread & inObj);	//	hidden
		virtual CNTV2FirmwareInstallerThread &		operator = (const CNTV2FirmwareInstallerThread & inObj);					//	hidden

	//	Instance Data
	protected:
		const NTV2DeviceInfo						m_deviceInfo;			///< @brief	Device info passed to me at construction
		NTV2DeviceID								m_desiredID;			///< @brief Desired dynamic reconfig profile
		std::string									m_bitfilePath;			///< @brief	Absolute path to bitfile on host that's to be flashed into device
		std::string									m_drFilesPath;			///< @brief Path to group of dynamic reconfig files
		bool										m_updateSuccessful;		///< @brief	Initially False, is set True if firmware successfully installed
		const bool									m_verbose;				///< @brief	Verbose logging to cout/cerr?
		const bool									m_forceUpdate;			///< @brief	Force the install of the bitfile
		const bool									m_useDynamicReconfig;	///< @brief Use dyanmic reconfig
		mutable CNTV2Card							m_device;				///< @brief	Talks to the AJA device
		mutable SSC_GET_FIRMWARE_PROGRESS_STRUCT	m_statusStruct;			///< @brief	Firmware update progress

};	//	CNTV2FirmwareInstallerThread

#endif	//	__NTV2FIRMWAREINSTALLERTHREAD_H__
