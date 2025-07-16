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
#include "d_netpackets.h"

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
#define WEAP_ID		BIGE_ID('W','E','A','P')

constexpr int MoreButtons = 0x80;

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

// These should never actually get executed. If they do, throw an error.
class EmptyUserCommandPacket : public EmptyPacket
{
	DEFINE_NETPACKET(EmptyUserCommandPacket, EmptyPacket, DEM_EMPTYUSERCMD, 0);
public:
	EmptyUserCommandPacket(usercmd_t& cmd, const usercmd_t* basis) : EmptyUserCommandPacket()
	{
		if (basis != nullptr)
			memcpy(&cmd, basis, sizeof(usercmd_t));
		else
			memset(&cmd, 0, sizeof(usercmd_t));
	}
};

NETPACKET_EXECUTE(EmptyUserCommandPacket)
{
	return false;
}

class UserCommandPacket : public NetPacket
{
	DEFINE_NETPACKET(UserCommandPacket, NetPacket, DEM_USERCMD, 1);
	static constexpr size_t CommandSize = 17u; // Normally can't be bigger than this, so error out if it was.
	static constexpr usercmd_t Blank = {};
	usercmd_t* _cmd = nullptr;
	const usercmd_t* _basis = nullptr;
	// This requires some special handling, so split the reading and writing operations entirely.
	bool Serialize(MeasureStream& stream, size_t& argCount)	override
	{
		TArrayView<const uint8_t> temp;
		SERIALIZE_SMALL_ARRAY_EXPECTING(temp, CommandSize, false);
		return true;
	}
	bool Serialize(WriteStream& stream, size_t& argCount) override
	{
		return SerializeWrite<WriteStream>(stream, argCount);
	}
	bool Serialize(DynamicWriteStream& stream, size_t& argCount) override
	{
		return SerializeWrite<DynamicWriteStream>(stream, argCount);
	}
	template<typename Stream>
	bool SerializeWrite(Stream& stream, size_t& argCount)
	{
		if (_basis == nullptr)
			_basis = &Blank;

		DynamicWriteStream w;
		const uint32_t buttonDelta = _cmd->buttons ^ _basis->buttons;
		if (buttonDelta)
		{
			// We can support up to 29 buttons using 1 to 4 bytes to store them. The most
			// significant bit of each button byte is a flag to indicate whether or not more buttons
			// should be read in excluding the last one which supports all 8 bits.
			buffer[0] |= UCMDF_BUTTONS;
			const uint8_t buttons[4] = { uint8_t(_cmd->buttons & 0x7F),
										uint8_t((_cmd->buttons >> 7) & 0x7F),
										uint8_t((_cmd->buttons >> 14) & 0x7F),
										uint8_t((_cmd->buttons >> 21) & 0xFF) };
			// Boon TODO: Clean this up a bit.
			w.WriteValue<uint8_t>(buttons[0]);
			if (buttonDelta & 0xFFFFFF80)
			{
				buffer[1] |= MoreButtons;
				w.WriteValue<uint8_t>(buttons[1]);
				if (buttonDelta & 0xFFFFC000)
				{
					buffer[2] |= MoreButtons;
					w.WriteValue<uint8_t>(buttons[2]);
					if (buttonDelta & 0xFFE00000)
					{
						buffer[3] |= MoreButtons;
						w.WriteValue<uint8_t>(buttons[3]);
					}
				}
			}
		}
		if (_cmd->pitch != _basis->pitch)
		{
			buffer[0] |= UCMDF_PITCH;
			w.WriteValue<int16_t>(_cmd->pitch);
		}
		if (cmd->yaw != basis->yaw)
		{
			buffer[0] |= UCMDF_YAW;
			w.WriteValue<int16_t>(_cmd->yaw);
		}
		if (cmd->forwardmove != basis->forwardmove)
		{
			buffer[0] |= UCMDF_FORWARDMOVE;
			w.WriteValue<int16_t>(_cmd->forwardmove);
		}
		if (cmd->sidemove != basis->sidemove)
		{
			buffer[0] |= UCMDF_SIDEMOVE;
			w.WriteValue<int16_t>(_cmd->sidemove);
		}
		if (cmd->upmove != basis->upmove)
		{
			buffer[0] |= UCMDF_UPMOVE;
			w.WriteValue<int16_t>(_cmd->upmove);
		}
		if (cmd->roll != basis->roll)
		{
			buffer[0] |= UCMDF_ROLL;
			w.WriteValue<int16_t>(_cmd->roll);
		}

		// Rather than using dynamic write commands, send this out as an array so it can
		// be properly size checked on the other side.
		SERIALIZE_SMALL_ARRAY_EXPECTING(w.GetView(), CommandSize, false);
		return true;
	}
	bool Serialize(ReadStream& stream, size_t& argCount)
	{
		TArrayView<const uint8_t> data;
		SERIALIZE_SMALL_ARRAY_EXPECTING(data, CommandSize, false);
		if (!data.Size()) // Flags should always be present here.
			return false;

		assert(data[0] != 0);
		ReadStream r = { data };
		// Boon TODO: This probably breaks with demos? (What doesn't)
		if (_basis != nullptr && _cmd != _basis)
			memcpy(&_cmd, _basis, sizeof(usercmd_t));

		if (data[0] & UCMDF_BUTTONS)
		{
			uint8_t in = 0u;
			r.SerializeUInt8(in);
			uint32_t buttons = (_cmd->buttons & ~0x7F) | (in & 0x7F);
			if (in & MoreButtons)
			{
				r.SerializeUInt8(in);
				buttons = (buttons & ~(0x7F << 7)) | ((in & 0x7F) << 7);
				if (in & MoreButtons)
				{
					r.SerializeUInt8(in);
					buttons = (buttons & ~(0x7F << 14)) | ((in & 0x7F) << 14);
					if (in & MoreButtons)
					{
						r.SerializeUInt8(in);
						buttons = (buttons & ~(0xFF << 21)) | (in << 21);
					}
				}
			}
			_cmd->buttons = buttons;
		}
		if (data[0] & UCMDF_PITCH)
			r.SerializeInt16(_cmd->pitch);
		if (data[0] & UCMDF_YAW)
			r.SerializeInt16(_cmd->yaw);
		if (data[0] & UCMDF_FORWARDMOVE)
			r.SerializeInt16(_cmd->forwardmove);
		if (data[0] & UCMDF_SIDEMOVE)
			r.SerializeInt16(_cmd->sidemove);
		if (data[0] & UCMDF_UPMOVE)
			r.SerializeInt16(_cmd->upmove);
		if (data[0] & UCMDF_ROLL)
			r.SerializeInt16(_cmd->roll);
		return true;
	}
public:
	UserCommandPacket(usercmd_t& cmd, const usercmd_t* basis) : UserCommandPacket()
	{
		_cmd = &cmd;
		_basis = basis;
	}
	bool HasCommand() const
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
};

NETPACKET_EXECUTE(UserCommandPacket)
{
	return false;
}

void WriteUserCommand(usercmd_t& cmd, const usercmd_t* basis, WriteStream& stream);
void ReadUserCommand(ReadStream& stream, int player, int tic);
void SkipUserCommand(ReadStream& stream);

void WriteDemoChunk(int32_t id, const TArrayView<const uint8_t> data, DynamicWriteStream& stream);
TArrayView<const uint8_t> ReadDemoChunk(int32_t& id, ReadStream& stream);

void RunPlayerEventData(int player, int tic);

#endif //__D_PROTOCOL_H__
