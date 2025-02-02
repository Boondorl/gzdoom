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
//		DOOM Network game communication and protocol,
//		all OS independent parts.
//
//-----------------------------------------------------------------------------

#include <stddef.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "version.h"
#include "menu.h"
#include "i_video.h"
#include "i_net.h"
#include "g_game.h"
#include "c_console.h"
#include "d_netinf.h"
#include "d_net.h"
#include "cmdlib.h"
#include "m_cheat.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "sbar.h"
#include "gi.h"
#include "m_misc.h"
#include "gameconfigfile.h"
#include "p_acs.h"
#include "p_trace.h"
#include "a_sharedglobal.h"
#include "st_start.h"
#include "teaminfo.h"
#include "p_conversation.h"
#include "d_eventbase.h"
#include "p_enemy.h"
#include "m_argv.h"
#include "p_lnspec.h"
#include "p_spec.h"
#include "hardware.h"
#include "r_utility.h"
#include "a_keys.h"
#include "intermission/intermission.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "events.h"
#include "i_time.h"
#include "i_system.h"
#include "vm.h"
#include "gstrings.h"
#include "s_music.h"
#include "screenjob.h"
#include "d_main.h"
#include "i_interface.h"
#include "savegamemanager.h"

EXTERN_CVAR (Int, disableautosave)
EXTERN_CVAR (Int, autosavecount)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, vid_vsync)
EXTERN_CVAR (Int, vid_maxfps)

//#define SIMULATEERRORS		(RAND_MAX/3)
#define SIMULATEERRORS			0

extern uint8_t		*demo_p;		// [RH] Special "ticcmds" get recorded in demos
extern FString	savedescription;
extern FString	savegamefile;

extern bool AppActive;

enum ENetMode : uint8_t
{
	NET_PeerToPeer,
	NET_PacketServer
};

struct FNetGameInfo
{
	uint32_t DetectedPlayers[MAXPLAYERS];
	uint8_t GotSetup[MAXPLAYERS];
};

// NETWORKING
//
// gametic is the tic about to (or currently being) run.
// ClientTic is the tick the client is currently on and building a command for.
//
// A world tick cannot be ran until CurrentSequence > gametic for all clients.

#define NetBuffer (doomcom.data)
ENetMode NetMode = NET_PeerToPeer;
int 				ClientTic = 0;
int					LocalDelay = 0;
usercmd_t			LocalCmds[LOCALCMDTICS];
FClientNetState		ClientStates[MAXPLAYERS] = {};
FConsistencyCheck	OutgoingConsistencyChecks; // If multiple ticks are ran during a single check, make sure all of them get sent over.

// If we're sending a packet to ourselves, store it here instead. This is the simplest way to execute
// playback as it means in the world running code itself all player commands are built the exact same way
// instead of having to rely on pulling from the correct local buffers. It also ensures all commands are
// executed over the net at the exact same tick.
static size_t	LocalNetBufferSize = 0;
static uint8_t	LocalNetBuffer[MAX_MSGLEN];

// Used for storing network delay times. This is separate since it's not actually tied to
// whatever sequence a client is currently on, only how many packets we've gotten from them.
static int		LastGameUpdate = 0; // Track the last time the game actually ran the world.

static int 	EnterTic = 0;
static int	LastEnterTic = 0;

void D_ProcessEvents(void); 
void G_BuildTiccmd(usercmd_t *cmd);
void D_DoAdvanceDemo(void);

static void SendSetup(const FNetGameInfo& info, int len);
static void RunScript(uint8_t **stream, AActor *pawn, int snum, int argn, int always);

extern	bool	 advancedemo;

