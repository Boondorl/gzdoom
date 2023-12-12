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

extern int forwardmove[2], sidemove[2], flyspeed[2];

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

// The player pointer isn't serialized since bots will have their slots rearranged on load
// anyway to account for real players if needed. Instead their player pointer is reset then.
void DBot::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);

	if (arc.isWriting())
	{
		TMap<FName, FString> props = Properties.GetProperties();
		arc("botid", _botID)
			("properties", props);
	}
	else
	{
		TMap<FName, FString> props = {};
		arc("botid", _botID)
			("properties", props);

		const FBotDefinition* const def = DBotManager::BotDefinitions.CheckKey(_botID);
		if (def == nullptr)
			Properties = { props }; // Keep its properties and key just in case it needs to be removed.
		else
			Properties = { props, &def->GetProperties() };
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

void DBot::NormalizeSpeed(short& cmd, const int* const speeds, const bool running)
{
	const bool curRunning = _player->cmd.ucmd.buttons & (BT_SPEED | BT_RUN);
	if (curRunning && !running)
		cmd *= static_cast<double>(speeds[0]) / speeds[1];
	else if (!curRunning && running)
		cmd *= static_cast<double>(speeds[1]) / speeds[0];
}

void DBot::SetMove(const EBotMoveDirection forward, const EBotMoveDirection side, const EBotMoveDirection up, const bool running)
{
	if (forward != MDIR_NO_CHANGE)
		_player->cmd.ucmd.forwardmove = forwardmove[running] * forward;
	else
		NormalizeSpeed(_player->cmd.ucmd.forwardmove, forwardmove, running);

	if (side != MDIR_NO_CHANGE)
		_player->cmd.ucmd.sidemove = sidemove[running] * side;
	else
		NormalizeSpeed(_player->cmd.ucmd.sidemove, sidemove, running);

	if (up != MDIR_NO_CHANGE)
		_player->cmd.ucmd.upmove = flyspeed[running] * up;
	else
		NormalizeSpeed(_player->cmd.ucmd.upmove, flyspeed, running);

	SetButtons(BT_SPEED | BT_RUN, running);
}

void DBot::SetButtons(const int cmds, const bool set)
{
	if (set)
		_player->cmd.ucmd.buttons |= cmds;
	else
		_player->cmd.ucmd.buttons &= ~cmds;
};

void DBot::SetAngleCommand(short& cmd, const DAngle& curAng, const DAngle& destAng)
{
	constexpr double AngToCmd = 65536.0 / 360.0;

	const DAngle delta = deltaangle(curAng, destAng);
	cmd = static_cast<int>(delta.Degrees() * AngToCmd);
}

void DBot::SetAngle(const DAngle& dest, const EBotAngleCmd type)
{
	switch (type)
	{
		case ACMD_YAW:
			SetAngleCommand(_player->cmd.ucmd.yaw, _player->mo->Angles.Yaw, dest);
			break;

		case ACMD_PITCH:
			SetAngleCommand(_player->cmd.ucmd.pitch, _player->mo->Angles.Pitch, dest);
			break;

		case ACMD_ROLL:
			SetAngleCommand(_player->cmd.ucmd.roll, _player->mo->Angles.Roll, dest);
			break;
	}
}
