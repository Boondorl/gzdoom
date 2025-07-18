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
#include "m_cheat.h"
#include "g_levellocals.h"
#include "gstrings.h"
#include "p_lnspec.h"

void InitializeDoomPackets()
{
	REGISTER_NETPACKET(SayPacket);
	REGISTER_NETPACKET(ChangeMusicPacket);
	REGISTER_NETPACKET(NetCommandPacket);
	REGISTER_NETPACKET(RevertCameraPacket);
}

EDemoCommand GetPacketType(const ReadStream& stream)
{
	return stream.Size() ? stream.PeekValue<EDemoCommand>() : DEM_INVALID;
}

//==========================================================================
//
// Packet functionality
//
//==========================================================================

bool CheckCheatmode(bool printmsg, bool sponly);

static bool ValidClass(const FString& cls, FName type = NAME_Actor)
{
	auto clss = PClass::FindActor(cls);
	return clss != nullptr && clss->IsDescendantOf(type);
}

NETPACKET_CONDITION(ScriptCallPacket)
{
	return _scriptNum;
}

NETPACKET_EXECUTE(ScriptCallPacket)
{
	P_StartScript(players[player].mo->Level, players[player].mo, nullptr, _scriptNum, players[player].mo->Level->MapName.GetChars(), Args, std::size(Args), ACS_NET | _bAlways);
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
	return true;
}

NETPACKET_EXECUTE(ChangeMusicPacket)
{
	S_ChangeMusic(Value.GetChars());
	return true;
}

NETPACKET_CONDITION(CheatPacket)
{
	if (!ValidClass(ItemCls, NAME_Inventory))
	{
		Printf("No Inventory of type %s exists", ItemCls.GetChars());
		return false;
	}
	return true;
}

NETPACKET_EXECUTE(GiveCheatPacket)
{
	if (!ValidClass(ItemCls, NAME_Inventory))
	{
		Printf("%s [%d] spawned invalid item %s", players[player].userinfo.GetName(), player, ItemCls.GetChars());
		return true; // Non-fatal, just means someone is about to desync.
	}

	cht_Give(&players[player], ItemCls, Amount);
	if (player != consoleplayer)
	{
		FString message = GStrings.GetString("TXT_X_CHEATS");
		message.Substitute("%s", players[player].userinfo.GetName());
		Printf("%s: give %s\n", message.GetChars(), ItemCls.GetChars());
	}
	return true;
}

NETPACKET_EXECUTE(TakeCheatPacket)
{
	if (!ValidClass(ItemCls, NAME_Inventory))
	{
		Printf("%s [%d] took invalid item %s", players[player].userinfo.GetName(), player, ItemCls.GetChars());
		return true; // Non-fatal, just means someone is about to desync.
	}

	cht_Take(&players[player], ItemCls, Amount);
	return true;
}

NETPACKET_EXECUTE(SetCheatPacket)
{
	if (!ValidClass(ItemCls, NAME_Inventory))
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
	if (!CheckCheatmode(player == consoleplayer, false))
		P_ExecuteSpecial(players[player].mo->Level, _special, nullptr, players[player].mo, false, Args[0], Args[1], Args[2], Args[3], Args[4]);
	return true;
}
