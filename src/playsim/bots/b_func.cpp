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
#include "p_linetracedata.h"

extern int P_GetRealMaxHealth(AActor* actor, int max);

// Check to see if the bot is capable of picking up a given item.
bool DBot::IsValidItem(AActor* const item)
{
	// Not something that can be picked up.
	if (item == nullptr || !(item->flags & MF_SPECIAL) || !item->IsKindOf(NAME_Inventory))
		return false;

	constexpr int bIsHealth = 1 << 22;
	if (item->IsKindOf(NAME_Weapon))
	{
		const auto heldWeapon = _player->mo->FindInventory(item->GetClass());
		if (heldWeapon != nullptr)
		{
			if ((!alwaysapplydmflags && !deathmatch) || (dmflags & DF_WEAPONS_STAY))
				return false;

			const auto ammo1 = heldWeapon->PointerVar<AActor>(NAME_Ammo1);
			const auto ammo2 = heldWeapon->PointerVar<AActor>(NAME_Ammo2);
			if ((ammo1 == nullptr || ammo1->IntVar(NAME_Amount) >= ammo1->IntVar(NAME_MaxAmount))
				&& (ammo2 == nullptr || ammo2->IntVar(NAME_Amount) >= ammo2->IntVar(NAME_MaxAmount)))
			{
				return false;
			}
		}
	}
	else if (item->IsKindOf(NAME_Ammo))
	{
		// Boon TODO: Check this
		AActor* heldAmmo = nullptr;
		PClassActor* parent = nullptr;
		IFVIRTUALPTRNAME(item, NAME_Ammo, GetParentAmmo)
		{
			void* retVal = nullptr;
			VMValue params[] = { item };
			VMReturn ret[] = { &retVal };

			VMCall(func, params, 1, ret, 1);
			parent = static_cast<PClassActor*>(retVal);
		}

		if (parent != nullptr)
			heldAmmo = _player->mo->FindInventory(parent);

		if (heldAmmo != nullptr && heldAmmo->IntVar(NAME_Amount) >= heldAmmo->IntVar(NAME_MaxAmount))
			return false;
	}
	else if (item->IntVar(NAME_ItemFlags) & bIsHealth) // Boon TODO: Make sure this works
	{
		// Unfortunately this has to be checked manually since Megaspheres are horribly set up.
		const auto testItem = item->GetClass()->TypeName == NAME_Megasphere
								? GetDefaultByName(NAME_MegasphereHealth)
								: item;

		if (testItem != nullptr)
		{
			const int maxHealth = P_GetRealMaxHealth(_player->mo, testItem->IntVar(NAME_MaxAmount));
			if (_player->health >= maxHealth)
				return false;
		}
	}

	return true;
}

// Simple check to see if a given Actor is within view of the bot.
bool DBot::IsActorInView(AActor* const mo, const DAngle& fov)
{
	return mo != nullptr && fov > nullAngle
			&& (fov >= DAngle360 || absangle(_player->mo->Angles.Yaw, _player->mo->AngleTo(mo)) <= fov * 0.5)
			&& P_CheckSight(_player->mo, mo, SF_SEEPASTBLOCKEVERYTHING);
}

// Aim the bots pitch towards the Actor.
void DBot::PitchTowardsActor(AActor* const target)
{
	_player->mo->Angles.Pitch = target != nullptr ? _player->mo->Vec3To(target).Pitch() : nullAngle;
}

// Sets the bot's FriendPlayer value to the player index it wants to stick with.
void DBot::FindPartner()
{
	// Check if current partner is still alive.
	unsigned int newFriend = _player->mo->FriendPlayer;
	if (newFriend > 0u && (newFriend > MAXPLAYERS || Level->Players[newFriend - 1u]->health <= 0))
		newFriend = 0u;

	if (newFriend)
		return;

	double closest = std::numeric_limits<double>::infinity();
	for (unsigned int i = 0; i < MAXPLAYERS; ++i)
	{
		AActor* const client = Level->Players[i]->mo;
		if (Level->PlayerInGame(i)
			&& _player->mo != client
			&& Level->Players[i]->health > 0
			&& (!deathmatch || ((_player->health / 2) <= Level->Players[i]->health && _player->mo->IsTeammate(client))))
		{
			const double dist = _player->mo->Distance3DSquared(client);
			if (dist < closest && P_CheckSight(_player->mo, client, SF_IGNOREVISIBILITY))
			{
				closest = dist;
				newFriend = i + 1u;
			}
		}
	}

	_player->mo->FriendPlayer = newFriend;
}

// Attempts to set the bot's target. If not in deathmatch mode, tries to get a monster within 20 blockmap units.
void DBot::FindEnemy(const DAngle& fov)
{
	const DAngle viewFov = fov <= nullAngle ? DAngle360 : fov;
	if (!deathmatch)
	{
		_player->mo->target = P_RoughMonsterSearch(_player->mo, 20, false, false, viewFov.Degrees() * 0.5);
		return;
	}

	constexpr double DarknessRange = 320.0 * 320.0; // Bots can see roughly 10m in front of them in darkness
	constexpr int DarknessThreshold = 50;

	AActor *target = nullptr;
	double closest = std::numeric_limits<double>::infinity();
	for (unsigned int i = 0; i < MAXPLAYERS; ++i)
	{
		AActor* const client = Level->Players[i]->mo;
		if (Level->PlayerInGame(i)
			&& _player->mo != client
			&& Level->Players[i]->health > 0
			&& !_player->mo->IsTeammate(client))
		{
			const double dist = _player->mo->Distance3DSquared(client);
			if (dist < closest
				&& (dist <= DarknessRange || client->Sector->GetLightLevel() >= DarknessThreshold)
				&& IsActorInView(client, viewFov))
			{
				closest = dist;
				target = client;
			}
		}
	}

	_player->mo->target = target;
}

