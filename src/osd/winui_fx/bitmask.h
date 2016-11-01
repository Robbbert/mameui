// license:BSD-3-Clause
// copyright-holders:Chris Kirmse, Mike Haaland, René Single, Mamesick

#pragma once 
 
#ifndef BITMASK_H
#define BITMASK_H

/* Bit array type */
typedef struct
{
	UINT m_nSize;
	UCHAR* m_lpBits;
} BITS, *LPBITS;

/* Linked list type */
typedef struct
{
	LPVOID data;
	LPVOID next;
	LPVOID prev;
} NODE, *LPNODE;

/* Bit functions */
LPBITS NewBits(UINT nLength);	 /* in bits */
void DeleteBits(LPBITS lpBits);
bool TestBit(LPBITS lpBits, UINT nBit);
void SetBit(LPBITS lpBits, UINT nBit);
void ClearBit(LPBITS lpBits, UINT nBit);
void SetAllBits(LPBITS lpBits, bool bSet);
int FindBit(LPBITS lpBits, int nStartPos, bool bSet);

#endif
