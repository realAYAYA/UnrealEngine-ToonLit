/*!
 * @file    ProResProperties.h
 *
 * @brief   ProRes property API.
 *
 * Copyright (c) 2010-2016 Apple Inc. All rights reserved.
 */

#ifndef PRORES_PROPERTIES_H
#define PRORES_PROPERTIES_H

#include "ProResEncoder.h"

#ifdef __cplusplus
extern "C" {
#endif


/*! ProRes property identifiers. */
typedef enum {

    /*! The aspect_ratio_information field in the ProRes frame header. */
    kPRPropertyID_FrameHeaderAspectRatio            = 'fasp',  // int32_t value type

    /*! The frame_rate_code field in the ProRes frame header. */
    kPRPropertyID_FrameHeaderFrameRate              = 'ffps',  // int32_t value type

    /*! The color_primaries field in the ProRes frame header. */
    kPRPropertyID_FrameHeaderColorPrimaries         = 'fcol',  // int32_t value type

    /*! The transfer_characteristic field in the ProRes frame header. */
    kPRPropertyID_FrameHeaderTransferCharacteristic = 'fxfr',  // int32_t value type

    /*! The matrix_coefficients field in the ProRes frame header.
     *  Note when encoding from a RGB pixel format, this configures the Y'CbCr
     *  matrix that will be used for encoding.
     */
    kPRPropertyID_FrameHeaderMatrixCoefficients     = 'fmat'   // int32_t value type

} PRPropertyID;


/*!
 * The aspect_ratio_information field in the ProRes frame header can take on the
 * following values.  0 is the default.  Use with
 * kPRPropertyID_FrameHeaderFrameRate.
 */
enum {
    kPRAspectRatio_Unspecified  = 0,    //!< default (unspecified or other)
    kPRAspectRatio_SquarePixel  = 1,    //!< 1:1 pixel aspect ratio
    kPRAspectRatio_4x3          = 2,    //!< 4:3 image aspect ratio
    kPRAspectRatio_16x9         = 3     //!< 16:9 image aspect ratio
};


/*!
 * The frame_rate_code field in the ProRes frame header can take on the
 * following values.  0 is the default.  Use with
 * kPRPropertyID_FrameHeaderFrameRate.
 */
enum {
    kPRFrameRate_Unspecified    = 0,    //!< default (unspecified or other)
    kPRFrameRate_23976          = 1,    //!< 24/1.001 fps
    kPRFrameRate_24             = 2,    //!< 24 fps
    kPRFrameRate_25             = 3,    //!< 25 fps
    kPRFrameRate_2997           = 4,    //!< 30/1.001 fps
    kPRFrameRate_30             = 5,    //!< 30 fps
    kPRFrameRate_50             = 6,    //!< 50 fps
    kPRFrameRate_5994           = 7,    //!< 60/1.001 fps
    kPRFrameRate_60             = 8,    //!< 60 fps
    kPRFrameRate_100            = 9,    //!< 100 fps
    kPRFrameRate_11988          = 10,   //!< 120/1.001 fps
    kPRFrameRate_120            = 11    //!< 120 fps
};


/*!
 * The color_primaries field in the ProRes frame header can take on the
 * following values.  2 is the default.  Use with
 * kPRPropertyID_FrameHeaderColorPrimaries.
 */
enum {
    kPRColorPrimaries_Unspecified = 2,  //!< default (unspecified)
    kPRColorPrimaries_ITU_R_709   = 1,  //!< ITU-R BT.709
    kPRColorPrimaries_EBU_3213    = 5,  //!< ITU-R BT.601 625-line / EBU 3213
    kPRColorPrimaries_SMPTE_C     = 6,  //!< ITU-R BT.601 525-line / SMPTE C
    kPRColorPrimaries_ITU_R_2020  = 9,  //!< ITU-R BT.2020
    kPRColorPrimaries_DCI_P3      = 11, //!< P3 with DCI white point
    kPRColorPrimaries_P3_D65      = 12  //!< P3 with D65 white point
};


/*!
 * The transfer_characteristic field in the ProRes frame header can take on the
 * following values.  2 is the default.  Use with
 * kPRPropertyID_FrameHeaderTransferCharacteristic.
 */
enum {
    kPRTransferCharacteristic_Unspecified = 2,  //!< default (unspecified)
    kPRTransferCharacteristic_ITU_R_709   = 1,  //!< ITU-R BT.709 / BT.601 / BT.2020
    kPRTransferCharacteristic_ST_2084     = 16, //!< SMPTE ST 2084 (PQ)
    kPRTransferCharacteristic_HLG         = 18  //!< BT.2100 Hybrid Log Gamma
};


/*!
 * The matrix_coefficients field in the ProRes frame header can take on the
 * following values.  2 is the default.  Use with
 * kPRPropertyID_FrameHeaderMatrixCoefficients.
 */
enum {
    kPRMatrixCoefficients_Unspecified     = 2,  //!< default (unspecified)
    kPRMatrixCoefficients_ITU_R_709       = 1,  //!< ITU-R BT.709
    kPRMatrixCoefficients_ITU_R_601       = 6,  //!< ITU-R BT.601
    kPRMatrixCoefficients_ITU_R_2020      = 9   //!< ITU-R BT.2020 (NCL)
};


/*!
 * Sets a property on the specified encoder instance.
 *
 * @param encoder           The encoder instance.
 * @param propID            The property ID.
 * @param propValueSize     Size (in bytes) of the property value type.
 * @param propValueAddress  Pointer to the property value.
 *
 * @return  0 if successful;
 *         -50 if a parameter is invalid;
 *         -2184 if the size of the property value is incorrect;
 *         -2195 if the property is not supported.
 */
int
PRSetEncoderProperty(
    PREncoderRef encoder,
    PRPropertyID propID,
    unsigned int propValueSize,
    const void*  propValueAddress);


#ifdef __cplusplus
}
#endif

#endif // PRORES_PROPERTIES_H
