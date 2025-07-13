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

class UserCommandPacket : public NetPacket
{
	// Intentionally has no args, this should never be serialized in the event reader.
	DEFINE_NETPACKET(UserCommandPacket, NetPacket, DEM_USERCMD, 0)
	usercmd_t* _cmd = nullptr;
	const usercmd_t* _basis = nullptr;
	DEFINE_NETPACKET_SERIALIZER()
	{
		if (Stream::IsWriting)
			return WriteCommand(stream, argCount);
		else
			return ReadCommand(stream, argCount);
	}
	// This requires some special handling, so split the reading and writing operations entirely.
	template<typename Stream>
	bool WriteCommand(Stream& stream, size_t& argCount)
	{
		usercmd_t blank;
		if (_basis == nullptr)
		{
			memset(&blank, 0, sizeof(blank));
			_basis = &blank;
		}

		uint8_t flags = 0u;
		uint8_t buttons[4] = {};
		const uint32_t buttonDelta = _cmd.buttons ^ _basis->buttons;
		if (buttonDelta)
		{
			// We can support up to 29 buttons using 1 to 4 bytes to store them. The most
			// significant bit of each button byte is a flag to indicate whether or not more buttons
			// should be read in excluding the last one which supports all 8 bits.
			flags |= UCMDF_BUTTONS;
			buttons[4] = { uint8_t(_cmd.buttons & 0x7F),
						uint8_t((_cmd.buttons >> 7) & 0x7F),
						uint8_t((_cmd.buttons >> 14) & 0x7F),
						uint8_t((_cmd.buttons >> 21) & 0xFF) };
			if (buttonDelta & 0xFFFFFF80)
			{
				buttons[0] |= MoreButtons;
				if (buttonDelta & 0xFFFFC000)
				{
					buttons[1] |= MoreButtons;
					if (buttonDelta & 0xFFE00000)
						buttons[2] |= MoreButtons;
				}
			}
		}
		if (_cmd.pitch != _basis->pitch)
			flags |= UCMDF_PITCH;
		if (cmd.yaw != basis->yaw)
			flags |= UCMDF_YAW;
		if (cmd.forwardmove != basis->forwardmove)
			flags |= UCMDF_FORWARDMOVE;
		if (cmd.sidemove != basis->sidemove)
			flags |= UCMDF_SIDEMOVE;
		if (cmd.upmove != basis->upmove)
			flags |= UCMDF_UPMOVE;
		if (cmd.roll != basis->roll)
			flags |= UCMDF_ROLL;

		// Write the packing bits
		SERIALIZE_UINT8(flags);
		if (!flags)
			return true;

		if (flags & UCMDF_BUTTONS)
		{
			SERIALIZE_UINT8(buttons[0]);
			if (buttons[0] & MoreButtons)
			{
				SERIALIZE_UINT8(buttons[1]);
				if (buttons[1] & MoreButtons)
				{
					SERIALIZE_UINT8(buttons[2]);
					if (buttons[2] & MoreButtons)
						SERIALIZE_UINT8(buttons[3]);
				}
			}
		}
		if (flags & UCMDF_PITCH)
			SERIALIZE_INT16(_cmd.pitch);
		if (flags & UCMDF_YAW)
			SERIALIZE_INT16(_cmd.yaw);
		if (flags & UCMDF_FORWARDMOVE)
			SERIALIZE_INT16(_cmd.forwardmove);
		if (flags & UCMDF_SIDEMOVE)
			SERIALIZE_INT16(_cmd.sidemove);
		if (flags & UCMDF_UPMOVE)
			SERIALIZE_INT16(_cmd.upmove);
		if (flags & UCMDF_ROLL)
			SERIALIZE_INT16(_cmd.roll);
		return true;
	}
	template<typename Stream>
	bool ReadCommand(Stream& stream, size_t& argCount)
	{
		uint8_t flags = 0u;
		SERIALIZE_UINT8(flags);
		if (!flags)
			return true;

		// Can happen when measuring.
		if (Stream::IsReading)
		{
			if (_basis != nullptr)
				memcpy(&_cmd, _basis, sizeof(usercmd_t));
		}

		if (flags & UCMDF_BUTTONS)
		{
			uint8_t in = 0u;
			SERIALIZE_UINT8(in);
			const uint32_t buttons = (_cmd.buttons & ~0x7F) | (in & 0x7F);
			if (in & MoreButtons)
			{
				SERIALIZE_UINT8(in);
				buttons = (buttons & ~(0x7F << 7)) | ((in & 0x7F) << 7);
				if (in & MoreButtons)
				{
					SERIALIZE_UINT8(in);
					buttons = (buttons & ~(0x7F << 14)) | ((in & 0x7F) << 14);
					if (in & MoreButtons)
					{
						SERIALIZE_UINT8(in);
						buttons = (buttons & ~(0xFF << 21)) | (in << 21);
					}
				}
			}
			if (Stream::IsReading)
				_cmd.buttons = buttons;
		}
		if (flags & UCMDF_PITCH)
			SERIALIZE_INT16(_cmd.pitch);
		if (flags & UCMDF_YAW)
			SERIALIZE_INT16(_cmd.yaw);
		if (flags & UCMDF_FORWARDMOVE)
			SERIALIZE_INT16(_cmd.forwardmove);
		if (flags & UCMDF_SIDEMOVE)
			SERIALIZE_INT16(_cmd.sidemove);
		if (flags & UCMDF_UPMOVE)
			SERIALIZE_INT16(_cmd.upmove);
		if (flags & UCMDF_ROLL)
			SERIALIZE_INT16(_cmd.roll);
		return true;
	}
public:
	UserCommandPacket(usercmd_t& cmd, const usercmd_t* const basis) : UserCommandPacket()
	{
		_cmd = &cmd;
		_basis = basis;
	}
	bool ShouldWrite() const override
	{
		if (_basis == nullptr)
		{
			return _cmd->buttons
					|| _cmd->pitch || _cmd->yaw || _cmd->roll
					|| _cmd->forwardmove || _cmd->sidemove || _cmd->upmove;
		}
		return _cmd->buttons != _basis->buttons
				|| _cmd->yaw != _basis->yaw || _cmd->pitch != _basis->pitch || _cmd->roll != _basis->roll
				|| _cmd->forwardmove != _basis->forwardmove || _cmd->sidemove != _basis->sidemove || _cmd->upmove != _basis->upmove;
	}
	// Intentionally leave the arg count out. If this gets called from the normal stream
	// reader it'll error out.
	void WriteUserCommand(TArrayView<uint8_t>& stream)
	{
		WriteStream w = { stream };
		if (!ShouldWrite())
		{
			if (!w.SerializeUInt8(DEM_EMPTYUSERCMD))
				return;
		}
		else
		{
			if (!w.SerializeUInt8(NetCommand))
				return;
			size_t temp = 0u;
			if (!Serialize(w, temp))
				return;
		}
		const size_t bytes = w.GetWrittenBytes();
		assert(bytes <= stream.Size());
		stream = { stream.Data() + bytes, stream.Size() - bytes };
	}
	void ReadUserCommand(TArrayView<const uint8_t>& stream)
	{
		ReadStream r = { stream };
		uint8_t type = 0u;
		if (!r.SerializeUInt8(type))
			return;
		if (type == NetCommand)
		{
			size_t temp = 0u;
			if (!Serialize(r, temp))
				return;
		}
		else if (type == DEM_EMPTYUSERCMD)
		{
			if (_basis != nullptr)
				memcpy(_cmd, _basis, sizeof(usercmd_t));
			else
				memset(_cmd, 0, sizeof(usercmd_t));
		}
		else
		{
			return;
		}
		const size_t bytes = r.GetReadBytes();
		assert(bytes <= stream.Size());
		stream = { stream.Data() + bytes, stream.Size() - bytes };
	}
	void SkipUserCommand(TArrayView<const uint8_t>& stream)
	{
		MeasureStream m = {};
		uint8_t type = 0u;
		if (!m.SerializeUInt8(type))
			return;
		if (type == NetCommand)
		{
			size_t temp = 0u;
			if (!Serialize(m, temp))
				return;
		}
		else if (type != DEM_EMPTYUSERCMD)
		{
			return;
		}
		const size_t bytes = m.GetTotalBytes();
		assert(bytes <= stream.Size());
		stream = { stream.Data() + bytes, stream.Size() - bytes };
	}
};

