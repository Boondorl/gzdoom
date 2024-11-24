//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Networking stuff.
//
//-----------------------------------------------------------------------------


#ifndef __D_NET__
#define __D_NET__

#include "doomtype.h"
#include "doomdef.h"
#include "d_protocol.h"
#include "i_net.h"
#include <queue>

uint64_t I_msTime();

class FDynamicBuffer
{
public:
	FDynamicBuffer();
	~FDynamicBuffer();

	void SetData(const uint8_t *data, int len);
	uint8_t* GetData(int *len = nullptr);

private:
	uint8_t* m_Data;
	int m_Len, m_BufferLen;
};

enum EClientFlags
{
	CF_NONE			= 0,
	CF_QUIT			= 1,		// If in packet server mode, this client sent an exit command and needs to be disconnected.
	CF_MISSING_SEQ	= 1 << 1,	// If a sequence was missed/out of order, ask this client to send back over their info.
	CF_RETRANSMIT	= 1 << 2,	// If set, this client needs data resent to them.
};

struct FClientNetState
{
	// Networked client data.
	struct FNetTic {
		FDynamicBuffer	Data;
		usercmd_t		Command;
	} Tics[BACKUPTICS];

	FClientNetState()
	{
		SentTime = LastRecvTime = I_msTime();
	}

	// Local information about client.
	uint64_t		LastRecvTime;		// The last time a packet arrived from this client.
	uint64_t		SentTime;			// Timestamp for when the client sent out their latest packet to us.
	unsigned int	SequenceAck = -1;		// The last sequence the client reported from us.
	unsigned int 	CurrentSequence = -1;	// The last sequence we've gotten from this client.
	int				Flags = 0;				// State of this client.

	// Ensure that clients aren't out of sync with each other by having them send over their consistency
	// for that 
	std::queue<int16_t>	ConsistencyChecks;
};

// If multiple ticks are ran during a single check, make sure all of them get sent over.
TArray<int16_t> OutgoingConsistencyChecks;

// New packet structure:
//
// Header:
//  One byte with net command flags.
//  Four bytes for the base sequence the client is working from.
//  Four bytes with the highest confirmed sequence the client got from us.
//  Four bytes with the time in ms the packet was sent out.
//  One byte for the net delay.
//  If NCMD_XTICS set, another byte with the number of additional tics - 3. Otherwise NCMD_1/2TICS determines tic count.
//  If NCMD_QUITTERS set, one byte with number of players followed by one byte with each player's consolenum. Packet server mode only.
//  If NCMD_MULTI set, one byte with number of players followed by one byte with each player's consolenum. Packet server mode only.
//
// For each tic:
//  One byte for the delta from the base sequence.
//  Two bytes for the consistency check.
//  Two bytes with the remaining data size.
//  The remaining command and event data for that player.

// Create any new ticcmds and broadcast to other players.
void NetUpdate();

// Broadcasts special packets to other players
//	to notify of game exit
void D_QuitNetGame (void);

//? how many ticks to run?
void TryRunTics (void);

//Use for checking to see if the netgame has stalled
void Net_CheckLastReceived();

// [RH] Functions for making and using special "ticcmds"
void Net_NewClientTic();
void Net_WriteInt8(uint8_t);
void Net_WriteInt16(int16_t);
void Net_WriteInt32(int32_t);
void Net_WriteInt64(int64_t);
void Net_WriteFloat(float);
void Net_WriteDouble(double);
void Net_WriteString(const char *);
void Net_WriteBytes(const uint8_t *, int len);

void Net_DoCommand(int cmd, uint8_t **stream, int player);
void Net_SkipCommand(int cmd, uint8_t **stream);

void Net_ClearBuffers();

// Netgame stuff (buffers and pointers, i.e. indices).

// This is the interface to the packet driver, a separate program
// in DOS, but just an abstraction here.
extern doomcom_t		doomcom;
extern usercmd_t		LocalCmds[LOCALCMDTICS];
extern int				ClientTic;
extern FClientNetState	ClientStates[MAXPLAYERS];

class player_t;
class DObject;

#endif
