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

#include "b_bot.h"
#include "info.h"

// FEntityProperties

static void GetString(const FEntityProperties* const self, const int key, const FString* const def, FString* const res)
{
	*res = self->GetString(ENamedName(key), *def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetString, GetString)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_STRING(def);

	FString res = {};
	GetString(self, key, &def, &res);
	ACTION_RETURN_STRING(res);
}

static int GetInt(const FEntityProperties* const self, const int key, const int def)
{
	return self->GetInt(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetInt, GetInt)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(def);

	ACTION_RETURN_INT(GetInt(self, key, def));
}

static int GetBool(const FEntityProperties* const self, const int key, const int def)
{
	return self->GetBool(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetBool, GetBool)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(def);

	ACTION_RETURN_INT(GetBool(self, key, def));
}

static double GetDouble(const FEntityProperties* const self, const int key, const double def)
{
	return self->GetDouble(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetDouble, GetDouble)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_FLOAT(def);

	ACTION_RETURN_FLOAT(GetDouble(self, key, def));
}

static void SetString(FEntityProperties* const self, const int key, const FString* const value)
{
	self->SetString(ENamedName(key), *value);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, SetString, SetString)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_STRING(value);

	SetString(self, key, &value);
	return 0;
}

static void SetInt(FEntityProperties* const self, const int key, const int value)
{
	self->SetInt(ENamedName(key), value);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, SetInt, SetInt)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(value);

	SetInt(self, key, value);
	return 0;
}

static void SetBool(FEntityProperties* const self, const int key, const int value)
{
	self->SetBool(ENamedName(key), value);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, SetBool, SetBool)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(value);

	SetBool(self, key, value);
	return 0;
}

static void SetDouble(FEntityProperties* const self, const int key, const double value)
{
	self->SetDouble(ENamedName(key), value);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, SetDouble, SetDouble)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_FLOAT(value);

	SetDouble(self, key, value);
	return 0;
}

static int HasProperty(const FEntityProperties* const self, const int key)
{
	return self->HasProperty(ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, HasProperty, HasProperty)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);

	ACTION_RETURN_INT(HasProperty(self, key));
}

static void RemoveProperty(FEntityProperties* const self, const int key)
{
	self->RemoveProperty(ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, RemoveProperty, RemoveProperty)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);

	RemoveProperty(self, key);
	return 0;
}

static void ResetProperty(FEntityProperties* const self, const int key)
{
	self->ResetProperty(ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, ResetProperty, ResetProperty)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);

	ResetProperty(self, key);
	return 0;
}

static void ResetAllProperties(FEntityProperties* const self)
{
	self->ResetAllProperties();
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, ResetAllProperties, ResetAllProperties)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);

	ResetAllProperties(self);
	return 0;
}

// DBot

DEFINE_FIELD(DBot, Properties)

static FEntityProperties* GetWeaponInfo(const PClassActor* const weap)
{
	return DBotManager::GetWeaponInfo(weap);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, GetWeaponInfo, GetWeaponInfo)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(weap, PClassActor);

	ACTION_RETURN_POINTER(GetWeaponInfo(weap));
}

static int IsSectorDangerous(const sector_t* const sec)
{
	return DBot::IsSectorDangerous(sec);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, IsSectorDangerous, IsSectorDangerous)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(sec, sector_t);

	ACTION_RETURN_INT(IsSectorDangerous(sec));
}

static int GetBotCount()
{
	return DBotManager::CountBots();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, GetBotCount, GetBotCount)
{
	PARAM_PROLOGUE;

	ACTION_RETURN_INT(GetBotCount());
}

static player_t* GetPlayer(DBot* const self)
{
	return self->GetPlayer();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, GetPlayer, GetPlayer)
{
	PARAM_SELF_PROLOGUE(DBot);

	ACTION_RETURN_POINTER(GetPlayer(self));
}

static int GetBotID(DBot* const self)
{
	return (&self->GetBotID())->GetIndex();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, GetBotID, GetBotID)
{
	PARAM_SELF_PROLOGUE(DBot);

	ACTION_RETURN_INT(GetBotID(self));
}

static void SetMove(DBot* const self, const int forw, const int side, const int running)
{
	return self->SetMove(static_cast<EBotMoveDirection>(forw), static_cast<EBotMoveDirection>(side), running);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, SetMove, SetMove)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_INT(forw);
	PARAM_INT(side);
	PARAM_INT(running);

	SetMove(self, forw, side, running);
	return 0;
}