void WriteUserCommand(usercmd_t& cmd, const usercmd_t* basis, TArrayView<uint8_t>& stream)
{
	UserCommandPacket(cmd, basis).WriteUserCommand(stream);
}

// Reads through the user command without actually setting any of its info. Used to get the size
// of the command when getting the length of the stream.
void SkipUserCommand(TArrayView<const uint8_t>& stream)
{
	usercmd_t cmd = {};
	UserCommandPacket(cmd, nullptr).SkipUserCommand(stream);
}

void ReadUserCommand(TArrayView<const uint8_t>& stream, int player, int tic)
{
	const int ticMod = tic % BACKUPTICS;

	auto& curTic = ClientStates[player].Tics[ticMod];
	usercmd_t& ticCmd = curTic.Command;

	const auto start = stream;

	// Skip until we reach the player command. Event data will get read off once the
	// tick is actually executed.
	Net_SkipCommands(stream);

	// Subtract a byte to account for the fact the stream head is now sitting on the
	// user command.
	curTic.AddData({ start.Data(), start.Size() - stream.Size() });
	UserCommandPacket(ticCmd, tic > 0 ? &ClientStates[player].Tics[(tic - 1) % BACKUPTICS].Command : nullptr).ReadUserCommand(stream);
}

void RunPlayerEventData(int player, int tic)
{
	// We don't have the full command yet, so don't run it.
	if (gametic % TicDup)
		return;

	auto& data = ClientStates[player].Tics[tic % BACKUPTICS].Data;
	if (data.Size())
	{
		TArrayView<const uint8_t> stream = { data.Data(), data.Size() };
		Net_ReadCommands(player, stream);
		if (!demorecording)
			data.Clear();
	}
}

// Demo related functionality

uint8_t* streamPos = nullptr;

// Write the header of an IFF chunk and leave space
// for the length field.
void StartChunk(int id, TArrayView<uint8_t>& stream)
{
	WriteInt32(id, stream);
	streamPos = stream.Data();
	AdvanceStream(stream, 4);
}

// Write the length field for the chunk and insert
// pad byte if the chunk is odd-sized.
void FinishChunk(TArrayView<uint8_t>& stream)
{
	if (streamPos == nullptr)
		return;

	int len = int(stream.Data() - streamPos - 4);
	auto streamPosView = TArrayView<uint8_t>(streamPos, 4);
	WriteInt32(len, streamPosView);
	if (len & 1)
		WriteInt8(0, stream);

	streamPos = nullptr;
}

// Skip past an unknown chunk. *stream should be
// pointing to the chunk's length field.
void SkipChunk(TArrayView<uint8_t>& stream)
{
	int len = ReadInt32(stream);
	AdvanceStream(stream, len + (len & 1));
}
