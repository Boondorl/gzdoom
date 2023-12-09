/*
**
**
**---------------------------------------------------------------------------
** Copyright 1999 Martin Colberg
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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

#include "g_levellocals.h" // b_bot.h is defined in here via d_player.h.
#include "c_dispatch.h"
#include "teaminfo.h"
#include "d_net.h"
#include "events.h"

static FRandom pr_botspawn("BotSpawn");

extern void G_DoPlayerPop(int playernum);

// Sends out a network message telling clients to spawn a bot. Name
// is optional and, if not specified, a random one is chosen. This
// can include duplicates. This is really only meant to be called
// from the console command, but is network safe.
bool DBotManager::SpawnBot(FLevelLocals* const level, const FName& name)
{
	if (consoleplayer != Net_Arbitrator)
	{
		Printf("Only the net arbitrator is allowed to spawn bots\n");
		return false;
	}

	if (gamestate != GS_LEVEL)
	{
		Printf("Bots cannot be added when not in a game\n");
		return false;
	}

	if (!BotDefinitions.CountUsed())
	{
		Printf("There are no bots available to spawn\n");
		return false;
	}

	unsigned int playerIndex = 0u;
	for (; playerIndex < MAXPLAYERS; ++playerIndex)
	{
		if (!level->PlayerInGame(playerIndex))
			break;
	}

	if (playerIndex >= MAXPLAYERS)
	{
		Printf("The maximum number of players has already been reached, bot could not be added\n");
		return false;
	}

	FName key = name;
	if (key != NAME_None)
	{
		if (BotDefinitions.CheckKey(name) == nullptr)
		{
   		 	Printf("Couldn't find bot %s\n", name.GetChars());
			return false;
		}
	}
	else
	{
		// Spawn a random bot if no name given.
		int r = pr_botspawn() % BotDefinitions.CountUsed();
		TMap<FName, FBotDefinition>::ConstPair* pair = nullptr;
		TMap<FName, FBotDefinition>::ConstIterator it = { BotDefinitions };
		while (it.NextPair(pair) && r-- > 0) {}
		key = pair->Key;
	}

	// Send off the player slot + bot id.
	Net_WriteByte(DEM_ADDBOT);
	Net_WriteByte(playerIndex);
	Net_WriteString(key.GetChars());

	return true;
}

// Attempts to add a bot to the given level. This is really only meant to be called when
// receiving the network message to spawn a bot. SpawnBot should be used instead if network
// safety is needed.
bool DBotManager::TryAddBot(FLevelLocals* const level, const unsigned int playerIndex, const FName& botID)
{
	if (gamestate != GS_LEVEL)
	{
		Printf("Bots cannot be added when not in a game\n");
		return false;
	}

	if (level->PlayerInGame(playerIndex))
	{
		Printf("Couldn't add bot to slot %d, already occupied\n", playerIndex);
		return false;
	}

	const auto bot = BotDefinitions.CheckKey(botID);
	if (bot == nullptr)
	{
		Printf("Bot %s does not exist\n", botID.GetChars());
		return false;
	}

	bot->GenerateUserInfo(); // This can change to account for team.
	uint8_t* stream = bot->GetUserInfo();
	D_ReadUserInfoStrings(playerIndex, &stream, false);

	multiplayer = true; // Count this as multiplayer, even if it's not a netgame.
	playeringame[playerIndex] = true;
	level->Players[playerIndex]->Bot = level->CreateThinker<DBot>(level->Players[playerIndex], botID);
	level->Players[playerIndex]->playerstate = PST_ENTER;

	if (teamplay)
		Printf("%s joined the %s team\n", level->Players[playerIndex]->userinfo.GetName(), Teams[level->Players[playerIndex]->userinfo.GetTeam()].GetName());
	else
		Printf("%s joined the game\n", level->Players[playerIndex]->userinfo.GetName());

	// Since bots can enter the game at any time unlike clients, this has to be handled here manually.
	level->DoReborn(playerIndex);
	level->localEventManager->PlayerEntered(playerIndex, false);
	return true;
}

void DBotManager::RemoveBot(FLevelLocals* const level, const unsigned int botNum)
{
	if (level->Players[botNum]->Bot == nullptr)
		return;

	// Make sure they stay on the same team next time they're added again.
	const auto bot = BotDefinitions.CheckKey(level->Players[botNum]->Bot->GetBotID());
	int team = level->Players[botNum]->userinfo.GetTeam();
	if (!TeamLibrary.IsValidTeam(team))
		team = bot->GetInt("Team", TEAM_NONE);

	bot->SetInt("Team", team);
	level->Players[botNum]->playerstate = PST_GONE;
	G_DoPlayerPop(botNum);
}

void DBotManager::RemoveAllBots(FLevelLocals* const level)
{
	for (unsigned int i = 0u; i < MAXPLAYERS; ++i)
	{
		if (level->Players[i]->Bot != nullptr)
			RemoveBot(level, i);
	}
}

int DBotManager::CountBots(FLevelLocals* const level)
{
	int bots = 0;
	if (level == nullptr)
	{
		for (const auto lev : AllLevels())
		{
			for (unsigned int i = 0u; i < MAXPLAYERS; ++i)
			{
				if (lev->PlayerInGame(i) && lev->Players[i]->Bot != nullptr)
					++bots;
			}
		}
	}
	else
	{
		for (unsigned int i = 0u; i < MAXPLAYERS; ++i)
		{
			if (level->PlayerInGame(i) && level->Players[i]->Bot != nullptr)
				++bots;
		}
	}

	return bots;
}

void DBotManager::ParseBotDefinitions()
{
	BotDefinitions.Clear();
	BotWeaponInfo.Clear();

	constexpr int NoLump = -1;
	constexpr char LumpName[] = "BOTDEF";

	int lump = NoLump;
	int lastLump = 0;
	while ((lump = fileSystem.FindLump(LumpName, &lastLump)) != NoLump)
	{
		FScanner sc = { lump };
		sc.SetCMode(true);
		while (sc.GetString())
		{
			if (sc.Compare("Bot"))
			{
				sc.MustGetString();
				const FName key = sc.String;
				sc.MustGetStringName("{");
				FBotDefinition def = {};
				BotDefinitions.Insert(key, ParseBot(sc, def));
			}
			else if (sc.Compare("Weapon"))
			{
				sc.MustGetString();
				const FName key = sc.String;
				sc.MustGetStringName("{");
				FEntityProperties props = {};
				BotWeaponInfo.Insert(key, ParseWeapon(sc, props));
			}
			else
			{
				sc.ScriptError("Expected 'Weapon' or 'Bot, got '%s'.", sc.String);
			}
		}
	}
}

FBotDefinition& DBotManager::ParseBot(FScanner& sc, FBotDefinition& def)
{
	while (sc.GetString())
	{
		if (sc.Compare("}"))
			break;

		const FName key = sc.String;
		sc.MustGetString();
		def.SetString(key, sc.String);
	}

	if ((&def.GetString("PlayerClass"))->IsEmpty())
		def.SetString("PlayerClass", "Random");

	const unsigned int team = def.GetInt("Team", UINT_MAX);
	if (!TeamLibrary.IsValidTeam(team))
		def.SetInt("Team", TEAM_NONE);

	return def;
}

FEntityProperties& DBotManager::ParseWeapon(FScanner& sc, FEntityProperties& props)
{
	while (sc.GetString())
	{
		if (sc.Compare("}"))
			break;

		const FName key = sc.String;
		sc.MustGetString();
		props.SetString(key, sc.String);
	}

	return props;
}

void DBotManager::SetNamedBots(const FString* const args, const int argCount)
{
	_botNameArgs.Clear();

	for (int i = 0; i < argCount; ++i)
		_botNameArgs.Push(args[i]);
}

// Boon TODO: This really needs a GS_LEVEL check...
void DBotManager::SpawnNamedBots(FLevelLocals* const level)
{
	// Only the host gets to specify bots.
	if (consoleplayer == Net_Arbitrator)
	{
		for (const auto& name : _botNameArgs)
			SpawnBot(level, name);
	}

	_botNameArgs.Clear();
}

CVAR(Int, bot_next_color, 0, 0)

ADD_STAT(bots)
{
	FString out = {};
	out.Format("think = %04.1f ms", DBotManager::BotThinkCycles.TimeMS());
	return out;
}

CCMD(addbot)
{
	if (argv.argc() > 2)
	{
		Printf("addbot [botname] : add a bot to the game\n");
		return;
	}

	DBotManager::SpawnBot(primaryLevel, argv.argc() > 1 ? argv[1] : FString{});
}

CCMD(removebots)
{
	if (consoleplayer != Net_Arbitrator)
	{
		Printf("Only the net arbitrator can remove bots\n");
		return;
	}

	Net_WriteByte(DEM_KILLBOTS);
}

CCMD(removebot)
{
	if (argv.argc() != 2)
	{
		Printf("removebot botname : remove a bot from the game\n");
		return;
	}

	if (consoleplayer != Net_Arbitrator)
	{
		Printf("Only the net arbitrator can remove bots\n");
		return;
	}

	const FName id = argv[1];
	if (DBotManager::BotDefinitions.CheckKey(id) == nullptr)
	{
		Printf("Bot with name %s does not exist\n", argv[1]);
		return;
	}

	unsigned int i = 0u;
	for (; i < MAXPLAYERS; ++i)
	{
		if (primaryLevel->PlayerInGame(i) && primaryLevel->Players[i]->Bot != nullptr
			&& primaryLevel->Players[i]->Bot->GetBotID() == id)
		{
			break;
		}
	}

	if (i >= MAXPLAYERS)
	{
		Printf("Bot %s is not currently in the game\n", argv[1]);
		return;
	}

	Net_WriteByte(DEM_KILLBOT);
	Net_WriteByte(i);
}

CCMD(listbots)
{
	TMap<FName, FBotDefinition>::ConstPair* pair = nullptr;
	TMap<FName, FBotDefinition>::ConstIterator it = { DBotManager::BotDefinitions };
	while (it.NextPair(pair))
	{
		unsigned int i = 0u;
		for (; i < MAXPLAYERS; ++i)
		{
			if (primaryLevel->PlayerInGame(i) && primaryLevel->Players[i]->Bot != nullptr
				&& primaryLevel->Players[i]->Bot->GetBotID() == pair->Key)
			{
				break;
			}
		}

		Printf("%s%s\n", pair->Key.GetChars(), i < MAXPLAYERS ? " (active)" : "");
	}

	Printf("> %d bots\n> %d bots active\n", DBotManager::BotDefinitions.CountUsed(), DBotManager::CountBots(primaryLevel));
}