static void SetButtons(DBot* const self, const int buttons, const int set)
{
	self->SetButtons(buttons, set);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, SetButtons, SetButtons)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_INT(buttons);
	PARAM_INT(set);

	SetButtons(self, buttons, set);
	return 0;
}

static int IsActorInView(DBot* const self, AActor* const mo, const double fov)
{
	return self->IsActorInView(mo, DAngle::fromDeg(fov));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, IsActorInView, IsActorInView)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_POINTER(mo, AActor);
	PARAM_FLOAT(fov);

	ACTION_RETURN_INT(IsActorInView(self, mo, fov));
}

static int CanReach(DBot* const self, AActor* const mo)
{
	return self->CanReach(mo);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, CanReach, CanReach)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_POINTER(mo, AActor);

	ACTION_RETURN_INT(CanReach(self, mo));
}

static int CheckMissileTrajectory(DBot* const self, const double x, const double y, const double z, const double minDistance, const double maxDistance)
{
	return self->CheckMissileTrajectory({ x, y, z }, minDistance, maxDistance);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, CheckMissileTrajectory, CheckMissileTrajectory)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_FLOAT(minDist);
	PARAM_FLOAT(maxDist);

	ACTION_RETURN_INT(CheckMissileTrajectory(self, x, y, z, minDist, maxDist));
}

static void FindEnemy(DBot* const self, const double fov)
{
	self->FindEnemy(DAngle::fromDeg(fov));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, FindEnemy, FindEnemy)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(fov);

	FindEnemy(self, fov);
	return 0;
}

static void FindPartner(DBot* const self)
{
	self->FindPartner();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, FindPartner, FindPartner)
{
	PARAM_SELF_PROLOGUE(DBot);

	FindPartner(self);
	return 0;
}

static int IsValidItem(DBot* const self, AActor* const item)
{
	return self->IsValidItem(item);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, IsValidItem, IsValidItem)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_POINTER(item, AActor);

	ACTION_RETURN_INT(IsValidItem(self, item));
}

static void PitchTowardsActor(DBot* const self, AActor* const mo)
{
	self->PitchTowardsActor(mo);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, PitchTowardsActor, PitchTowardsActor)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_POINTER(mo, AActor);

	PitchTowardsActor(self, mo);
	return 0;
}

static int FakeCheckPosition(DBot* const self, const double x, const double y, FCheckPosition* const tm, const int actorsOnly)
{
	int res = 0;
	if (tm == nullptr)
	{
		FCheckPosition temp = {};
		res = self->FakeCheckPosition({ x, y }, temp, actorsOnly);
	}
	else
	{
		res = self->FakeCheckPosition({ x, y }, *tm, actorsOnly);
	}

	return res;
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, FakeCheckPosition, FakeCheckPosition)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_POINTER(tm, FCheckPosition);
	PARAM_INT(actorsOnly);

	ACTION_RETURN_INT(FakeCheckPosition(self, x, y, tm, actorsOnly));
}

static void Roam(DBot* const self)
{
	self->Roam();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, Roam, Roam)
{
	PARAM_SELF_PROLOGUE(DBot);

	Roam(self);
	return 0;
}

static int CheckMove(DBot* const self, const double x, const double y)
{
	return self->CheckMove({ x, y });
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, CheckMove, CheckMove)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	ACTION_RETURN_INT(CheckMove(self, x, y));
}

static int Move(DBot* const self)
{
	return self->Move();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, Move, Move)
{
	PARAM_SELF_PROLOGUE(DBot);

	ACTION_RETURN_INT(Move(self));
}

static int TryWalk(DBot* const self)
{
	return self->TryWalk();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, TryWalk, TryWalk)
{
	PARAM_SELF_PROLOGUE(DBot);

	ACTION_RETURN_INT(TryWalk(self));
}

static void NewChaseDir(DBot* const self)
{
	self->NewChaseDir();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, NewChaseDir, NewChaseDir)
{
	PARAM_SELF_PROLOGUE(DBot);

	NewChaseDir(self);
	return 0;
}

static int PickStrafeDirection(DBot* const self, const int startDir)
{
	return self->PickStrafeDirection(static_cast<EBotMoveDirection>(startDir));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, PickStrafeDirection, PickStrafeDirection)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_INT(startDir);

	ACTION_RETURN_INT(PickStrafeDirection(self, startDir));
}
