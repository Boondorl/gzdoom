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

EDemoCommand GetPacketType(const TArrayView<const uint8_t> stream)
{
	return stream.Size() ? static_cast<EDemoCommand>(stream[0]) : DEM_INVALID;
}

static bool ValidClass(const FString& cls, FName type = NAME_Actor)
{
	auto cls = PClass::FindActor(cls);
	return cls != nullptr && cls->IsDescendantOf(type);
}

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

NETPACKET_CONDITION(CheatPacket)
{
	if (!ValidClass())
	{
		Printf("No Inventory of type %s exists", ItemCls.GetChars());
		return false;
	}
	return true;
}

NETPACKET_EXECUTE(GiveCheatPacket)
{
	if (!ValidClass())
	{
		Printf("%s [%d] spawned invalid item %s", players[player].userinfo.GetName(), player, ItemCls.GetChars());
		return true; // Non-fatal, just means someone is about to desync.
	}

	cht_Give(&players[pNum], ItemCls, Amount);
	if (pNum != consoleplayer)
	{
		FString message = GStrings.GetString("TXT_X_CHEATS");
		message.Substitute("%s", players[pNum].userinfo.GetName());
		Printf("%s: give %s\n", message.GetChars(), ItemCls.GetChars());
	}
	return true;
}

NETPACKET_EXECUTE(TakeCheatPacket)
{
	if (!ValidClass())
	{
		Printf("%s [%d] took invalid item %s", players[player].userinfo.GetName(), player, ItemCls.GetChars());
		return true; // Non-fatal, just means someone is about to desync.
	}

	cht_Take(&players[pNum], ItemCls, Amount);
	return true;
}

NETPACKET_EXECUTE(SetCheatPacket)
{
	if (!ValidClass())
	{
		Printf("%s [%d] set invalid item %s", players[player].userinfo.GetName(), player, ItemCls.GetChars());
		return true; // Non-fatal, just means someone is about to desync.
	}

	cht_SetInv(&players[player], ItemCls, Amount, _bPastMax);
	return true;
}

NETPACKET_CONDITION(SayPacket)
{
	return _message.IsNotEmpty();
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

NETPACKET_CONDITION(RunSpecialPacket)
{
	return _special > 0;
}

NETPACKET_EXECUTE(RunSpecialPacket)
{
	if (!CheckCheatmode(player == consoleplayer))
		P_ExecuteSpecial(players[player].mo->Level, _special, nullptr, players[player].mo, false, _args[0], _args[1], _args[2], _args[3], _args[4]);
	return true;
}
