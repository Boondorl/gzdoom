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

#include "d_net.h"
#include "s_music.h"
#include "screenjob.h"
#include "vm.h"
#include "actor.h"
#include "d_player.h"
#include "i_interface.h"
#include "gi.h"

void InitializeDoomPackets()
{
	REGISTER_NETPACKET(SayPacket);
	REGISTER_NETPACKET(ChangeMusicPacket);
	REGISTER_NETPACKET(NetCommandPacket);
	REGISTER_NETPACKET(RevertCameraPacket);
}

EDemoCommand GetPacketType(const TArrayView<const uint8_t>& stream)
{
	return stream.Size() ? static_cast<EDemoCommand>(stream[0]) : DEM_INVALID;
}

// Data-typed packets for easier one-liners.

NETPACKET_EXECUTE(EndScreenRunnerPacket)
{
	EndScreenJob();
	return true;
}

NETPACKET_EXECUTE(PlayerReadyPacket)
{
	Net_PlayerReadiedUp(player);
	return true;
}

NETPACKET_EXECUTE(RevertCameraPacket)
{
	players[player].camera = players[player].mo;
	return true;
}

NETPACKET_EXECUTE(UseAllPacket)
{
	if (gamestate == GS_LEVEL && !paused
		&& players[player].playerstate != PST_DEAD)
	{
		AActor* item = players[player].mo->Inventory;
		while (item != nullptr)
		{
			AActor* next = item->Inventory;
			IFVIRTUALPTRNAME(item, NAME_Inventory, UseAll)
				VMCallVoid<AActor*, AActor*>(func, item, players[player].mo);
			item = next;
		}
	}
}

NETPACKET_EXECUTE(ChangeMusicPacket)
{
	S_ChangeMusic(Value.GetChars());
	return true;
}

NETPACKET_EXECUTE(SayPacket)
{
	if (cl_showchat == CHAT_DISABLED || (MutedClients & ((uint64_t)1u << player)))
		return true;

	const char* name = players[player].userinfo.GetName();
	if (!(_flags & MSG_TEAM))
	{
		if (cl_showchat < CHAT_GLOBAL)
			return true;

		// Said to everyone
		if (deathmatch && teamplay)
			Printf(PRINT_CHAT, "(All) ");
		if ((_flags & MSG_BOLD) && !cl_noboldchat)
			Printf(PRINT_CHAT, TEXTCOLOR_BOLD "* %s [%d]" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, player, _message.GetChars());
		else
			Printf(PRINT_CHAT, "%s [%d]" TEXTCOLOR_CHAT ": %s" TEXTCOLOR_CHAT "\n", name, player, _message.GetChars());

		if (!cl_nochatsound)
			S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
	}
	else if (!deathmatch || players[player].userinfo.GetTeam() == players[consoleplayer].userinfo.GetTeam())
	{
		if (cl_showchat < CHAT_TEAM_ONLY)
			return;

		// Said only to members of the player's team
		if (deathmatch && teamplay)
			Printf(PRINT_TEAMCHAT, "(Team) ");
		if ((_flags & MSG_BOLD) && !cl_noboldchat)
			Printf(PRINT_TEAMCHAT, TEXTCOLOR_BOLD "* %s [%d]" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, player, _message.GetChars());
		else
			Printf(PRINT_TEAMCHAT, "%s [%d]" TEXTCOLOR_TEAMCHAT ": %s" TEXTCOLOR_TEAMCHAT "\n", name, player, _message.GetChars());

		if (!cl_nochatsound)
			S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
	}
	return true;
}

NETPACKET_EXECUTE(RunSpecialPacket)
{
	if (!CheckCheatmode(player == consoleplayer))
		P_ExecuteSpecial(players[player].mo->Level, _special, nullptr, players[player].mo, false, _args[0], _args[1], _args[2], _args[3], _args[4]);
	return true;
}

class CheatPacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	FString ItemCls = {};
	int32_t Amount = 0;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_STRING(ItemCls);
		SERIALIZE_INT32(Amount);
		return true;
	}
public:
	bool ShouldWrite() const override
	{
		if (Amount <= 0)
			return false;

		auto cls = PClass::FindActor(ItemCls);
		if (cls == nullptr || !cls->IsDescendantOf(NAME_Inventory))
		{
			Printf("No Inventory of type %s exists", ItemCls.GetChars());
			return false;
		}
		return true;
	}
};

class GiveCheatPacket : public CheatPacket
{
	DEFINE_NETPACKET(GiveCheatPacket, CheatPacket, DEM_GIVECHEAT, 2)
public:
	bool Execute(int pNum) override
	{
		cht_Give(&players[pNum], ItemCls, Amount);
		if (pNum != consoleplayer)
		{
			FString message = GStrings.GetString("TXT_X_CHEATS");
			message.Substitute("%s", players[pNum].userinfo.GetName());
			Printf("%s: give %s\n", message.GetChars(), ItemCls.GetChars());
		}
		return true;
	}
};

class TakeCheatPacket : public CheatPacket
{
	DEFINE_NETPACKET(TakeCheatPacket, CheatPacket, DEM_TAKECHEAT, 2)
public:
	bool Execute(int pNum) override
	{
		cht_Take(&players[pNum], ItemCls, Amount);
		return true;
	}
};

class SetCheatPacket : public CheatPacket
{
	DEFINE_NETPACKET(SetCheatPacket, CheatPacket, DEM_TAKECHEAT, 3)
		bool _bPastMax = false;
public:
	bool Read(TArrayView<uint8_t>& stream) override
	{
		bool passed = Super::Read(stream);
		if (passed)
			_bPastMax = ReadInt8(stream);
		return passed;
	}
	bool Write(TArrayView<uint8_t>& stream) override
	{
		bool passed = Super::Read(stream);
		if (passed)
			WriteInt8(_bPastMax, stream);
		return passed;
	}
	bool Execute(int pNum) override
	{
		cht_SetInv(&players[pNum], ItemCls, Amount, _bPastMax);
		return true;
	}
};

class NetCommandPacket : public NetPacket
{
	DEFINE_NETPACKET(NetCommandPacket, NetPacket, DEM_ZSC_CMD, 2)
	FName _command = NAME_None;
	TArray<uint8_t> _buffer = {}; // Make sure the command is completely sandboxed.
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_NAME(_command);
		TArray<const uint8_t> data = { _buffer.Data(), _buffer.Size() };
		SERIALIZE_ARRAY(uint8_t, data);
		if (Stream::Reading)
		{
			_buffer.Clear();
			for (size_t i = 0u; i < data.Size(); ++i)
				_buffer.Push(data[i]);
		}
	}
public:
	NetCommandPacket(FName command, const TArrayView<const uint8_t>& data) : NetCommandPacket()
	{
		_command = command;
	}
	bool Execute(int pNum) override
	{
		FNetworkCommand netCmd = { pNum, _command, _buffer };
		players[pNum].mo->Level->localEventManager->NetCommand(netCmd);
		return true;
	}
};
