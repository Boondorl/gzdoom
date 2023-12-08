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

#ifndef __B_BOT_H__
#define __B_BOT_H__

#include "g_levellocals.h"
#include "d_event.h"
#include "vm.h"
#include "m_random.h"
#include "p_checkposition.h"

EXTERN_CVAR(Int, bot_next_color) // Unused.

enum EBotMoveDirection
{
	MDIR_BACKWARDS = -1,
	MDIR_NONE,
	MDIR_FORWARDS,
	MDIR_NO_CHANGE,

	MDIR_RIGHT = MDIR_BACKWARDS,
	MDIR_LEFT = MDIR_FORWARDS
};

// Allow for modders to set up any custom properties they want in BOTDEFS. Includes wrapper functionality
// for getting simple data types (others like vectors can be implemented ZScript side). Name is more generic since
// this can technically be used for anything for any reason.
struct FEntityProperties
{
private:
	TMap<FName, FString> _properties = {};
	const FEntityProperties* _default = nullptr; // Similar to a class' default field.

public:
	FEntityProperties() {}
	FEntityProperties(const TMap<FName, FString>& _properties) : _properties(_properties) {}
	FEntityProperties(const FEntityProperties* _default) : _default(_default) { ResetAllProperties(); }
	FEntityProperties(const TMap<FName, FString>& _properties, const FEntityProperties* _default) : _properties(_properties), _default(_default) {}

	// Needed for when the bot is destroyed.
	void Clear()
	{
		_properties.Clear();
		_default = nullptr;
	}

	void ResetProperty(const FName& key)
	{
		if (_default != nullptr && _default->HasProperty(key))
			SetString(key, _default->GetString(key));
		else
			RemoveProperty(key);
	}

	void ResetAllProperties()
	{
		if (_default != nullptr)
			_properties = _default->_properties;
		else
			_properties = {};
	}

	// This is important so that properties can be properly reset to use defaults
	// specified by the client. Even having an empty string is not the same as the value not
	// existing.
	void RemoveProperty(const FName& key)
	{
		_properties.Remove(key);
	}

	const TMap<FName, FString>& GetProperties() const
	{
		return _properties;
	}

	bool HasProperty(const FName& key) const
	{
		return _properties.CheckKey(key) != nullptr;
	}

	void SetString(const FName& key, const FString& value)
	{
		_properties.Insert(key, value);
	}

	void SetBool(const FName& key, const bool value)
	{
		FString val;
		val.Format("%d", value);

		_properties.Insert(key, val);
	}

	void SetInt(const FName& key, const int value)
	{
		FString val;
		val.Format("%d", value);

		_properties.Insert(key, val);
	}

	void SetDouble(const FName& key, const double value)
	{
		FString val;
		val.Format("%f", value);

		_properties.Insert(key, val);
	}

	const FString& GetString(const FName& key, const FString& def = {}) const
	{
		auto value = _properties.CheckKey(key);
		return value != nullptr ? *value : def;
	}

	bool GetBool(const FName& key, const bool def = false) const
	{
		auto value = _properties.CheckKey(key);
		return value != nullptr ? static_cast<bool>(value->ToLong()) : def;
	}

	int GetInt(const FName& key, const int def = 0) const
	{
		auto value = _properties.CheckKey(key);
		return value != nullptr ? static_cast<int>(value->ToLong()) : def;
	}

	double GetDouble(const FName& key, const double def = 0.0) const
	{
		auto value = _properties.CheckKey(key);
		return value != nullptr ? value->ToDouble() : def;
	}
};

// Info about bots in the BOTDEF files.
struct FBotDefinition
{
private:
	FEntityProperties _properties = {};
	FString _userInfo = {};

public:
	// Just in case.
	void Clear()
	{
		_properties.Clear();
		_userInfo = FString{};
	}

	// This is needed so that the player's userinfo values get set correctly.
	void GenerateUserInfo()
	{
		// Reset it if it's being regenerated.
		_userInfo = FString{};

		TMap<FName, FString>::ConstPair *pair = nullptr;
		TMap<FName, FString>::ConstIterator it { _properties.GetProperties() };
		while (it.NextPair(pair))
			_userInfo.AppendFormat("%s\\%s\\", pair->Key, pair->Value);
	}

	const char *GetUserInfo() const
	{
		return _userInfo.GetChars();
	}

	const FEntityProperties &GetProperties() const
	{
		return _properties;
	}

	void SetString(const FName& key, const FString& value)
	{
		_properties.SetString(key, value);
	}

	void SetBool(const FName& key, const bool value)
	{
		_properties.SetBool(key, value);
	}

	void SetInt(const FName& key, const int value)
	{
		_properties.SetInt(key, value);
	}

	void SetDouble(const FName& key, const double value)
	{
		_properties.SetDouble(key, value);
	}

	const FString& GetString(const FName& key, const FString& def = {}) const
	{
		return _properties.GetString(key, def);
	}

	bool GetBool(const FName& key, const bool def = false) const
	{
		return _properties.GetBool(key, def);
	}

