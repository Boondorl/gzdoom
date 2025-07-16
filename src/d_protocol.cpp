/*
** d_protocol.cpp
** Basic network packet creation routines and simple IFF parsing
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

#include "d_protocol.h"
#include "d_net.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "serializer.h"
#include "d_netpackets.h"

FSerializer& Serialize(FSerializer& arc, const char* key, usercmd_t& cmd, usercmd_t* def)
{
	// This used packed data with the old serializer but that's totally counterproductive when
	// having a text format that is human-readable. So this compression has been undone here.
	// The few bytes of file size it saves are not worth the obfuscation.
	if (arc.BeginObject(key))
	{
		arc("buttons", cmd.buttons)
			("pitch", cmd.pitch)
			("yaw", cmd.yaw)
			("roll", cmd.roll)
			("forwardmove", cmd.forwardmove)
			("sidemove", cmd.sidemove)
			("upmove", cmd.upmove)
			.EndObject();
	}
	return arc;
}

void WriteUserCommand(usercmd_t& cmd, const usercmd_t* basis, WriteStream& stream)
{
	auto p = UserCommandPacket(cmd, basis);
	if (!p.HasCommand())
	{
		auto e = EmptyUserCommandPacket::GetDefault();
		WritePacket(e, stream);
	}
	else
	{
		WritePacket(p, stream);
	}
}

// Reads through the user command without actually setting any of its info. Used to get the size
// of the command when getting the length of the stream.
void SkipUserCommand(ReadStream& stream)
{
	if (GetPacketType(stream) == DEM_EMPTYUSERCMD)
	{
		auto p = EmptyUserCommandPacket::GetDefault();
		SkipPacket(p, stream);
	}
	else
	{
		auto p = UserCommandPacket::GetDefault();
		SkipPacket(p, stream);
	}
}

void ReadUserCommand(ReadStream& stream, int player, int tic)
{
	const int ticMod = tic % BACKUPTICS;

	auto& curTic = ClientStates[player].Tics[ticMod];
	usercmd_t& ticCmd = curTic.Command;

	// Skip until we reach the player command. Event data will get read off once the
	// tick is actually executed.
	stream.StartMeasuring();
	Net_SkipCommands(stream);
	curTic.Data.WriteBytes(stream.GetMeasuredData());
	stream.ClearMeasurement();

	auto basis = tic > 0 ? &ClientStates[player].Tics[(tic - 1) % BACKUPTICS].Command : nullptr;
	if (GetPacketType(stream) == DEM_EMPTYUSERCMD)
	{
		auto p = EmptyUserCommandPacket(ticCmd, basis);
		ReadPacket(p, stream);
	}
	else
	{
		auto p = UserCommandPacket(ticCmd, basis);
		ReadPacket(p, stream);
	}
}

void RunPlayerEventData(int player, int tic)
{
	// We don't have the full command yet, so don't run it.
	if (gametic % TicDup)
		return;

	auto& data = ClientStates[player].Tics[tic % BACKUPTICS].Data;
	if (data.Size())
	{
		ReadStream r = { data.GetView() };
		Net_ReadCommands(player, r);
		if (!IsRecordingDemo())
			data.Clear();
	}
}

// Demo related functionality

void WriteDemoChunk(int32_t id, const TArrayView<const uint8_t> data, DynamicWriteStream& stream)
{
	stream.WriteValue<int32_t>(id);
	// TArrays can't go beyond 32-bit unsigned for ZScript compat so
	// writing this out as a chunk is pointless.
	stream.WriteLargeArray<uint8_t>(data);
	if (data.Size() & 1)
		stream.WriteValue<uint8_t>(0u);
}

TArrayView<const uint8_t> ReadDemoChunk(int32_t& id, ReadStream& stream)
{
	if (!stream.SerializeInt32(id))
		return { nullptr, 0u };
	TArrayView<const uint8_t> data = { nullptr, 0u };
	if (!stream.SerializeLargeArray<uint8_t>(data, 0u, false))
		return { nullptr, 0u };
	uint8_t pad = 0u;
	if ((data.Size() & 1) && !stream.SerializeUInt8(pad))
		return { nullptr, 0u };
	return data;
}
