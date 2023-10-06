/* SPDX-License-Identifier: MIT */
/**
	@file		ancillarydata_hdr_hlg.h
	@brief		Declares the AJAAncillaryData_HDR_HLG class.
	@copyright	(C) 2012-2021 AJA Video Systems, Inc.
**/

#ifndef AJA_ANCILLARYDATA_HDR_HLG_H
#define AJA_ANCILLARYDATA_HDR_HLG_H

#include "ancillarydatafactory.h"
#include "ancillarydata.h"

const uint8_t	AJAAncillaryData_HDR_HLG_DID = 0xC0;
const uint8_t	AJAAncillaryData_HDR_HLG_SID = 0x00;


/**
	@brief	This class handles "5251" Frame Status Information packets.
**/
class AJA_EXPORT AJAAncillaryData_HDR_HLG : public AJAAncillaryData
{
public:
	AJAAncillaryData_HDR_HLG ();	///< @brief	My default constructor.

	/**
		@brief	My copy constructor.
		@param[in]	inClone	The AJAAncillaryData object to be cloned.
	**/
	AJAAncillaryData_HDR_HLG (const AJAAncillaryData_HDR_HLG & inClone);

	/**
		@brief	My copy constructor.
		@param[in]	pInClone	A valid pointer to the AJAAncillaryData object to be cloned.
	**/
	AJAAncillaryData_HDR_HLG (const AJAAncillaryData_HDR_HLG * pInClone);

	/**
		@brief	Constructs me from a generic AJAAncillaryData object.
		@param[in]	pInData	A valid pointer to the AJAAncillaryData object.
	**/
	AJAAncillaryData_HDR_HLG (const AJAAncillaryData * pInData);

	virtual									~AJAAncillaryData_HDR_HLG ();	///< @brief		My destructor.

	virtual void							Clear (void);								///< @brief	Frees my allocated memory, if any, and resets my members to their default values.

	/**
		@brief	Assignment operator -- replaces my contents with the right-hand-side value.
		@param[in]	inRHS	The value to be assigned to me.
		@return		A reference to myself.
	**/
	virtual AJAAncillaryData_HDR_HLG &			operator = (const AJAAncillaryData_HDR_HLG & inRHS);


	virtual inline AJAAncillaryData_HDR_HLG *	Clone (void) const	{return new AJAAncillaryData_HDR_HLG (this);}	///< @return	A clone of myself.

	/**
		@brief		Parses out (interprets) the "local" ancillary data from my payload data.
		@return		AJA_STATUS_SUCCESS if successful.
	**/
	virtual AJAStatus						ParsePayloadData (void);

	/**
		@brief		Streams a human-readable representation of me to the given output stream.
		@param		inOutStream		Specifies the output stream.
		@param[in]	inDetailed		Specify 'true' for a detailed representation;  otherwise use 'false' for a brief one.
		@return		The given output stream.
	**/
	virtual std::ostream &					Print (std::ostream & inOutStream, const bool inDetailed = false) const;

	/**
		@param[in]	pInAncData	A valid pointer to a base AJAAncillaryData object that contains the Anc data to inspect.
		@return		AJAAncillaryDataType if I recognize this Anc data (or AJAAncillaryDataType_Unknown if unrecognized).
	**/
	static AJAAncillaryDataType				RecognizeThisAncillaryData (const AJAAncillaryData * pInAncData);


protected:
	void		Init (void);	// NOT virtual - called by constructors

};	//	AJAAncillaryData_HDR_HLG

#endif	// AJA_ANCILLARYDATA_HDR_HLG_H