// Fires off a series of tracers to emulate a missile moving down along a path. Collision checking
// of the Actor type is intentionally kept lazy since more robust solutions can be written from
// ZScript.
bool DBot::CheckMissileTrajectory(const DVector3& dest, const double minDistance, const double maxDistance)
{
	if (_player->ReadyWeapon == nullptr)
		return false;

	const PClass* missileType = nullptr;
	const auto& weapInfo = DBotManager::BotWeaponInfo.CheckKey(_player->ReadyWeapon->GetClass()->TypeName);
	if (weapInfo != nullptr)
		missileType = PClass::FindClass(weapInfo->GetString("ProjectileType"));

	// Don't use this with hitscan weapons.
	if (missileType == nullptr)
		return false;

	// Don't bother testing against missiles that don't move.
	const auto def = GetDefaultByType(missileType);
	if (def->Speed < EQUAL_EPSILON)
		return false;

	const DVector3 origin = _player->mo->PosAtZ(_player->mo->Center() - _player->mo->Floorclip + _player->mo->AttackOffset());
	const DVector3 dir = dest - origin + Level->Displacements.getOffset(Level->PointInSector(dest.XY())->PortalGroup, _player->mo->Sector->PortalGroup);
	const double dist = dir.Length();
	if (minDistance >= EQUAL_EPSILON && dist <= minDistance)
		return false;
	if (dist <= def->radius * 2.0)
		return true;

	// The trajectory check here fires off a series of five line traces in a cross pattern:
	// * One from the center.
	// * Two from the furthest extents left/right of the center.
	// * One from the bottom/front center.
	// * One from the top/back center.
	
	const DAngle yaw = dir.Angle();
	const double range = maxDistance >= EQUAL_EPSILON && dist > maxDistance ? maxDistance : dist;
	const DAngle pitch = dir.Pitch();
	constexpr int Flags = TRF_ABSPOSITION | TRF_THRUHITSCAN | TRF_SOLIDACTORS;
	const double middle = def->Height * 0.5;

	FLineTraceData data = {};
	DVector3 pos = origin.plusZ(middle);
	if (P_LineTrace(_player->mo, yaw, range, pitch, Flags, pos.Z, pos.X, pos.Y, &data)
		&& (data.HitType != TRACE_HitActor || _player->mo->IsFriend(data.HitActor)))
	{
		return false;
	}

	// Get the radius accounting for the AABB's shape.
	const DVector2 cs = yaw.ToVector();
	const double radius = fabs(def->radius * cs.X) + fabs(def->radius * cs.Y);
	const DVector2 offset = cs * radius;

	pos = origin + DVector3(offset.Y, -offset.X, middle);
	if (P_LineTrace(_player->mo, yaw, range, pitch, Flags, pos.Z, pos.X, pos.Y, &data)
		&& (data.HitType != TRACE_HitActor || _player->mo->IsFriend(data.HitActor)))
	{
		return false;
	}

	pos = origin + DVector3(-offset.Y, offset.X, middle);
	if (P_LineTrace(_player->mo, yaw, range, pitch, Flags, pos.Z, pos.X, pos.Y, &data)
		&& (data.HitType != TRACE_HitActor || _player->mo->IsFriend(data.HitActor)))
	{
		return false;
	}

	// If the bot is aiming far enough up/down, go forward and backward instead of top and bottom.
	constexpr double PitchLimit = 60.0;
	if (fabs(pitch.Degrees()) <= PitchLimit)
	{
		pos = origin;
		if (P_LineTrace(_player->mo, yaw, range, pitch, Flags, pos.Z, pos.X, pos.Y, &data)
			&& (data.HitType != TRACE_HitActor || _player->mo->IsFriend(data.HitActor)))
		{
			return false;
		}

		pos = origin.plusZ(def->Height);
		if (P_LineTrace(_player->mo, yaw, range, pitch, Flags, pos.Z, pos.X, pos.Y, &data)
			&& (data.HitType != TRACE_HitActor || _player->mo->IsFriend(data.HitActor)))
		{
			return false;
		}
	}
	else
	{
		pos = origin + DVector3(offset.X, offset.Y, middle);
		if (P_LineTrace(_player->mo, yaw, range, pitch, Flags, pos.Z, pos.X, pos.Y, &data)
			&& (data.HitType != TRACE_HitActor || _player->mo->IsFriend(data.HitActor)))
		{
			return false;
		}

		pos = origin + DVector3(-offset.X, -offset.Y, middle);
		if (P_LineTrace(_player->mo, yaw, range, pitch, Flags, pos.Z, pos.X, pos.Y, &data)
			&& (data.HitType != TRACE_HitActor || _player->mo->IsFriend(data.HitActor)))
		{
			return false;
		}
	}

	return true;
}
