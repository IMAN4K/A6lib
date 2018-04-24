/*
* SMS-SUBMIT & SMS-DELIVER PDU encoder, decoder for GSM + UCS2 coding scheme.
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

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "pdu.h"

#define READ_BIT(arg, n) (((arg) >> (n)) & 0x01)
#define SET_BIT(arg, n) ((arg) |= (0x01 << (n)))
#define CLEAR_BIT(arg, n) ((arg) &= ~(0x01 << (n)))
#define LOW_NIBBLE(arg) ((arg) & 0x0F)
#define HIGTH_NIBBLE(arg) (((arg) & 0xF0) >> 4)
#define SWAP_NIBBLE(arg) ((HIGTH_NIBBLE(arg) << 4) | (LOW_NIBBLE(arg)))
#define TO_CHAR(arg) (char)(arg + 48)
#define TO_HEX(arg) (uint8_t)(arg - 48)

/* reverse string <in> of size <len> to hex <out> */
bool str_reverse(const char* in, uint8_t len, uint8_t* out) {
	if (in == NULL || out == NULL || len == 0)
		return false;

	for (uint8_t i = 0; i < len;) {
		uint8_t f = TO_HEX(in[i++]);
		uint8_t s = TO_HEX(in[i++]) << 4;
		*out++ = s | f;
	}

	return true;
}

/* convert ascii input string to 7-bit GSM alphabet */
int ascii_to_gsm(const char* in, uint8_t len, uint8_t* out) {
	if (in == NULL || out == NULL || len == 0)
		return -1;

	uint8_t bytes_written = 0;
	uint16_t bit_count = 0;
	uint16_t bit_queue = 0;
	while (len--) {
		bit_queue |= (*in & 0x7F) << bit_count;
		bit_count += 7;
		if (bit_count >= 8) {
			*out++ = (uint8_t)bit_queue;
			bytes_written++;
			bit_count -= 8;
			bit_queue >>= 8;
		}
		in++;
	}

	if (bit_count > 0) {
		*out++ = (uint8_t)bit_queue;
		bytes_written++;
	}

	return bytes_written;
}

/*!
* \brief pdu_encode Encode input SMS \a text (which is coded in ASCII) into a SMS-SUBMIT pdu.
* \param sca a null terminated string contain SMS service center address
* \param phone a null terminated string contain destination phone number
* \param text the SMS content in ASCII
* \param text_len the number of chars in SMS content(could be up to 160 char long)
* \param pdu the input buffer which is going to hold the final pdu
* \param pdu_size the size of input pdu buffer
* \return if success a positive value represent number of pdu octets written, if fail a negative value represent error code
*/
int pdu_encode(const char* sca, const char* phone, const char* text, uint8_t text_len, uint8_t* pdu, uint8_t pdu_size) {
	if (sca == NULL || phone == NULL || text == NULL || pdu == NULL || pdu_size < PDU_MIN_LEN)
		return PDU_INVALID_ARG_ERR;

	uint8_t indx = 0;
	
	uint8_t sca_len = strlen(sca);
	if (sca_len == 0)
		return PDU_INVALID_ARG_ERR;
	
	char *r_sca = malloc(sca_len + 2);
	if (r_sca == NULL)
		return PDU_UNEXPECTED_ERR;

	memcpy(r_sca, sca, sca_len);
	r_sca[sca_len + 1] = 0;
	if (sca_len % 2 == 0) { // no fill bits
		r_sca[sca_len] = 0;
		pdu[indx++] = sca_len / 2 + 1;
		pdu[indx++] = 0x91; // type of address
		str_reverse(r_sca, sca_len, pdu + indx);
		indx += sca_len / 2;
	} else { // append fill bits
		r_sca[sca_len] = 63; // 63 - 48 = 0x0F
		pdu[indx++] = sca_len / 2 + 2;
		pdu[indx++] = 0x91; // type of address
		str_reverse(r_sca, sca_len + 1, pdu + indx);
		indx += (sca_len + 1) / 2;
	}
	free(r_sca);
	pdu[indx++] = 0x11; // pdu type
	pdu[indx++] = 0x00;

	/* build DA into PDU */
	uint8_t phone_len = strlen(phone);
	if (phone_len == 0)
		return PDU_INVALID_ARG_ERR;

	char* r_phone = malloc(phone_len + 2);
	if (r_phone == NULL)
		return PDU_UNEXPECTED_ERR;

	memcpy(r_phone, phone, phone_len);
	r_phone[phone_len + 1] = 0;
	if (phone_len % 2 == 0) { // no fill bits
		r_phone[phone_len] = 0;
		pdu[indx++] = phone_len;
		pdu[indx++] = 0x91; // type of address
		str_reverse(r_phone, phone_len, pdu + indx);
		indx += phone_len / 2;
	} else { // append fill bits
		r_phone[phone_len] = 63; // 63 - 48 = 0x0F
		pdu[indx++] = phone_len;
		pdu[indx++] = 0x91; // type of address
		str_reverse(r_phone, phone_len + 1, pdu + indx);
		indx += (phone_len + 1) / 2;
	}
	free(r_phone);
	pdu[indx++] = 0x00; // TP-PID
	pdu[indx++] = 0x00; // DCS -> defualt GSM alphabet
	pdu[indx++] = 0x81; // TP-VP -> 0x81 * 5min
	pdu[indx++] = text_len; // UDHL -> number of chars

	/* convert text from ASCII to GSM alphabet representation */
	int octets = ascii_to_gsm(text, text_len, pdu + indx);
	indx += octets;

	return indx;
}

