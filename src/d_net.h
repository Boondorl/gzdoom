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
	} Tics[BACKUPTICS] = {};

	FClientNetState()
	{
		SentTime = LastRecvTime = I_msTime();
	}

	// Local information about client.
	uint64_t		LastRecvTime;			// The last time a packet arrived from this client.
	uint64_t		SentTime;				// Timestamp for when the client sent out their latest packet to us.
	int				SequenceAck = -1;		// The last sequence the client reported from us.
	int 			CurrentSequence = -1;	// The last sequence we've gotten from this client.
	int				Flags = 0;				// State of this client.

	// Every packet includes consistencies for tics that client ran. When
	// a world tic is ran, the local client will store all the consistencies
	// of the clients in their LocalConsistency. Then the consistencies will
	// be checked against retroactively as they come in.
	int ConsistencyAck = -1;					// Last consistency the client reported from us.
	int LastVerifiedConsistency = 0;			// Last consistency we checked from this client. If < CurrentNetConsistency, run through them.
	int CurrentNetConsistency = 0;				// Last consistency we got from this client.
	int16_t NetConsistency[BACKUPTICS] = {};	// Consistencies we got from this client.
	int LastSentConsistency = 0;				// Last consistency we sent out. If < CurrentLocalConsistency, send them out.
	int CurrentLocalConsistency = 0;			// Amount of consistencies that were generated since the last packet.
	int16_t LocalConsistency[BACKUPTICS] = {};	// Local consitency of the client to check against.
};

enum ENetMode : uint8_t
{
	NET_PeerToPeer,
	NET_PacketServer
};

// New packet structure:
//
// Header:
//  One byte for the net command flags.
//  Four bytes for the base sequence the client is working from.
//  Four bytes for the highest confirmed sequence the client got from us.
//  If NCMD_QUITTERS set, one byte for the number of players followed by one byte for each player's consolenum. Packet server mode only.
//  One byte for the number of players.
//  If NCMD_XTICS set, one byte for the number of additional tics - 3. Otherwise NCMD_1/2TICS determines tic count.
//  Four bytes for the highest confirmed consistency the client got from us.
//  For each player:
//   One byte for the player number.
//   Eight bytes for the time in ms the packet was sent out.
//   One byte for the number of consistencies followed by two bytes for each consistency.
//   For each tic:
//    One byte for the delta from the base sequence.
//    The remaining command and event data for that player.

// Create any new ticcmds and broadcast to other players.
void NetUpdate(int tics);

// Broadcasts special packets to other players
//	to notify of game exit
void D_QuitNetGame (void);

//? how many ticks to run?
void TryRunTics (void);

//Use for checking to see if the netgame has stalled
void Net_CheckLastReceived();

// [RH] Functions for making and using special "ticcmds"
void Net_NewClientTic();
void Net_Initialize();
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
extern doomcom_t			doomcom;
extern usercmd_t			LocalCmds[LOCALCMDTICS];
extern int					ClientTic;
extern FClientNetState		ClientStates[MAXPLAYERS];
extern ENetMode				NetMode;

class player_t;
class DObject;

#endif
