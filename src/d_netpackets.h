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
//		Packets for demo commands.
//
//-----------------------------------------------------------------------------


#ifndef __D_NETPACKET__
#define __D_NETPACKET__

#include "i_protocol.h"

#define xx(cmd, packet) DEM_##cmd

enum EDemoCommand : uint8_t
{
	DEM_INVALID,	//  0 Intentionally has no packet
	DEM_USERCMD,		//  1 Player inputs
	DEM_EMPTYUSERCMD,	//  2 Equivalent to [DEM_USERCMD, 0]
	xx(MUSICCHANGE, ChangeMusicPacket),	//  4 String: music name
	DEM_STOP,			//  7 End of demo
	DEM_UINFCHANGED,	//  8 User info changed
	DEM_SINFCHANGED,	//  9 Server/Host info changed
	DEM_GENERICCHEAT,	// 10 Int8: cheat
	DEM_GIVECHEAT,		// 11 String: item to give, Int32: quantity
	xx(SAY, SayPacket),			// 12 Int8: who to talk to, String: message
	DEM_CHANGEMAP,		// 14 String: name of map
	DEM_SUICIDE,		// 15 
	DEM_ADDBOT,			// 16 Int8: botshift, String: userinfo for bot, Type: botskill_t
	DEM_KILLBOTS,		// 17 
	DEM_INVUSEALL,		// 18 
	DEM_INVUSE,			// 19 Int32: ID of item
	DEM_PAUSE,			// 20 
	DEM_SAVEGAME,		// 21 String: filename, String: description
	DEM_SUMMON,			// 26 String: thing to fabricate
	DEM_FOV,			// 27 Float: new FOV for all players
	DEM_MYFOV,			// 28 Float: new FOV for this player
	DEM_CHANGEMAP2,		// 29 Int8: position in new map, String: map name
	DEM_RUNSCRIPT,		// 32 Int16: Script#, Int8: # of args; each arg is an Int32
	DEM_SINFCHANGEDXOR,	// 33 Like DEM_SINFCHANGED, but data is a byte indicating how to set a bit
	DEM_INVDROP,		// 34 Int32: ID of item Int32: amount
	DEM_WARPCHEAT,		// 35 Int16: x position, Int16: y position, Int16: z position
	DEM_CENTERVIEW,		// 36 
	DEM_SUMMONFRIEND,	// 37 String: thing to fabricate
	DEM_SPRAY,			// 38 String: decal name
	DEM_CROUCH,			// 39 
	DEM_RUNSCRIPT2,		// 40 Same as DEM_RUNSCRIPT, but always executes
	DEM_CHECKAUTOSAVE,	// 41 
	DEM_DOAUTOSAVE,		// 42 
	DEM_MORPHEX,		// 43 String: the class to morph to
	DEM_SUMMONFOE,		// 44 String: thing to fabricate
	DEM_TAKECHEAT,		// 47 String: item class, Int32: quantity
	DEM_ADDCONTROLLER,	// 48 Int8: player
	DEM_DELCONTROLLER,	// 49 Int8: player
	DEM_KILLCLASSCHEAT,	// 50 String: class to kill
	DEM_SUMMON2,		// 52 String: thing to fabricate, Int16: angle offset, Int16: tid, Int8: special, 5xInt32: args
	DEM_SUMMONFRIEND2,	// 53 String: thing to fabricate, Int16: angle offset, Int16: tid, Int8: special, 5xInt32: args
	DEM_SUMMONFOE2,		// 54 String: thing to fabricate, Int16: angle offset, Int16: tid, Int8: special, 5xInt32: args
	DEM_ADDSLOTDEFAULT,	// 55 Int8: slot
	DEM_ADDSLOT,		// 56 Int8: slot
	DEM_SETSLOT,		// 57 Int8: slot, Int8: weapons
	DEM_SUMMONMBF,		// 58 String: thing to fabricate
	DEM_CONVREPLY,		// 59 Int16: dialogue node, Int8: reply number
	DEM_CONVCLOSE,		// 60
	DEM_CONVNULL,		// 61
	DEM_RUNSPECIAL,		// 62 Int16: special, Int8: Arg count, Int32s: Args
	DEM_SETPITCHLIMIT,	// 63 Int8: up limit, Int8: down limit (in degrees)
	DEM_RUNNAMEDSCRIPT,	// 65 String: script name, Int8: Arg count + Always flag; each arg is an Int32
	xx(REVERTCAMERA, RevertCameraPacket),	// 66 
	DEM_SETSLOTPNUM,	// 67 Int8: player number, the rest is the same as DEM_SETSLOT
	DEM_REMOVE,			// 68 String: class to remove
	DEM_FINISHGAME,		// 69 
	xx(NETEVENT, NetEventPacket),		// 70 String: event name, Int8: Arg count; each arg is an Int32, Int8: manual
	DEM_MDK,			// 71 String: damage type
	DEM_SETINV,			// 72 String: item name, Int32: amount, Int8: allow beyond max
	DEM_ENDSCREENJOB,	// 73 
	DEM_ZSC_CMD,		// 74 String: command, Int16: Byte size of command
	DEM_CHANGESKILL,	// 75 Int32: Skill
	DEM_KICK,			// 76 Int8: Player number
	xx(READIED, PlayerReadyPacket),		// 77 

