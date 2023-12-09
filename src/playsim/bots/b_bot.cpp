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
#include "serializer_doom.h"
#include "gi.h"

IMPLEMENT_CLASS(DBot, false, false)

// For Thinkers the default constructor isn't called at all, so these need to be initialized here.
void DBot::Construct(player_t* const player, const FName& botID)
{
	_player = player;
	_botID = botID;
	Properties = { &DBotManager::BotDefinitions.CheckKey(botID)->GetProperties() };
}

// This has to be cleared manually since the destructor never gets called on Thinkers.
void DBot::OnDestroy()
{
	Super::OnDestroy();

	Properties.Clear();
}

// Player is serialized via player slot. Bots always refresh their player struct on load so saving it is not needed.
// If a player currently occupies its desired slot, its player pointer will be null and an attempt
// will be made to see if a new slot exists for it to be moved to.
void DBot::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);

	if (arc.isWriting())
	{
		int32_t pNum = Level->PlayerNum(_player);
		TMap<FName, FString> props = Properties.GetProperties();
		arc("player", pNum)
			("botid", _botID)
			("properties", props);
	}
	else
	{
		int32_t pNum = 0;
		TMap<FName, FString> props = {};
		arc("player", pNum)
			("botid", _botID)
			("properties", props);

		const FBotDefinition* const def = DBotManager::BotDefinitions.CheckKey(_botID);
		if (def == nullptr)
			Properties = { props }; // Keep its properties and key just in case it needs to be removed.
		else
			Properties = { props, &def->GetProperties() };

		// Make sure the player slot is actually the bot. If not, the bot will
		// attempt to find a new slot if its player pointer is still null
		// after loading. If no slot open, it'll just boot itself.
		if (Level->Players[pNum]->Bot == this)
			_player = Level->Players[pNum];
	}
}

// Called directly before its player's Think so commands can be properly set up
void DBot::CallBotThink()
{
	DBotManager::BotThinkCycles.Clock();

	IFVIRTUAL(DBot, BotThink)
	{
		VMValue params[] = { this };
		VMCall(func, params, 1, nullptr, 0);
	}

	DBotManager::BotThinkCycles.Unclock();
}

void DBot::SetMove(const EBotMoveDirection forward, const EBotMoveDirection side, const bool running)
{
	if (forward != MDIR_NO_CHANGE)
		_player->cmd.ucmd.forwardmove = static_cast<int>(gameinfo.normforwardmove[running] * 256.0 * forward);
	if (side != MDIR_NO_CHANGE)
		_player->cmd.ucmd.sidemove = static_cast<int>(gameinfo.normsidemove[running] * 256.0 * side);

	SetButtons(BT_SPEED | BT_RUN, running);
}

void DBot::SetButtons(const int cmds, const bool set)
{
	if (set)
		_player->cmd.ucmd.buttons |= cmds;
	else
		_player->cmd.ucmd.buttons &= ~cmds;
};