/*!
* \brief pdu_encodew Encode input SMS \a text (which is coded in UCS2) into a SMS-SUBMIT pdu.
* \param sca a null terminated string contain SMS service center address
* \param phone a null terminated string contain destination phone number
* \param text the SMS content coded in UCS2 coding scheme
* \param text_len the number of UCS2 chars in SMS content(could be up to 70 char long)
* \param pdu the input buffer which is going to hold the final pdu
* \param pdu_size the size of input pdu buffer
* \return if success a positive value represent number of pdu octets written, if fail a negative value represent error code
*/
int pdu_encodew(const char* sca, const char* phone, const wchar_t* text, uint8_t text_len, uint8_t* pdu, uint8_t pdu_size) {
	if (sca == NULL || phone == NULL || text == NULL || pdu == NULL || pdu_size < PDU_MIN_LEN)
		return PDU_INVALID_ARG_ERR;

	uint8_t indx = 0;

	uint8_t sca_len = strlen(sca);
	if (sca_len == 0)
		return PDU_INVALID_ARG_ERR;

	char *r_sca = malloc(sca_len + 2);
	if (r_sca == NULL)
		return PDU_UNEXPECTED_ERR;

	memcpy(r_sca, sca, sca_len);
	r_sca[sca_len + 1] = 0;
	if (sca_len % 2 == 0) { // no fill bits
		r_sca[sca_len] = 0;
		pdu[indx++] = sca_len / 2 + 1;
		pdu[indx++] = 0x91; // type of address
		str_reverse(r_sca, sca_len, pdu + indx);
		indx += sca_len / 2;
	} else { // append fill bits
		r_sca[sca_len] = 63; // 63 - 48 = 0x0F
		pdu[indx++] = sca_len / 2 + 2;
		pdu[indx++] = 0x91; // type of address
		str_reverse(r_sca, sca_len + 1, pdu + indx);
		indx += (sca_len + 1) / 2;
	}
	free(r_sca);
	pdu[indx++] = 0x11; // pdu type
	pdu[indx++] = 0x00;

	/* build DA into PDU */
	uint8_t phone_len = strlen(phone);
	if (phone_len == 0)
		return PDU_INVALID_ARG_ERR;

	char* r_phone = malloc(phone_len + 2);
	if (r_phone == NULL)
		return PDU_UNEXPECTED_ERR;

	memcpy(r_phone, phone, phone_len);
	r_phone[phone_len + 1] = 0;
	if (phone_len % 2 == 0) { // no fill bits
		r_phone[phone_len] = 0;
		pdu[indx++] = phone_len;
		pdu[indx++] = 0x91; // type of address
		str_reverse(r_phone, phone_len, pdu + indx);
		indx += phone_len / 2;
	} else { // append fill bits
		r_phone[phone_len] = 63; // 63 - 48 = 0x0F
		pdu[indx++] = phone_len;
		pdu[indx++] = 0x91; // type of address
		str_reverse(r_phone, phone_len + 1, pdu + indx);
		indx += (phone_len + 1) / 2;
	}
	free(r_phone);
	pdu[indx++] = 0x00; // TP-PID
	pdu[indx++] = 0x08; // DCS -> defualt GSM alphabet
	pdu[indx++] = 0x81; // TP-VP -> 0x81 * 5min
	pdu[indx++] = text_len * 2; // UDHL -> number of chars

	/* add UCS2 content as is */
	for (size_t i = 0; i < text_len; i++) {
		wchar_t w = text[i];
		pdu[indx++] = w >> 8;
		pdu[indx++] = w;
	}

	return indx;
}