CVAR(Bool, vid_dontdowait, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, vid_lowerinbackground, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR(Bool, net_ticbalance, false, CVAR_SERVERINFO | CVAR_NOSAVE)
CUSTOM_CVAR(Int, net_extratic, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
	else if (self > 2)
		self = 2;
}

// Used to write out all network events that occured leading up to the next tick.
static struct NetEventData
{
	struct FStream {
		uint8_t* Stream;
		size_t Used = 0;

		FStream()
		{
			Grow(256);
		}

		~FStream()
		{
			if (Stream != nullptr)
				M_Free(Stream);
		}

		void Grow(size_t size)
		{
			Stream = (uint8_t*)M_Realloc(Stream, size);
		}
	} Streams[BACKUPTICS];

private:
	size_t CurrentSize = 0;
	size_t MaxSize = 256;
	int CurrentClientTic = 0;

	// Make more room for special Command.
	void GetMoreBytes(size_t newSize)
	{
		MaxSize = max<size_t>(MaxSize * 2, newSize + 30);

		DPrintf(DMSG_NOTIFY, "Expanding special size to %zu\n", MaxSize);

		for (auto& stream : Streams)
			Streams->Grow(MaxSize);

		CurrentStream = Streams[CurrentClientTic % BACKUPTICS].Stream + CurrentSize;
	}

	void AddBytes(size_t bytes)
	{
		if (CurrentSize + bytes >= MaxSize)
			GetMoreBytes(CurrentSize + bytes);

		CurrentSize += bytes;
	}

public:
	uint8_t* CurrentStream = nullptr;

	// Boot up does some faux network events so we need to wait until after
	// everything is initialized to actually set up the network stream.
	void InitializeEventData()
	{
		CurrentStream = Streams[0].Stream;
		CurrentSize = 0;
	}

	void NewClientTic()
	{
		const int tic = ClientTic / doomcom.ticdup;
		if (CurrentClientTic == tic)
			return;

		Streams[CurrentClientTic % BACKUPTICS].Used = CurrentSize;
		
		CurrentClientTic = tic;
		CurrentStream = Streams[tic % BACKUPTICS].Stream;
		CurrentSize = 0;
	}

	NetEventData& operator<<(uint8_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(1);
			WriteInt8(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int16_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(2);
			WriteInt16(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int32_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(4);
			WriteInt32(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int64_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(8);
			WriteInt64(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(float it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(4);
			WriteFloat(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(double it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(8);
			WriteDouble(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(const char *it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(strlen(it) + 1);
			WriteString(it, &CurrentStream);
		}
		return *this;
	}
} NetEvents;

void Net_ClearBuffers()
{
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		playeringame[i] = false;
		for (int j = 0; j < BACKUPTICS; ++j)
			ClientStates[i].Tics[j].Data.SetData(nullptr, 0);
	}

	NetworkClients.Clear();
	netgame = multiplayer = false;
	LastEnterTic = EnterTic;
	gametic = ClientTic = 0;
	LastGameUpdate = 0;

	memset(LocalCmds, 0, sizeof(LocalCmds));
	memset(ClientStates, 0, sizeof(ClientStates));

	doomcom.command = doomcom.datalength = 0;
	doomcom.remoteplayer = -1;
	doomcom.numplayers = doomcom.ticdup = 1;
	consoleplayer = doomcom.consoleplayer = 0;
	playeringame[0] = true;
	NetworkClients += 0;
}

// [RH] Rewritten to properly calculate the packet size
//		with our variable length Command.
static int GetNetBufferSize()
{
	if (NetBuffer[0] & NCMD_EXIT)
		return 1;
	if (NetBuffer[0] & NCMD_NEWHOST)
		return 2;
	// TODO: Need a skipper for this.
	if (NetBuffer[0] & NCMD_SETUP)
		return doomcom.datalength;

	int totalBytes = 17;
	if (NetBuffer[0] & NCMD_QUITTERS)
		totalBytes += NetBuffer[totalBytes] + 1;

	int playerCount = NetBuffer[totalBytes++];

	int numTics = NetBuffer[0] & NCMD_XTICS;
	if (numTics == NCMD_XTICS)
		numTics += NetBuffer[totalBytes++];

	// Need at least 1 byte per tic and 2 per player - 1 for tic offset, 1 for each player number, and 1 for each empty user command.
	if (doomcom.datalength < totalBytes + numTics + 2 * playerCount)
		return totalBytes + numTics + 2 * playerCount;

	uint8_t* skipper = &NetBuffer[totalBytes];
	for (int i = 0; i < numTics; ++i)
	{
		++skipper;
		for (int p = 0; p < playerCount; ++p)
		{
			++skipper;
			SkipUserCmdMessage(skipper);
		}
	}

	return int(skipper - NetBuffer);
}

//
// HSendPacket
//
static void HSendPacket(int client, size_t size)
{
	// This data already exists locally in the demo file, so don't write it out.
	if (demoplayback)
		return;

	doomcom.remoteplayer = client;
	doomcom.datalength = size;
	if (client == consoleplayer)
	{
		memcpy(LocalNetBuffer, NetBuffer, size);
		LocalNetBufferSize = size;
		return;
	}

	if (!netgame)
		I_Error("Tried to send a packet to a client while offline");

	doomcom.command = CMD_SEND;
	I_NetCmd();
}

// HGetPacket
// Returns false if no packet is waiting
static bool HGetPacket()
{
	if (demoplayback)
		return false;

	if (LocalNetBufferSize)
	{
		memcpy(NetBuffer, LocalNetBuffer, LocalNetBufferSize);
		doomcom.datalength = LocalNetBufferSize;
		doomcom.remoteplayer = consoleplayer;
		LocalNetBufferSize = 0;
		return true;
	}

	if (!netgame)
		return false;

	doomcom.command = CMD_GET;
	I_NetCmd();

	if (doomcom.remoteplayer == -1)
		return false;

	int sizeCheck = GetNetBufferSize();
	if (doomcom.datalength != sizeCheck)
	{
		Printf("Incorrect packet size %d (expected %d)\n", doomcom.datalength, sizeCheck);
		return false;
	}

	return true;
}

static void ClientConnecting(int client)
{
	if (consoleplayer != Net_Arbitrator)
		return;

	NetBuffer[0] = NCMD_GAMEREADY;
	HSendPacket(client, 1);
}

static void DisconnectClient(int clientNum)
{
	NetworkClients -= clientNum;
	playeringame[clientNum] = false;
	// Capture the pawn leaving in the next world tick.
	players[clientNum].playerstate = PST_GONE;
}

static void SetArbitrator(int clientNum)
{
	Net_Arbitrator = clientNum;
	players[Net_Arbitrator].settings_controller = true;
	Printf("%s is the new host\n", players[Net_Arbitrator].userinfo.GetName());
	if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator)
	{
		// Since everybody is gonna be sending us packets now, we need to reset
		// their latency measurements (normally this comes from the host themselves
		// in this mode).
		for (auto client : NetworkClients)
			ClientStates[client].SentTime = I_msTime();
	}
}

static void ClientQuit(int clientNum)
{
	if (!NetworkClients.InGame(clientNum))
		return;

	// This will get caught in the main loop and send it out to everyone as one big packet. The only
	// exception is the host who will leave instantly and send out any needed data.
	if (NetMode == NET_PacketServer)
	{
		if (consoleplayer != Net_Arbitrator)
			DPrintf(DMSG_WARNING, "Received disconnect packet from client %d erroneously\n", clientNum);
		else
			ClientStates[clientNum].Flags |= CF_QUIT;

		return;
	}

	DisconnectClient(clientNum);
	if (clientNum == Net_Arbitrator)
		SetArbitrator(NetworkClients[0]);

	if (demorecording)
		G_CheckDemoStatus();
}

//
// GetPackets
//
static void GetPackets()
{						 
	while (HGetPacket())
	{
		const int clientNum = doomcom.remoteplayer;
		auto& clientState = ClientStates[clientNum];
		clientState.LastRecvTime = I_msTime();
		clientState.Flags &= ~(CF_MISSING_SEQ | CF_RETRANSMIT);

		if (NetBuffer[0] & NCMD_SETUP)
		{
			ClientConnecting(clientNum);
			continue;
		}

		if (NetBuffer[0] & NCMD_NEWHOST)
		{
			DisconnectClient(clientNum);
			SetArbitrator(NetBuffer[1]);
			continue;
		}

		if (NetBuffer[0] & NCMD_EXIT)
		{
			ClientQuit(clientNum);
			continue;
		}

		const unsigned int baseSequence = NetBuffer[1] << 24 | NetBuffer[2] << 16 | NetBuffer[3] << 8 | NetBuffer[4];
		clientState.SequenceAck = NetBuffer[5] << 24 | NetBuffer[6] << 16 | NetBuffer[7] << 8 | NetBuffer[8];
		clientState.SentTime = (uint64_t(NetBuffer[9]) << 56) | (uint64_t(NetBuffer[10]) << 48) | (uint64_t(NetBuffer[11]) << 40) | (uint64_t(NetBuffer[12]) << 32)
								| (uint64_t(NetBuffer[13]) << 24) | (uint64_t(NetBuffer[14]) << 16) | (uint64_t(NetBuffer[15]) << 8) | uint64_t(NetBuffer[16]);

		if (NetBuffer[0] & NCMD_RETRANSMIT)
			clientState.Flags |= CF_RETRANSMIT;

		int curByte = 17;
		if (NetBuffer[0] & NCMD_QUITTERS)
		{
			int numPlayers = NetBuffer[curByte++];
			for (int i = 0; i < numPlayers; ++i)
				DisconnectClient(NetBuffer[curByte++]);
		}

		int playerCount = NetBuffer[curByte++];

		int totalTics = (NetBuffer[0] & NCMD_XTICS);
		if (totalTics == NCMD_XTICS)
			totalTics += NetBuffer[curByte++];

		// Each tic within a given packet is given a sequence number to ensure that things were put
		// back together correctly. Normally this wouldn't matter as much but since we need to keep
		// clients in lock step a misordered packet will instantly cause a desync.
		for (int t = 0; t < totalTics; ++t)
		{
			const int seq = baseSequence + NetBuffer[curByte++];

			// Duplicate command, ignore it.
			if (seq <= clientState.CurrentSequence)
			{
				for (int i = 0; i < playerCount; ++i)
				{
					uint8_t* skipper = &NetBuffer[++curByte];
					curByte += SkipUserCmdMessage(skipper);
				}

				continue;
			}

			// Skipped a command. Packet likely got corrupted while being put back together, so have
			// the client send over the properly ordered commands. 
			if (seq > clientState.CurrentSequence + 1)
			{
				clientState.Flags |= CF_MISSING_SEQ;
				break;
			}

			for (int i = 0; i < playerCount; ++i)
			{
				const int pNum = NetBuffer[curByte++];

				uint8_t* start = &NetBuffer[curByte];
				curByte += ReadUserCmdMessage(start, pNum, seq);

				// Set this on the individual player as well so host migration is easier in
				// packet server mode.
				ClientStates[pNum].CurrentSequence = seq;
			}

			clientState.CurrentSequence = seq;
		}
	}
}

void NetUpdate(int tics)
{
	// When playing in packet server mode, the host can strategically tell each client how long they should wait
	// after starting before generating and sending out inputs. This significantly reduces the net latency clients
	// have since they'll be forced to wait at minimum host -> highest latency client round trip before they can
	// do anything anyway, so time packets to arrive at the host roughly the same time as it does for the
	// highest latency player. This way the only net delay is the round trip time from this client to the host.
	if (NetMode == NET_PacketServer && LocalDelay)
	{
		if (LocalDelay > 0)
		{
			if (LocalDelay >= tics)
			{
				LocalDelay -= tics;
				tics = 0;
			}
			else
			{
				tics -= LocalDelay;
				LocalDelay = 0;
			}
		}
		else if (consoleplayer == Net_Arbitrator)
		{
			bool allFound = true;
			for (auto client : NetworkClients)
			{
				if (client != Net_Arbitrator && ClientStates[client].CurrentSequence < 0)
				{
					allFound = false;
					tics = 0;
					break;
				}
			}

			if (allFound)
				LocalDelay = 0;
		}
	}

	if (tics <= 0)
	{
		GetPackets();
		return;
	}

	// build new ticcmds for console player
	const int startTic = ClientTic;
	for (int i = 0; i < tics; ++i)
	{
		I_StartTic();
		D_ProcessEvents();
		if (pauseext || (ClientTic - gametic) / doomcom.ticdup >= BACKUPTICS / 2 - 1)
			break;			// can't hold any more
		
		G_BuildTiccmd(&LocalCmds[ClientTic++ % LOCALCMDTICS]);
		if (doomcom.ticdup == 1 || ClientTic == 1)
		{
			Net_NewClientTic();
		}
		else
		{
			if (ClientTic % doomcom.ticdup)
			{
				const int mod = ClientTic - ClientTic % doomcom.ticdup;

				// Even if we're not sending out inputs, update the local commands so that the doomcom.ticdup
				// is correctly played back while predicting as best as possible. This will help prevent
				// minor hitches when playing online.
				// Boon TODO: why 2???
				for (int j = ClientTic - 2; j >= mod; --j)
					LocalCmds[j % LOCALCMDTICS].buttons |= LocalCmds[(j + 1) % LOCALCMDTICS].buttons;
			}
			else
			{
				// Gather up the Command across the last doomcom.ticdup number of tics
				// and average them out. These are propagated back to the local
				// command so that they'll be predicted correctly.
				const int lastTic = ClientTic - doomcom.ticdup;

				int pitch = 0;
				int yaw = 0;
				int roll = 0;
				int forwardmove = 0;
				int sidemove = 0;
				int upmove = 0;

				for (int j = 0; j < doomcom.ticdup; ++j)
				{
					const int mod = (lastTic + j) % LOCALCMDTICS;
					pitch += LocalCmds[mod].pitch;
					yaw += LocalCmds[mod].yaw;
					roll += LocalCmds[mod].roll;
					forwardmove += LocalCmds[mod].forwardmove;
					sidemove += LocalCmds[mod].sidemove;
					upmove += LocalCmds[mod].upmove;
				}

				pitch /= doomcom.ticdup;
				yaw /= doomcom.ticdup;
				roll /= doomcom.ticdup;
				forwardmove /= doomcom.ticdup;
				sidemove /= doomcom.ticdup;
				upmove /= doomcom.ticdup;

				for (int j = 0; j < doomcom.ticdup; ++j)
				{
					const int mod = (lastTic + j) % LOCALCMDTICS;
					LocalCmds[mod].pitch = pitch;
					LocalCmds[mod].yaw = yaw;
					LocalCmds[mod].roll = roll;
					LocalCmds[mod].forwardmove = forwardmove;
					LocalCmds[mod].sidemove = sidemove;
					LocalCmds[mod].upmove = upmove;
				}

				Net_NewClientTic();
			}
		}
	}

	if (singletics)
		return; 		// singletic update is synchronous

	if (demoplayback)
	{
		// Boon TODO: What?
		// Don't touch net command data while playing a demo, as it'll already exist.
		ClientStates[consoleplayer].CurrentSequence = (ClientTic / doomcom.ticdup);
		return;
	}

	// If ClientTic didn't cross a doomcom.ticdup boundary, only send packets
	// to players waiting for resends.
	const int startSequence = startTic / doomcom.ticdup;
	const int endSequence = ClientTic / doomcom.ticdup;
	const bool resendOnly = startSequence == endSequence;

	int quitters = 0;
	int quitNums[MAXPLAYERS];
	if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator)
	{
		for (auto client : NetworkClients)
		{
			// The host has special handling when disconnecting in a packet server game.
			if (client != Net_Arbitrator && (ClientStates[client].Flags & CF_QUIT))
				quitNums[quitters++] = client;
		}
	}

	const uint64_t time = I_msTime();
	for (auto client : NetworkClients)
	{
		// If in packet server mode, we don't want to send information to anyone but the host. On the other
		// hand, if we're the host we send out everyone's info to everyone else. Whatever consistency checks
		// we have on the host machine is used for verification (don't rely on clients to verify themselves).
		if (NetMode == NET_PacketServer && client != Net_Arbitrator)
			continue;

		auto& curState = ClientStates[client];
		// If we can only resend, don't send clients any information that they already have.
		if (resendOnly && !(curState.Flags & CF_RETRANSMIT))
			continue;
		
		// Only send over our newest tics. If a client missed one, they'll let us know.
		const int sequenceNum = (curState.Flags & CF_RETRANSMIT) ? curState.SequenceAck + 1 : startSequence;
		int numTics = endSequence - sequenceNum;
		if (numTics > BACKUPTICS)
			I_Error("Player %d missed too many tics", client);

		NetBuffer[0] = (curState.Flags & CF_MISSING_SEQ) ? NCMD_RETRANSMIT : 0;
		// Sequence basis for our own packet.
		NetBuffer[1] = (sequenceNum >> 24);
		NetBuffer[2] = (sequenceNum >> 16);
		NetBuffer[3] = (sequenceNum >> 8);
		NetBuffer[4] = sequenceNum;
		// Last sequence we got from this client.
		NetBuffer[5] = (curState.CurrentSequence >> 24);
		NetBuffer[6] = (curState.CurrentSequence >> 16);
		NetBuffer[7] = (curState.CurrentSequence >> 8);
		NetBuffer[8] = curState.CurrentSequence;
		// Time used to track latency.
		NetBuffer[9] = (time >> 56);
		NetBuffer[10] = (time >> 48);
		NetBuffer[11] = (time >> 40);
		NetBuffer[12] = (time >> 32);
		NetBuffer[13] = (time >> 24);
		NetBuffer[14] = (time >> 16);
		NetBuffer[15] = (time >> 8);
		NetBuffer[16] = time;

		size_t size = 17;
		if (quitters > 0)
		{
			NetBuffer[0] |= NCMD_QUITTERS;
			NetBuffer[size++] = quitters;
			for (int i = 0; i < quitters; ++i)
				NetBuffer[size++] = quitNums[i];
		}

		int playerNums[MAXPLAYERS];
		int playerCount = NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator ? NetworkClients.Size() : 1;
		NetBuffer[size++] = playerCount;
		if (playerCount > 1)
		{
			int i = 0;
			for (auto cl : NetworkClients)
				playerNums[i++] = cl;
		}
		else
		{
			playerNums[0] = consoleplayer;
		}
		
		if (numTics < 3)
		{
			NetBuffer[0] |= numTics;
		}
		else
		{
			NetBuffer[0] |= NCMD_XTICS;
			NetBuffer[size++] = numTics - 3;
		}

		// Client commands.

		uint8_t* cmd = &NetBuffer[size];
		for (int t = 0; t < numTics; ++t)
		{
			cmd[0] = t;
			++cmd;

			for (int i = 0; i < playerCount; ++i)
			{
				cmd[0] = playerNums[i];
				++cmd;

				int curTic = sequenceNum + t, lastTic = curTic - 1;
				if (playerNums[i] == consoleplayer)
				{
					int realTic = (curTic * doomcom.ticdup) % LOCALCMDTICS;
					int realLastTic = (lastTic * doomcom.ticdup) % LOCALCMDTICS;
					// Write out the net events before the user commands so inputs can
					// be used as a marker for when the given tic ends.
					auto& stream = NetEvents.Streams[curTic % BACKUPTICS];
					if (stream.Used)
					{
						memcpy(cmd, stream.Stream, stream.Used);
						cmd += stream.Used;
					}

					WriteUserCmdMessage(LocalCmds[realTic],
						realLastTic >= 0 ? &LocalCmds[realLastTic] : nullptr, cmd);
				}
				else
				{
					auto& netTic = ClientStates[playerNums[i]].Tics[curTic];

					int len;
					uint8_t* data = netTic.Data.GetData(&len);
					if (data != nullptr)
					{
						memcpy(cmd, data, len);
						cmd += len;
					}

					WriteUserCmdMessage(netTic.Command,
						lastTic >= 0 ? &ClientStates[playerNums[i]].Tics[lastTic].Command : nullptr, cmd);
				}
			}
		}

		HSendPacket(client, int(cmd - NetBuffer));
		if (client != consoleplayer && net_extratic)
			HSendPacket(client, int(cmd - NetBuffer));
	}

	// Listen for other packets. This has to come after sending so the player that sent
	// data to themselves gets it immediately (important for singleplayer; otherwise there
	// would always be a one-tic delay).
	GetPackets();
}

// User info packets look like this:
//
//  One byte set to NCMD_SETUP or NCMD_USERINFO
//  One byte for the relevant player number.
//	Three bytes for the sender's game version (major, minor, revision).
//	Four bytes for the bit mask indicating which players the sender knows about.
//  If NCMD_SETUP, one byte indicating the guest got the game setup info.
//  Player's user information.
//
//    The guests always send NCMD_SETUP packets, and the host always
//    sends NCMD_USERINFO packets.
//
// Game info packets look like this:
//
//  One byte set to NCMD_GAMEINFO.
//  One byte for doomcom.ticdup setting.
//  One byte for NetMode setting.
//  String with starting map's name.
//  The game's RNG seed.
//  Remaining game information.
//
// Ready packets look like this:
//
//  One byte set to NCMD_GAMEREADY.
//
// Each machine sends user info packets to the host. The host sends user
// info packets back to the other machines as well as game info packets.
// Negotiation is done when all the guests have reported to the host that
// they know about the other nodes.

bool ExchangeNetGameInfo(void *userdata)
{
	FNetGameInfo *data = reinterpret_cast<FNetGameInfo*>(userdata);
	uint8_t *stream = nullptr;
	while (HGetPacket())
	{
		// For now throw an error until something more complex can be done to handle this case.
		if (NetBuffer[0] == NCMD_EXIT)
			I_Error("Game unexpectedly ended.");

		if (NetBuffer[0] == NCMD_SETUP || NetBuffer[0] == NCMD_USERINFO)
		{
			int clientNum = NetBuffer[1];
			data->DetectedPlayers[clientNum] =
				(NetBuffer[5] << 24) | (NetBuffer[6] << 16) | (NetBuffer[7] << 8) | NetBuffer[8];

			if (NetBuffer[0] == NCMD_SETUP)
			{
				// Sent from guest.
				data->GotSetup[clientNum] = NetBuffer[9];
				stream = &NetBuffer[10];
			}
			else
			{
				// Sent from host.
				stream = &NetBuffer[9];
			}

			D_ReadUserInfoStrings(clientNum, &stream, false);
			if (!NetworkClients.InGame(clientNum))
			{
				if (NetBuffer[2] != VER_MAJOR % 255 || NetBuffer[3] != VER_MINOR % 255 || NetBuffer[4] != VER_REVISION % 255)
					I_Error("Different " GAMENAME " versions cannot play a net game");

				NetworkClients += clientNum;
				data->DetectedPlayers[consoleplayer] |= 1 << clientNum;

				I_NetMessage("Found %s (%d)", players[clientNum].userinfo.GetName(), clientNum);
			}
		}
		else if (NetBuffer[0] == NCMD_GAMEINFO)
		{
			data->GotSetup[consoleplayer] = true;

			doomcom.ticdup = clamp<int>(NetBuffer[1], 1, MAXTICDUP);
			NetMode = static_cast<ENetMode>(NetBuffer[2]);

			stream = &NetBuffer[3];
			startmap = ReadStringConst(&stream);
			rngseed = ReadInt32(&stream);
			C_ReadCVars(&stream);
		}
		else if (NetBuffer[0] == NCMD_GAMEREADY)
		{
			return true;
		}
	}

	// If everybody already knows everything, it's time to start.
	if (consoleplayer == Net_Arbitrator)
	{
		bool stillWaiting = false;

		// Update this dynamically in case a player dropped.
		// Boon TODO: This isn't going to work since ClientInGame won't be filled yet.
		int allPlayers = 1 << Net_Arbitrator;
		for (auto client : NetworkClients)
			allPlayers |= 1 << client;

		for (auto client : NetworkClients)
		{
			if (!data->GotSetup[client] || (data->DetectedPlayers[client] & allPlayers) != allPlayers)
			{
				stillWaiting = true;
				break;
			}
		}

		if (!stillWaiting)
			stillWaiting = (data->DetectedPlayers[Net_Arbitrator] & allPlayers) == allPlayers;

		if (!stillWaiting)
			return true;
	}

	NetBuffer[2] = VER_MAJOR % 255;
	NetBuffer[3] = VER_MINOR % 255;
	NetBuffer[4] = VER_REVISION % 255;
	NetBuffer[5] = data->DetectedPlayers[consoleplayer] >> 24;
	NetBuffer[6] = data->DetectedPlayers[consoleplayer] >> 16;
	NetBuffer[7] = data->DetectedPlayers[consoleplayer] >> 8;
	NetBuffer[8] = data->DetectedPlayers[consoleplayer];

	if (consoleplayer != Net_Arbitrator)
	{
		// If we're a guest, send our info over to the host.
		NetBuffer[0] = NCMD_SETUP;
		NetBuffer[1] = consoleplayer;
		NetBuffer[9] = data->GotSetup[consoleplayer];
		// If the host already knows we're here, just send over a heartbeat.
		if (!(data->DetectedPlayers[Net_Arbitrator] & (1 << consoleplayer)))
		{
			stream = &NetBuffer[10];
			auto str = D_GetUserInfoStrings(consoleplayer, true);
			memcpy(stream, str.GetChars(), str.Len() + 1);
			stream += str.Len();
		}
	}
	else
	{
		// If we're the host, send over known player data to guests. This is done instantly
		// since the game info will also get sent out after this.
		NetBuffer[0] = NCMD_USERINFO;
		for (auto client : NetworkClients)
		{
			if (client == Net_Arbitrator)
				continue;

			for (auto cl : NetworkClients)
			{
				if (cl == client)
					continue;

				// If the host knows about a client that the guest doesn't, send that client's info over to them.
				const int clBit = 1 << cl;
				if ((data->DetectedPlayers[Net_Arbitrator] & clBit) && !(data->DetectedPlayers[client] & clBit))
				{
					NetBuffer[1] = cl;
					stream = &NetBuffer[9];
					auto str = D_GetUserInfoStrings(cl, true);
					memcpy(stream, str.GetChars(), str.Len() + 1);
					stream += str.Len();
					HSendPacket(client, int(stream - NetBuffer));
				}
			}
		}

		// If we're the host, send the game info too.
		NetBuffer[0] = NCMD_GAMEINFO;
		NetBuffer[1] = (uint8_t)doomcom.ticdup;
		NetBuffer[2] = NetMode;
		stream = &NetBuffer[3];
		WriteString(startmap.GetChars(), &stream);
		WriteInt32(rngseed, &stream);
		C_WriteCVars(&stream, CVAR_SERVERINFO, true);
	}

	SendSetup(*data, int(stream - NetBuffer));
	return false;
}

static bool D_ExchangeNetInfo()
{
	// Return right away if it's just a solo net game.
	if (doomcom.numplayers == 1)
		return true;

	autostart = true;

	FNetGameInfo info = {};
	info.DetectedPlayers[consoleplayer] = 1 << consoleplayer;
	info.GotSetup[consoleplayer] = consoleplayer == Net_Arbitrator;
	NetworkClients += consoleplayer;
	
	if (consoleplayer == Net_Arbitrator)
		I_NetInit("Sending game information", 1);
	else
		I_NetInit("Waiting for host information", 1);

	if (!I_NetLoop(ExchangeNetGameInfo, &info))
		return false;

	// Let everyone else know the game is ready to start.
	if (consoleplayer == Net_Arbitrator)
	{
		NetBuffer[0] = NCMD_GAMEREADY;
		for (auto client : NetworkClients)
		{
			if (client != Net_Arbitrator)
				HSendPacket(client, 1);
		}
	}

	I_NetDone();
	return true;
}

static void SendSetup(const FNetGameInfo& info, int len)
{
	if (consoleplayer != Net_Arbitrator)
	{
		HSendPacket(Net_Arbitrator, len);
	}
	else
	{
		for (auto client : NetworkClients)
		{
			// Only send game info over to clients still needing it.
			if (client != Net_Arbitrator && !info.GotSetup[client])
				HSendPacket(client, len);
		}
	}
}

// Connects players to each other if needed.
bool D_CheckNetGame()
{
	const char* v = Args->CheckValue("-netmode");
	int result = I_InitNetwork(); // I_InitNetwork sets doomcom and netgame
	if (result == -1)
		return false;
	else if (result > 0 && v == nullptr) // Don't override manual netmode setting.
		NetMode = NET_PacketServer;

	if (doomcom.id != DOOMCOM_ID)
		I_FatalError("Invalid doomcom id set for network buffer");

	players[Net_Arbitrator].settings_controller = true;
	consoleplayer = doomcom.consoleplayer;
	for (auto client : NetworkClients)
		playeringame[client] = true;

	if (consoleplayer == Net_Arbitrator)
	{
		if (v != nullptr)
			NetMode = atoi(v) ? NET_PacketServer : NET_PeerToPeer;

		if (doomcom.numplayers > 1)
		{
			Printf("Selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode. (%s)\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server",
				v != nullptr ? "forced" : "auto");
		}

		// Boon TODO: Change this to a bool
		if (Args->CheckParm("-extratic"))
			net_extratic = 1;
	}

	// [RH] Setup user info
	D_SetupUserInfo();

	if (netgame)
	{
		GameConfig->ReadNetVars();	// [RH] Read network ServerInfo cvars
		if (!D_ExchangeNetInfo())
			return false;
	}

	if (consoleplayer != Net_Arbitrator && doomcom.numplayers > 1)
		Printf("Host selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode.\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server");

	Printf("player %d of %d\n", consoleplayer + 1, doomcom.numplayers);
	
	return true;
}

//
// D_QuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame()
{
	if (!netgame || !usergame || consoleplayer == -1 || demoplayback || NetworkClients.Size() == 1)
		return;

	// Send a bunch of packets for stability.
	if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator)
	{
		// This currently isn't much different from the regular P2P code, but it's being split off into its
		// own branch should proper host migration be added in the future (i.e. sending over stored event
		// data rather than just dropping it entirely).
		NetBuffer[0] = NCMD_NEWHOST;
		int nextHost = 0;
		for (auto client : NetworkClients)
		{
			if (client != Net_Arbitrator)
			{
				nextHost = client;
				break;
			}
		}

		NetBuffer[1] = nextHost;
		for (int i = 0; i < 4; ++i)
		{
			for (auto client : NetworkClients)
			{
				if (client != Net_Arbitrator)
					HSendPacket(nextHost, 2);
			}

			I_WaitVBL(1);
		}
	}
	else
	{
		NetBuffer[0] = NCMD_EXIT;
		for (int i = 0; i < 4; ++i)
		{
			// If in packet server mode, only the host should know about this
			// information.
			if (NetMode == NET_PacketServer)
			{
				HSendPacket(Net_Arbitrator, 1);
			}
			else
			{
				for (auto client : NetworkClients)
				{
					if (client != consoleplayer)
						HSendPacket(client, 1);
				}
			}

			I_WaitVBL(1);
		}
	}
}

// Forces playsim processing time to be consistent across frames.
// This improves interpolation for frames in between tics.
//
// With this cvar off the mods with a high playsim processing time will appear
// less smooth as the measured time used for interpolation will vary.

CVAR(Bool, r_ticstability, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

static uint64_t stabilityticduration = 0;
static uint64_t stabilitystarttime = 0;

static void TicStabilityWait()
{
	using namespace std::chrono;
	using namespace std::this_thread;

	if (!r_ticstability)
		return;

	uint64_t start = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	while (true)
	{
		uint64_t cur = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
		if (cur - start > stabilityticduration)
			break;
	}
}

static void TicStabilityBegin()
{
	using namespace std::chrono;
	stabilitystarttime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

static void TicStabilityEnd()
{
	using namespace std::chrono;
	uint64_t stabilityendtime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	stabilityticduration = min(stabilityendtime - stabilitystarttime, (uint64_t)1'000'000);
}

//
// TryRunTics
//
void TryRunTics()
{
	bool doWait = (cl_capfps || pauseext || (r_NoInterpolate && !M_IsAnimated()));
	if (vid_dontdowait && (vid_maxfps > 0 || vid_vsync))
		doWait = false;
	if (!AppActive && vid_lowerinbackground)
		doWait = true;

	// Get the full number of tics the client can run.
	if (doWait)
		EnterTic = I_WaitForTic(LastEnterTic);
	else
		EnterTic = I_GetTime();

	const int startCommand = ClientTic;
	const int totalTics = EnterTic - LastEnterTic;
	LastEnterTic = EnterTic;

	GC::CheckGC();

	// Listen for other clients and send out data as needed. This is also
	// needed for singleplayer! But is instead handled entirely through local
	// buffers. This has a limit of 17 tics that can be generated.
	NetUpdate(totalTics);

	// If the game is paused, everything we need to update has already done so.
	if (pauseext)
		return;

	// Get the amount of tics the client can actually run. This accounts for waiting for other
	// players over the network.
	int lowestSequence = INT_MAX;
	for (auto client : NetworkClients)
	{
		if (ClientStates[client].CurrentSequence < lowestSequence)
			lowestSequence = ClientStates[client].CurrentSequence;
	}

	// If the lowest confirmed tic matches the server gametic or greater, allow the client
	// to run some of them.
	const int availableTics = (lowestSequence - gametic / doomcom.ticdup) + 1;

	// Cap the number of tics we can run per frame if we're lagging behind. We don't
	// want the client to accidentally get caught in a loop of trying to catch up.
	int runTics = availableTics;
	if (totalTics < availableTics - 1)
		runTics = totalTics + 1;
	else if (totalTics < availableTics)
		runTics = totalTics;
	
	// If there are no tics to run, check for possible stall conditions and new
	// commands to predict.
	if (runTics <= 0)
	{
		if (!doWait)
			TicStabilityWait();

		// Check if a client dropped connection.
		Net_CheckLastReceived();

		// If we waited long enough to actually advance, re-predict since the client
		// will have built a new user command.
		if (ClientTic > startCommand)
		{
			C_Ticker();
			M_Ticker();
			P_UnPredictPlayer();
			P_PredictPlayer(&players[consoleplayer]);
		}
		return;
	}

	for (int i = 0; i < MAXPLAYERS; ++i)
		players[i].waiting = false;

	// Update the last time the game tic'd.
	LastGameUpdate = EnterTic;

	// Run the available tics.
	P_UnPredictPlayer();
	while (runTics--)
	{
		TicStabilityBegin();

		if (advancedemo)
			D_DoAdvanceDemo();

		C_Ticker();
		M_Ticker();
		G_Ticker();
		++gametic;
		GC::CheckGC();

		TicStabilityEnd();
	}
	P_PredictPlayer(&players[consoleplayer]);
	S_UpdateSounds(players[consoleplayer].camera);	// Update sounds only after predicting the client's newest position.
}

void Net_CheckLastReceived()
{
	constexpr int MaxDelay = TICRATE * 3;

	// [Ed850] Check to see the last time a packet was received.
	// If it's longer then 3 seconds, a node has likely stalled.
	const int time = I_GetTime();
	if (time - LastGameUpdate >= MaxDelay)
	{
		// Try again in the next MaxDelay tics.
		LastGameUpdate = time;

		if (NetMode == NET_PeerToPeer || consoleplayer == Net_Arbitrator)
		{
			// For any clients that are currently lagging behind, send our data back over in case they were having trouble
			// receiving it. We have to do this in case our data is actually the stall condition for the other client.
			for (auto client : NetworkClients)
			{
				if (client == consoleplayer)
					continue;

				if (ClientStates[client].CurrentSequence < gametic)
				{
					ClientStates[client].Flags |= CF_RETRANSMIT;
					players[client].waiting = true;
				}
				else
				{
					players[client].waiting = false;
				}
			}
		}
		else
		{
			// In packet server mode, the client is waiting for data from the host and hasn't recieved it yet. Send
			// our data back over in case the host is waiting for us.
			ClientStates[Net_Arbitrator].Flags |= CF_RETRANSMIT;
			players[Net_Arbitrator].waiting = true;
		}
	}
}

void Net_NewClientTic()
{
	NetEvents.NewClientTic();
}

void Net_Initialize()
{
	NetEvents.InitializeEventData();
}

void Net_WriteInt8(uint8_t it)
{
	NetEvents << it;
}

void Net_WriteInt16(int16_t it)
{
	NetEvents << it;
}

void Net_WriteInt32(int32_t it)
{
	NetEvents << it;
}

void Net_WriteInt64(int64_t it)
{
	NetEvents << it;
}

void Net_WriteFloat(float it)
{
	NetEvents << it;
}

void Net_WriteDouble(double it)
{
	NetEvents << it;
}

void Net_WriteString(const char *it)
{
	NetEvents << it;
}

void Net_WriteBytes(const uint8_t *block, int len)
{
	while (len--)
		NetEvents << *block++;
}

//==========================================================================
//
// Dynamic buffer interface
//
//==========================================================================

FDynamicBuffer::FDynamicBuffer()
{
	m_Data = nullptr;
	m_Len = m_BufferLen = 0;
}

FDynamicBuffer::~FDynamicBuffer()
{
	if (m_Data != nullptr)
	{
		M_Free(m_Data);
		m_Data = nullptr;
	}
	m_Len = m_BufferLen = 0;
}

void FDynamicBuffer::SetData(const uint8_t *data, int len)
{
	if (len > m_BufferLen)
	{
		m_BufferLen = (len + 255) & ~255;
		m_Data = (uint8_t *)M_Realloc(m_Data, m_BufferLen);
	}

	if (data != nullptr)
	{
		m_Len = len;
		memcpy(m_Data, data, len);
	}
	else 
	{
		m_Len = 0;
	}
}

uint8_t *FDynamicBuffer::GetData(int *len)
{
	if (len != nullptr)
		*len = m_Len;
	return m_Len ? m_Data : nullptr;
}

static int RemoveClass(FLevelLocals *Level, const PClass *cls)
{
	AActor *actor;
	int removecount = 0;
	bool player = false;
	auto iterator = Level->GetThinkerIterator<AActor>(cls->TypeName);
	while ((actor = iterator.Next()))
	{
		if (actor->IsA(cls))
		{
			// [MC]Do not remove LIVE players.
			if (actor->player != nullptr)
			{
				player = true;
				continue;
			}
			// [SP] Don't remove owned inventory objects.
			if (!actor->IsMapActor())
				continue;

			removecount++; 
			actor->ClearCounters();
			actor->Destroy();
		}
	}

	if (player)
		Printf("Cannot remove live players!\n");

	return removecount;

}
// [RH] Execute a special "ticcmd". The type byte should
//		have already been read, and the stream is positioned
//		at the beginning of the command's actual data.
void Net_DoCommand(int cmd, uint8_t **stream, int player)
{
	uint8_t pos = 0;
	const char* s = nullptr;
	int i = 0;

	switch (cmd)
	{
	case DEM_SAY:
		{
			const char *name = players[player].userinfo.GetName();
			uint8_t who = ReadInt8(stream);

			s = ReadStringConst(stream);
			if (!(who & 1) || players[player].userinfo.GetTeam() == TEAM_NONE)
			{
				// Said to everyone
				if (who & 2)
					Printf(PRINT_CHAT, TEXTCOLOR_BOLD "* %s" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, s);
				else
					Printf(PRINT_CHAT, "%s" TEXTCOLOR_CHAT ": %s" TEXTCOLOR_CHAT "\n", name, s);

				S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
			}
			else if (players[player].userinfo.GetTeam() == players[consoleplayer].userinfo.GetTeam())
			{
				// Said only to members of the player's team
				if (who & 2)
					Printf(PRINT_TEAMCHAT, TEXTCOLOR_BOLD "* (%s" TEXTCOLOR_BOLD ")%s" TEXTCOLOR_BOLD "\n", name, s);
				else
					Printf(PRINT_TEAMCHAT, "(%s" TEXTCOLOR_TEAMCHAT "): %s" TEXTCOLOR_TEAMCHAT "\n", name, s);

				S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
			}
		}
		break;

	case DEM_MUSICCHANGE:
		S_ChangeMusic(ReadStringConst(stream));
		break;

	case DEM_PRINT:
		Printf("%s", ReadStringConst(stream));
		break;

	case DEM_CENTERPRINT:
		C_MidPrint(nullptr, ReadStringConst(stream));
		break;

	case DEM_UINFCHANGED:
		D_ReadUserInfoStrings(player, stream, true);
		break;

	case DEM_SINFCHANGED:
		D_DoServerInfoChange(stream, false);
		break;

	case DEM_SINFCHANGEDXOR:
		D_DoServerInfoChange(stream, true);
		break;

	case DEM_GIVECHEAT:
		s = ReadStringConst(stream);
		cht_Give(&players[player], s, ReadInt32(stream));
		if (player != consoleplayer)
		{
			FString message = GStrings.GetString("TXT_X_CHEATS");
			message.Substitute("%s", players[player].userinfo.GetName());
			Printf("%s: give %s\n", message.GetChars(), s);
		}
		break;

	case DEM_TAKECHEAT:
		s = ReadStringConst(stream);
		cht_Take(&players[player], s, ReadInt32(stream));
		break;

	case DEM_SETINV:
		s = ReadStringConst(stream);
		i = ReadInt32(stream);
		cht_SetInv(&players[player], s, i, !!ReadInt8(stream));
		break;

	case DEM_WARPCHEAT:
		{
			int x = ReadInt16(stream);
			int y = ReadInt16(stream);
			int z = ReadInt16(stream);
			P_TeleportMove(players[player].mo, DVector3(x, y, z), true);
		}
		break;

	case DEM_GENERICCHEAT:
		cht_DoCheat(&players[player], ReadInt8(stream));
		break;

	case DEM_CHANGEMAP2:
		pos = ReadInt8(stream);
		[[fallthrough]];
	case DEM_CHANGEMAP:
		// Change to another map without disconnecting other players
		s = ReadStringConst(stream);
		// Using LEVEL_NOINTERMISSION tends to throw the game out of sync.
		// That was a long time ago. Maybe it works now?
		primaryLevel->flags |= LEVEL_CHANGEMAPCHEAT;
		primaryLevel->ChangeLevel(s, pos, 0);
		break;

	case DEM_SUICIDE:
		cht_Suicide(&players[player]);
		break;

	case DEM_ADDBOT:
		primaryLevel->BotInfo.TryAddBot(primaryLevel, stream, player);
		break;

	case DEM_KILLBOTS:
		primaryLevel->BotInfo.RemoveAllBots(primaryLevel, true);
		Printf ("Removed all bots\n");
		break;

	case DEM_CENTERVIEW:
		players[player].centering = true;
		break;

	case DEM_INVUSEALL:
		if (gamestate == GS_LEVEL && !paused
			&& players[player].playerstate != PST_DEAD)
		{
			AActor *item = players[player].mo->Inventory;
			auto pitype = PClass::FindActor(NAME_PuzzleItem);
			while (item != nullptr)
			{
				AActor *next = item->Inventory;
				IFVIRTUALPTRNAME(item, NAME_Inventory, UseAll)
				{
					VMValue param[] = { item, players[player].mo };
					VMCall(func, param, 2, nullptr, 0);
				}
				item = next;
			}
		}
		break;

	case DEM_INVUSE:
	case DEM_INVDROP:
		{
			uint32_t which = ReadInt32(stream);
			int amt = -1;
			if (cmd == DEM_INVDROP)
				amt = ReadInt32(stream);

			if (gamestate == GS_LEVEL && !paused
				&& players[player].playerstate != PST_DEAD)
			{
				auto item = players[player].mo->Inventory;
				while (item != nullptr && item->InventoryID != which)
					item = item->Inventory;

				if (item != nullptr)
				{
					if (cmd == DEM_INVUSE)
						players[player].mo->UseInventory(item);
					else
						players[player].mo->DropInventory(item, amt);
				}
			}
		}
		break;

	case DEM_SUMMON:
	case DEM_SUMMONFRIEND:
	case DEM_SUMMONFOE:
	case DEM_SUMMONMBF:
	case DEM_SUMMON2:
	case DEM_SUMMONFRIEND2:
	case DEM_SUMMONFOE2:
		{
			int angle = 0;
			int16_t tid = 0;
			uint8_t special = 0;
			int args[5] = {};

			s = ReadStringConst(stream);
			if (cmd >= DEM_SUMMON2 && cmd <= DEM_SUMMONFOE2)
			{
				angle = ReadInt16(stream);
				tid = ReadInt16(stream);
				special = ReadInt8(stream);
				for (i = 0; i < 5; ++i)
					args[i] = ReadInt32(stream);
			}

			PClassActor* typeinfo = PClass::FindActor(s);
			if (typeinfo != nullptr)
			{
				AActor *source = players[player].mo;
				if (source != nullptr)
				{
					const AActor* def = GetDefaultByType(typeinfo);
					if (def->flags & MF_MISSILE)
					{
						P_SpawnPlayerMissile(source, 0.0, 0.0, 0.0, typeinfo, source->Angles.Yaw);
					}
					else
					{
						DVector3 spawnpos = source->Vec3Angle(def->radius * 2.0 + source->radius, source->Angles.Yaw, 8.0);
						AActor *spawned = Spawn(primaryLevel, typeinfo, spawnpos, ALLOW_REPLACE);
						if (spawned != nullptr)
						{
							spawned->SpawnFlags |= MTF_CONSOLETHING;
							if (cmd == DEM_SUMMONFRIEND || cmd == DEM_SUMMONFRIEND2 || cmd == DEM_SUMMONMBF)
							{
								spawned->ClearCounters();
								spawned->FriendPlayer = player + 1;
								spawned->flags |= MF_FRIENDLY;
								spawned->LastHeard = players[player].mo;
								spawned->health = spawned->SpawnHealth();
								if (cmd == DEM_SUMMONMBF)
									spawned->flags3 |= MF3_NOBLOCKMONST;
							}
							else if (cmd == DEM_SUMMONFOE || cmd == DEM_SUMMONFOE2)
							{
								spawned->FriendPlayer = 0;
								spawned->flags &= ~MF_FRIENDLY;
								spawned->health = spawned->SpawnHealth();
							}

							if (cmd >= DEM_SUMMON2 && cmd <= DEM_SUMMONFOE2)
							{
								spawned->Angles.Yaw = source->Angles.Yaw - DAngle::fromDeg(angle);
								spawned->special = special;
								for(i = 0; i < 5; i++)
									spawned->args[i] = args[i];
								if(tid)
									spawned->SetTID(tid);
							}
						}
					}
				}
			}
		}
		break;

	case DEM_SPRAY:
		s = ReadStringConst(stream);
		SprayDecal(players[player].mo, s);
		break;

	case DEM_MDK:
		s = ReadStringConst(stream);
		cht_DoMDK(&players[player], s);
		break;

	case DEM_PAUSE:
		if (gamestate == GS_LEVEL)
		{
			if (paused)
			{
				paused = 0;
				S_ResumeSound(false);
			}
			else
			{
				paused = player + 1;
				S_PauseSound(false, false);
			}
		}
		break;

	case DEM_SAVEGAME:
		if (gamestate == GS_LEVEL)
		{
			savegamefile = ReadStringConst(stream);
			savedescription = ReadStringConst(stream);
			if (player != consoleplayer)
			{
				// Paths sent over the network will be valid for the system that sent
				// the save command. For other systems, the path needs to be changed.
				FString basename = ExtractFileBase(savegamefile.GetChars(), true);
				savegamefile = G_BuildSaveName(basename.GetChars());
			}
		}
		gameaction = ga_savegame;
		break;

	case DEM_CHECKAUTOSAVE:
		// Do not autosave in multiplayer games or when dead.
		// For demo playback, DEM_DOAUTOSAVE already exists in the demo if the
		// autosave happened. And if it doesn't, we must not generate it.
		if (!netgame && !demoplayback && disableautosave < 2 && autosavecount
			&& players[player].playerstate == PST_LIVE)
		{
			Net_WriteInt8(DEM_DOAUTOSAVE);
		}
		break;

	case DEM_DOAUTOSAVE:
		gameaction = ga_autosave;
		break;

	case DEM_FOV:
		{
			float newfov = ReadFloat(stream);
			if (newfov != players[player].DesiredFOV)
			{
				Printf("FOV%s set to %g\n",
					player == Net_Arbitrator ? " for everyone" : "",
					newfov);
			}

			for (auto client : NetworkClients)
				players[client].DesiredFOV = newfov;
		}
		break;

	case DEM_MYFOV:
		players[player].DesiredFOV = ReadFloat(stream);
		break;

	case DEM_RUNSCRIPT:
	case DEM_RUNSCRIPT2:
		{
			int snum = ReadInt16(stream);
			int argn = ReadInt8(stream);
			RunScript(stream, players[player].mo, snum, argn, (cmd == DEM_RUNSCRIPT2) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNNAMEDSCRIPT:
		{
			s = ReadStringConst(stream);
			int argn = ReadInt8(stream);
			RunScript(stream, players[player].mo, -FName(s).GetIndex(), argn & 127, (argn & 128) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNSPECIAL:
		{
			int snum = ReadInt16(stream);
			int argn = ReadInt8(stream);
			int arg[5] = {};

			for (i = 0; i < argn; ++i)
			{
				int argval = ReadInt32(stream);
				if ((unsigned)i < countof(arg))
					arg[i] = argval;
			}

			if (!CheckCheatmode(player == consoleplayer))
				P_ExecuteSpecial(primaryLevel, snum, nullptr, players[player].mo, false, arg[0], arg[1], arg[2], arg[3], arg[4]);
		}
		break;

	case DEM_CROUCH:
		if (gamestate == GS_LEVEL && players[player].mo != nullptr
			&& players[player].playerstate == PST_LIVE && !(players[player].oldbuttons & BT_JUMP)
			&& !P_IsPlayerTotallyFrozen(&players[player]))
		{
			players[player].crouching = players[player].crouchdir < 0 ? 1 : -1;
		}
		break;

	case DEM_MORPHEX:
		{
			s = ReadStringConst(stream);
			FString msg = cht_Morph(players + player, PClass::FindActor(s), false);
			if (player == consoleplayer)
				Printf("%s\n", msg[0] != '\0' ? msg.GetChars() : "Morph failed.");
		}
		break;

	case DEM_ADDCONTROLLER:
		{
			uint8_t playernum = ReadInt8(stream);
			players[playernum].settings_controller = true;
			if (consoleplayer == playernum || consoleplayer == Net_Arbitrator)
				Printf("%s has been added to the controller list.\n", players[playernum].userinfo.GetName());
		}
		break;

	case DEM_DELCONTROLLER:
		{
			uint8_t playernum = ReadInt8(stream);
			players[playernum].settings_controller = false;
			if (consoleplayer == playernum || consoleplayer == Net_Arbitrator)
				Printf("%s has been removed from the controller list.\n", players[playernum].userinfo.GetName());
		}
		break;

	case DEM_KILLCLASSCHEAT:
		{
			s = ReadStringConst(stream);
			int killcount = 0;
			PClassActor *cls = PClass::FindActor(s);

			if (cls != nullptr)
			{
				killcount = primaryLevel->Massacre(false, cls->TypeName);
				PClassActor *cls_rep = cls->GetReplacement(primaryLevel);
				if (cls != cls_rep)
					killcount += primaryLevel->Massacre(false, cls_rep->TypeName);

				Printf("Killed %d monsters of type %s.\n", killcount, s);
			}
			else
			{
				Printf("%s is not an actor class.\n", s);
			}
		}
		break;

	case DEM_REMOVE:
		{
			s = ReadStringConst(stream);
			int removecount = 0;
			PClassActor *cls = PClass::FindActor(s);
			if (cls != nullptr && cls->IsDescendantOf(RUNTIME_CLASS(AActor)))
			{
				removecount = RemoveClass(primaryLevel, cls);
				const PClass *cls_rep = cls->GetReplacement(primaryLevel);
				if (cls != cls_rep)
					removecount += RemoveClass(primaryLevel, cls_rep);

				Printf("Removed %d actors of type %s.\n", removecount, s);
			}
			else
			{
				Printf("%s is not an actor class.\n", s);
			}
		}
		break;

	case DEM_CONVREPLY:
	case DEM_CONVCLOSE:
	case DEM_CONVNULL:
		P_ConversationCommand(cmd, player, stream);
		break;

	case DEM_SETSLOT:
	case DEM_SETSLOTPNUM:
		{
			int pnum = player;
			if (cmd == DEM_SETSLOTPNUM)
				pnum = ReadInt8(stream);

			unsigned int slot = ReadInt8(stream);
			int count = ReadInt8(stream);
			if (slot < NUM_WEAPON_SLOTS)
				players[pnum].weapons.ClearSlot(slot);

			for (i = 0; i < count; ++i)
			{
				PClassActor *wpn = Net_ReadWeapon(stream);
				players[pnum].weapons.AddSlot(slot, wpn, pnum == consoleplayer);
			}
		}
		break;

	case DEM_ADDSLOT:
		{
			int slot = ReadInt8(stream);
			PClassActor *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlot(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_ADDSLOTDEFAULT:
		{
			int slot = ReadInt8(stream);
			PClassActor *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlotDefault(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_SETPITCHLIMIT:
		players[player].MinPitch = DAngle::fromDeg(-ReadInt8(stream));		// up
		players[player].MaxPitch = DAngle::fromDeg(ReadInt8(stream));		// down
		break;

	case DEM_REVERTCAMERA:
		players[player].camera = players[player].mo;
		break;

	case DEM_FINISHGAME:
		// Simulate an end-of-game action
		primaryLevel->ChangeLevel(nullptr, 0, 0);
		break;

	case DEM_NETEVENT:
		{
			s = ReadStringConst(stream);
			int argn = ReadInt8(stream);
			int arg[3] = { 0, 0, 0 };
			for (int i = 0; i < 3; i++)
				arg[i] = ReadInt32(stream);
			bool manual = !!ReadInt8(stream);
			primaryLevel->localEventManager->Console(player, s, arg[0], arg[1], arg[2], manual, false);
		}
		break;

	case DEM_ENDSCREENJOB:
		EndScreenJob();
		break;

	case DEM_ZSC_CMD:
		{
			FName cmd = ReadStringConst(stream);
			unsigned int size = ReadInt16(stream);

			TArray<uint8_t> buffer;
			if (size)
			{
				buffer.Grow(size);
				for (unsigned int i = 0u; i < size; ++i)
					buffer.Push(ReadInt8(stream));
			}

			FNetworkCommand netCmd = { player, cmd, buffer };
			primaryLevel->localEventManager->NetCommand(netCmd);
		}
		break;

	case DEM_CHANGESKILL:
		NextSkill = ReadInt32(stream);
		break;
		
	default:
		I_Error("Unknown net command: %d", cmd);
		break;
	}
}

// Used by DEM_RUNSCRIPT, DEM_RUNSCRIPT2, and DEM_RUNNAMEDSCRIPT
static void RunScript(uint8_t **stream, AActor *pawn, int snum, int argn, int always)
{
	// Scripts can be invoked without a level loaded, e.g. via puke(name) CCMD in fullscreen console
	if (pawn == nullptr)
		return;

	int arg[4] = {};
	for (int i = 0; i < argn; ++i)
	{
		int argval = ReadInt32(stream);
		if ((unsigned)i < countof(arg))
			arg[i] = argval;
	}

	P_StartScript(pawn->Level, pawn, nullptr, snum, primaryLevel->MapName.GetChars(), arg, min<int>(countof(arg), argn), ACS_NET | always);
}

// TODO: This really needs to be replaced with some kind of packet system that can simply read through packets and opt
// not to execute them. Right now this is making setting up net commands a nightmare.
// Reads through the network stream but doesn't actually execute any command. Used for getting the size of a stream.
// The skip amount is the number of bytes the command possesses. This should mirror the bytes in Net_DoCommand().
void Net_SkipCommand(int cmd, uint8_t **stream)
{
	size_t skip = 0;
	switch (cmd)
	{
		case DEM_SAY:
			skip = strlen((char *)(*stream + 1)) + 2;
			break;

		case DEM_ADDBOT:
			skip = strlen((char *)(*stream + 1)) + 6;
			break;

		case DEM_GIVECHEAT:
		case DEM_TAKECHEAT:
			skip = strlen((char *)(*stream)) + 5;
			break;

		case DEM_SETINV:
			skip = strlen((char *)(*stream)) + 6;
			break;

		case DEM_NETEVENT:
			skip = strlen((char *)(*stream)) + 15;
			break;

		case DEM_ZSC_CMD:
			skip = strlen((char*)(*stream)) + 1;
			skip += (((*stream)[skip] << 8) | (*stream)[skip + 1]) + 2;
			break;

		case DEM_SUMMON2:
		case DEM_SUMMONFRIEND2:
		case DEM_SUMMONFOE2:
			skip = strlen((char *)(*stream)) + 26;
			break;
		case DEM_CHANGEMAP2:
			skip = strlen((char *)(*stream + 1)) + 2;
			break;
		case DEM_MUSICCHANGE:
		case DEM_PRINT:
		case DEM_CENTERPRINT:
		case DEM_UINFCHANGED:
		case DEM_CHANGEMAP:
		case DEM_SUMMON:
		case DEM_SUMMONFRIEND:
		case DEM_SUMMONFOE:
		case DEM_SUMMONMBF:
		case DEM_REMOVE:
		case DEM_SPRAY:
		case DEM_MORPHEX:
		case DEM_KILLCLASSCHEAT:
		case DEM_MDK:
			skip = strlen((char *)(*stream)) + 1;
			break;

		case DEM_WARPCHEAT:
			skip = 6;
			break;

		case DEM_INVUSE:
		case DEM_FOV:
		case DEM_MYFOV:
		case DEM_CHANGESKILL:
			skip = 4;
			break;

		case DEM_INVDROP:
			skip = 8;
			break;

		case DEM_GENERICCHEAT:
		case DEM_DROPPLAYER:
		case DEM_ADDCONTROLLER:
		case DEM_DELCONTROLLER:
			skip = 1;
			break;

		case DEM_SAVEGAME:
			skip = strlen((char *)(*stream)) + 1;
			skip += strlen((char *)(*stream) + skip) + 1;
			break;

		case DEM_SINFCHANGEDXOR:
		case DEM_SINFCHANGED:
			{
				uint8_t t = **stream;
				skip = 1 + (t & 63);
				if (cmd == DEM_SINFCHANGED)
				{
					switch (t >> 6)
					{
					case CVAR_Bool:
						skip += 1;
						break;
					case CVAR_Int:
					case CVAR_Float:
						skip += 4;
						break;
					case CVAR_String:
						skip += strlen((char*)(*stream + skip)) + 1;
						break;
					}
				}
				else
				{
					skip += 1;
				}
			}
			break;

		case DEM_RUNSCRIPT:
		case DEM_RUNSCRIPT2:
			skip = 3 + *(*stream + 2) * 4;
			break;

		case DEM_RUNNAMEDSCRIPT:
			skip = strlen((char *)(*stream)) + 2;
			skip += ((*(*stream + skip - 1)) & 127) * 4;
			break;

		case DEM_RUNSPECIAL:
			skip = 3 + *(*stream + 2) * 4;
			break;

		case DEM_CONVREPLY:
			skip = 3;
			break;

		case DEM_SETSLOT:
		case DEM_SETSLOTPNUM:
			{
				skip = 2 + (cmd == DEM_SETSLOTPNUM);
				for (int numweapons = (*stream)[skip-1]; numweapons > 0; --numweapons)
					skip += 1 + ((*stream)[skip] >> 7);
			}
			break;

		case DEM_ADDSLOT:
		case DEM_ADDSLOTDEFAULT:
			skip = 2 + ((*stream)[1] >> 7);
			break;

		case DEM_SETPITCHLIMIT:
			skip = 2;
			break;
	}

	*stream += skip;
}

// This was taken out of shared_hud, because UI code shouldn't do low level calculations that may change if the backing implementation changes.
int Net_GetLatency(int* localDelay, int* arbitratorDelay)
{
	int consoleDelay = 0, arbiDelay = 0;
	for (int i = 0; i < BACKUPTICS; ++i)
	{
		//arbiDelay += ClientStates[Net_Arbitrator].Delay[i];
		//consoleDelay += ClientStates[consoleplayer].Delay[i];
	}

	const int gameDelayMs = max<int>(consoleDelay, arbiDelay) * doomcom.ticdup * 1000 / (BACKUPTICS * TICRATE);
	int severity = 0;
	if (gameDelayMs >= 160)
		severity = 3;
	else if (gameDelayMs >= 120)
		severity = 2;
	else if (gameDelayMs >= 80)
		severity = 1;
	
	*localDelay = consoleDelay;
	*arbitratorDelay = arbiDelay;
	return severity;
}

//==========================================================================
//
//
//
//==========================================================================

// [RH] List "ping" times
CCMD(pings)
{
	// In Packet Server mode, this displays the latency each individual client has to the host
	for (auto client : NetworkClients)
	{
		if (client != consoleplayer || (NetMode == NET_PacketServer && client != Net_Arbitrator))
			Printf("% 4" PRId64 "ms %s\n", I_msTime() - ClientStates[client].SentTime, players[client].userinfo.GetName());
	}
}

//==========================================================================
//
// Network_Controller
//
// Implement players who have the ability to change settings in a network
// game.
//
//==========================================================================

static void Network_Controller(int pNum, bool add)
{
	if (!netgame)
	{
		Printf("This command can only be used when playing a net game.\n");
		return;
	}

	if (consoleplayer != Net_Arbitrator)
	{
		Printf("This command is only accessible to the host.\n");
		return;
	}

	if (pNum == Net_Arbitrator)
	{
		Printf("The host cannot change their own settings controller status.\n");
		return;
	}

	if (!NetworkClients.InGame(pNum))
	{
		Printf("Player %d is not a valid client\n", pNum);
		return;
	}

	if (players[pNum].settings_controller && add)
	{
		Printf("%s is already on the setting controller list.\n", players[pNum].userinfo.GetName());
		return;
	}

	if (!players[pNum].settings_controller && !add)
	{
		Printf("%s is not on the setting controller list.\n", players[pNum].userinfo.GetName());
		return;
	}

	Net_WriteInt8(add ? DEM_ADDCONTROLLER : DEM_DELCONTROLLER);
	Net_WriteInt8(pNum);
}

//==========================================================================
//
// CCMD net_addcontroller
//
//==========================================================================

CCMD(net_addcontroller)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: net_addcontroller <player num>\n");
		return;
	}

	Network_Controller(atoi (argv[1]), true);
}

//==========================================================================
//
// CCMD net_removecontroller
//
//==========================================================================

CCMD(net_removecontroller)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: net_removecontroller <player num>\n");
		return;
	}

	Network_Controller(atoi(argv[1]), false);
}

//==========================================================================
//
// CCMD net_listcontrollers
//
//==========================================================================

CCMD(net_listcontrollers)
{
	if (!netgame)
	{
		Printf ("This command can only be used when playing a net game.\n");
		return;
	}

	Printf("The following players can change the game settings:\n");

	for (auto client : NetworkClients)
	{
		if (players[client].settings_controller)
			Printf("- %s\n", players[client].userinfo.GetName());
	}
}
