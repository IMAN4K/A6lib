/*
 * SMS PDU encoder, decoder for GSM + UCS2 coding scheme.
 * Copyright (C) 2018 Iman Ahmadvand
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
*/

#ifndef PDU_H
#define PDU_H

#include <stdint.h>
#include <inttypes.h>

///@cond INTERNAL
#define GSM_CODING_MAX_CHAR 160
#define UCS2_CODING_MAX_CHAR 70
#define PDU_MIN_LEN 9

/* error codes */
#define SMALL_INPUT_BUFF_ERR  -1
#define PDU_INVALID_ARG_ERR   -2
#define PDU_UNEXPECTED_ERR    -128
///@endcond

/*!
* \brief Encode input SMS \a text (which is coded in ASCII) into a SMS-SUBMIT pdu.
* \param sca a null terminated string contain SMS service center address
* \param phone a null terminated string contain destination phone number
* \param text the SMS content in ASCII
* \param text_len the number of chars in SMS content(could be up to 160 char long)
* \param pdu the input buffer which is going to hold the final pdu
* \param pdu_size the size of input pdu buffer
* \return if success a positive value represent number of pdu octets written, if fail a negative value represent error code
*/
int pdu_encode(const char* sca, const char* phone, const char* text, uint8_t text_len, uint8_t* pdu, uint8_t pdu_size);

/*!
* \brief Encode input SMS \a text (which is coded in UCS2) into a SMS-SUBMIT pdu.
* \param sca a null terminated string contain SMS service center address
* \param phone a null terminated string contain destination phone number
* \param text the SMS content coded in UCS2 coding scheme
* \param text_len the number of UCS2 chars in SMS content(could be up to 70 char long)
* \param pdu the input buffer which is going to hold the final pdu
* \param pdu_size the size of input pdu buffer
* \return if success a positive value represent number of pdu octets written, if fail a negative value represent error code
*/
int pdu_encodew(const char* sca, const char* phone, const uint16_t* text, uint8_t text_len, uint8_t* pdu, uint8_t pdu_size);

#endif // PDU_H
