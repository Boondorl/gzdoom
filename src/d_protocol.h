/*
** d_protocol.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __D_PROTOCOL_H__
#define __D_PROTOCOL_H__

#include "doomtype.h"
#include "i_protocol.h"

// The IFF routines here all work with big-endian IDs, even if the host
// system is little-endian.
#define BIGE_ID(a,b,c,d)	((d)|((c)<<8)|((b)<<16)|((a)<<24))

#define FORM_ID		BIGE_ID('F','O','R','M')
#define ZDEM_ID		BIGE_ID('Z','D','E','M')
#define ZDHD_ID		BIGE_ID('Z','D','H','D')
#define VARS_ID		BIGE_ID('V','A','R','S')
#define UINF_ID		BIGE_ID('U','I','N','F')
#define COMP_ID		BIGE_ID('C','O','M','P')
#define BODY_ID		BIGE_ID('B','O','D','Y')
#define NETD_ID		BIGE_ID('N','E','T','D')
#define WEAP_ID		BIGE_ID('W','E','A','P')

constexpr int MoreButtons = 0x80;

struct zdemoheader_s {
	uint8_t	demovermajor;
	uint8_t	demoverminor;
	uint8_t	minvermajor;
	uint8_t	minverminor;
	uint8_t	map[8];
	unsigned int rngseed;
	uint8_t	consoleplayer;
};

struct usercmd_t
{
	uint32_t	buttons;
	short	pitch;			// up/down
	short	yaw;			// left/right
	short	roll;			// "tilt"
	short	forwardmove;
	short	sidemove;
	short	upmove;
};

// When transmitted, the above message is preceded by a byte
// indicating which fields are actually present in the message.
enum
{
	UCMDF_BUTTONS		= 0x01,
	UCMDF_PITCH			= 0x02,
	UCMDF_YAW			= 0x04,
	UCMDF_FORWARDMOVE	= 0x08,
	UCMDF_SIDEMOVE		= 0x10,
	UCMDF_UPMOVE		= 0x20,
	UCMDF_ROLL			= 0x40,
};

class UserCommandPacket;

void StartChunk (int id, TArrayView<uint8_t>& stream);
void FinishChunk (TArrayView<uint8_t>& stream);
void SkipChunk (TArrayView<uint8_t>& stream);

void RunPlayerEventData(int player, int tic);

#endif //__D_PROTOCOL_H__