	DEM_NUM_COMMANDS
};

#undef xx

DEFINE_NETPACKET_EMPTY(EndScreenRunnerPacket, DEM_ENDSCREENJOB);
DEFINE_NETPACKET_EMPTY(PlayerReadyPacket, DEM_READIED);
DEFINE_NETPACKET_EMPTY(UseAllPacket, DEM_INVUSEALL);
DEFINE_NETPACKET_EMPTY(RevertCameraPacket, DEM_REVERTCAMERA);

class SayPacket : public NetPacket
{
	DEFINE_NETPACKET(SayPacket, NetPacket, DEM_SAY, 2)
	uint8_t _flags = 0u;
	FString _message = {};
	static constexpr uint8_t MSG_TEAM = 1u;
	static constexpr uint8_t MSG_BOLD = 2u;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_UINT8(_flags);
		SERIALIZE_STRING(_message);
		return true;
	}
public:
	SayPacket(uint8_t flags, const FString& message) : SayPacket()
	{
		_flags = flags;
		_message = message;
	}
	bool ShouldWrite() const override
	{
		return _message.IsNotEmpty();
	}
};

class RunSpecialPacket : public NetPacket
{
	DEFINE_NETPACKET(RunSpecialPacket, NetPacket, DEM_RUNSPECIAL, 2)
	int16_t _special = 0;
	int32_t _args[5] = {};
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_INT16(_special);
		size_t len = std::size(_args);
		IF_WRITING()
		{
			int i = len - 1;
			for (; i >= 0; --i)
			{
				if (_args[i])
					break;
			}
			len = (size_t)(i + 1);
		}
		TArrayView<const int32_t> data = { _args, len };
		SERIALIZE_SMALL_ARRAY_EXPECTING(data, std::size(_args), false);
		IF_READING()
			memcpy(_args, data.Data(), data.Size());
		return true;
	}
public:
	RunSpecialPacket(int16_t special, const TArrayView<const int32_t>& args) : RunSpecialPacket()
	{
		_special = special;
		memcpy(_args, args.Data(), min<size_t>(args.Size(), std::size(_args)));
	}
	bool ShouldWrite() const override
	{
		return _special > 0;
	}
};

class NetEventPacket : public NetPacket
{
	DEFINE_NETPACKET(NetEventPacket, NetPacket, DEM_NETEVENT, 3)
	FString _event = {};
	int32_t _args[3] = {};
	bool _bManual = true;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_STRING(_event);
		size_t len = std::size(args);
		IF_WRITING()
		{
			int i = len - 1;
			for (; i >= 0; --i)
			{
				if (_args[i])
					break;
			}
			len = (size_t)(i + 1);
		}
		TArrayView<const int32_t> data = { _args, len };
		SERIALIZE_ARRAY_EXPECTING(int32_t, data, std::size(_args), false);
		IF_READING()
			memcpy(_args, data.Data(), data.Size());
		SERIALIZE_BOOL(_bManual);
	}
public:
	NetEventPacket(const FString& event, int32_t arg0, int32_t arg1, int32_t arg2, bool bManual) : NetEventPacket()
	{
		_event = event;
		_args[0] = arg0;
		_args[1] = arg1;
		_args[2] = arg2;
		_bManual = bManual;
	}
};

DEFINE_NETPACKET_STRING(ChangeMusicPacket, DEM_MUSICCHANGE);

EDemoCommand GetPacketType(const TArrayView<const uint8_t>& stream);
void InitializeDoomPackets();

#endif // __D_NETPACKET__