	int GetInt(const FName& key, const int def = 0) const
	{
		return _properties.GetInt(key, def);
	}

	double GetDouble(const FName& key, const double def = 0.0) const
	{
		return _properties.GetDouble(key, def);
	}
};

// Used to keep all the globally needed variables and functions in order. A namespace isn't used
// here in order to prevent certain functionality from being accessed globally.
class DBotManager final
{
private:
	static TArray<FString> _botNameArgs; // Bot names given when the host launched the game with the "-bots" arg.

	static FBotDefinition& ParseBot(FScanner& sc, FBotDefinition& def);				// Function that parses a bot block in BOTDEFS.
	static FEntityProperties& ParseWeapon(FScanner& sc, FEntityProperties& props);	// Function that parses a weapon block in BOTDEFS.

	DBotManager(); // Don't instantiate it.

public:
	static cycle_t BotThinkCycles;							// For tracking think time of bots specifically.
	static TMap<FName, FBotDefinition> BotDefinitions;		// Default properties and userinfo to give when spawning a bot. Stored by bot ID.
	static TMap<FName, FEntityProperties> BotWeaponInfo;	// Key information about how bots should use each weapon. Stored by weapon class name.

	static void ParseBotDefinitions();									// Parses the BOTDEF lumps.
	static void SetNamedBots(const FString* args, const int argCount);	// Parses the "-bots" arg for the names of the bots.
	static void SpawnNamedBots(FLevelLocals* level);					// Spawns any named bots. Only the host can do this. Triggers on level load.
	static int CountBots(FLevelLocals* level = nullptr);				// Counts the number of bots in the game. Used after loading.

	static bool SpawnBot(FLevelLocals* level, const FName& name = NAME_None);						// Spawns a bot over the network. If no name is passed, spawns a random one.
	static bool TryAddBot(FLevelLocals *level, const unsigned int playerIndex, const FName& botID);	// Parses the network message to try and add a bot.
	static void RemoveBot(FLevelLocals* level, const unsigned int botNum);							// Removes the bot and makes it emulate a player leaving the game.
	static void RemoveAllBots(FLevelLocals* level);													// Removes all bots from the game.
};

class DBot : public DThinker
{
	DECLARE_CLASS(DBot, DThinker)

private:
	player_t* _player;	// Player info for the bot.
	FName _botID;		// Tracks which bot definition it's tied to.

public:
	static const int DEFAULT_STAT = STAT_BOT;

	FEntityProperties Properties; // Stores current information about the bot. Uses the properties from its bot ID as defaults.

	static bool IsSectorDangerous(const sector_t* sec); // Checks if the sector is dangerous to the bot.

	void Construct();										// Set the default values of the class fields.
	void OnDestroy() override;								// Clear the Properties map.
	void Initialize(player_t* player, const FName& index);	// Initialize the bot with a player and bot ID.
	void Serialize(FSerializer &arc);						// Stuff to write to and load from the save file.

	constexpr player_t *GetPlayer() const;
	constexpr FName GetBotID() const;

	void CallBotThink(); // Handles overall thinking logic. Called directly before PlayerThink.
	
	// Boon TODO: nullptr checks since these will be exported
	bool IsActorInView(AActor* mo, const DAngle& fov = DAngle90) const;	// Check if the bot has sight of the Actor within a view cone.
	bool CanReach(AActor* target) const;								// Checks to see if a valid movement can be made towards the target.
	bool CheckMissileTrajectory(const DVector3& dest, const double minDistance = 0.0, const double maxDistance = 320.0) const; // Checks if anything is blocking the ReadyWeapon missile's path.
	void FindEnemy(const DAngle& fov = nullAngle) const;				// Tries to find a target.
	void FindPartner() const;											// Looks for a player to stick near, bot or real.
	bool IsValidItem(AActor* item) const;								// Checks to see if the item is able to be picked up.
	void PitchTowardsActor(AActor* target) const;						// Aim the bot's pitch towards the target.
	bool FakeCheckPosition(const DVector2& pos, FCheckPosition& tm, const bool actorsOnly = false) const; // Same as CheckPosition but prevent picking up items.
	void Roam() const;													// Attempt to move towards the boat's goal similar to how monsters move.
	bool CheckMove(const DVector2& pos) const;							// Check if a valid movement can be made to the given position. Also jumps if needed if that move is valid.
	bool Move() const;													// Check to see if a movement is valid in the current moveDir.
	EBotMoveDirection PickStrafeDirection(const EBotMoveDirection startDir = MDIR_NONE) const; // Picks a valid strafe direction to move. Can also jump.
	bool TryWalk() const;												// Same as Move but also sets a turn cool down when moving.
	void NewChaseDir() const;											// Attempts to get a new direction to move towards the bot's goal.
	void SetMove(const EBotMoveDirection forward, const EBotMoveDirection side, const bool running) const;	// Sets the forwardmove/sidemove commands.
	void SetButtons(const int cmd, const bool set) const;													// Sets the button commands.
};

#endif
