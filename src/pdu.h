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
#include <wchar.h>

#define GSM_CODING_MAX_CHAR 160
#define UCS2_CODING_MAX_CHAR 70
#define PDU_MIN_LEN 9

/* error codes */
#define SMALL_INPUT_BUFF_ERR  -1
#define PDU_INVALID_ARG_ERR   -2
#define PDU_UNEXPECTED_ERR    -128

int pdu_encode(const char* sca, const char* phone, const char* text, uint8_t text_len, uint8_t* pdu, uint8_t pdu_size);

int pdu_encodew(const char* sca, const char* phone, const wchar_t* text, uint8_t text_len, uint8_t* pdu, uint8_t pdu_size);

#endif // PDU_H
