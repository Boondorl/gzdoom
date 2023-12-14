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

#include "actorinlines.h" // b_bot.h is defined in here via d_player.h.
#include "p_linetracedata.h"

extern int P_GetRealMaxHealth(AActor* actor, int max);

static bool IsHexenArmorValid(AActor* const mo, AActor* const item)
{
	const auto hexenArmor = mo->FindInventory(NAME_HexenArmor);
	if (hexenArmor != nullptr)
	{
		const int slot = item->IntVar(NAME_Health);
		if (slot < 0 || slot > 3)
			return false;

		const double* const slots = hexenArmor->FloatArray(NAME_Slots);
		const double* const increments = hexenArmor->FloatArray(NAME_SlotsIncrement);
		if (item->IntVar(NAME_Amount) <= 0)
		{
			if (slots[slot] < increments[slot])
				return true;
		}
		else
		{
			const double total = slots[0] + slots[1] + slots[2] + slots[3] + slots[4];
			const double max = increments[0] + increments[1] + increments[2] + increments[3] + slots[4] + 20.0;
			if (total < max)
				return true;
		}

		return false;
	}

	return true;
}

// Check to see if the bot is capable of picking up a given item.
bool DBot::IsValidItem(AActor* const item)
{
	// Not something that can be picked up.
	if (item == nullptr || !(item->flags & MF_SPECIAL) || !item->IsKindOf(NAME_Inventory))
		return false;

	if (item->IsKindOf(NAME_Weapon))
	{
		const auto heldWeapon = _player->mo->FindInventory(item->GetClass());
		if (heldWeapon == nullptr)
			return true;

		if ((!alwaysapplydmflags && !deathmatch) || (dmflags & DF_WEAPONS_STAY))
			return false;

		const auto ammo1 = heldWeapon->PointerVar<AActor>(NAME_Ammo1);
		const auto ammo2 = heldWeapon->PointerVar<AActor>(NAME_Ammo2);
		return (ammo1 != nullptr && ammo1->IntVar(NAME_Amount) < ammo1->IntVar(NAME_MaxAmount))
				|| (ammo2 != nullptr && ammo2->IntVar(NAME_Amount) < ammo2->IntVar(NAME_MaxAmount));
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

		return heldAmmo == nullptr || heldAmmo->IntVar(NAME_Amount) < heldAmmo->IntVar(NAME_MaxAmount);
	}
	else if (item->IsKindOf(NAME_PowerupGiver))
	{
		const auto powerup = item->PointerVar<PClassActor>(NAME_PowerupType);
		return powerup != nullptr && _player->mo->FindInventory(powerup) == nullptr;
	}
	else if (item->IsKindOf(NAME_Key) || item->IsKindOf(NAME_PuzzleItem) || item->IsKindOf(NAME_WeaponPiece))
	{
		return _player->mo->FindInventory(item->GetClass()->TypeName) == nullptr;
	}
	else
	{
		constexpr int bIsArmor = 1 << 21;
		constexpr int bIsHealth = 1 << 22;
		
		const int itemFlags = item->IntVar(NAME_ItemFlags);
		if ((itemFlags & bIsHealth) && !item->IsKindOf(NAME_HealthPickup))
		{
			// Unfortunately this has to be checked manually since Megaspheres are set up like this.
			const auto testItem = item->GetClass()->TypeName == NAME_Megasphere
									? GetDefaultByName(NAME_MegasphereHealth)
									: item;

			if (testItem != nullptr)
			{
				const int maxHealth = P_GetRealMaxHealth(_player->mo, testItem->IntVar(NAME_MaxAmount));
				if (_player->health >= maxHealth)
					return false;
			}

			return true;
		}
		
		if (itemFlags & bIsArmor)
		{
			const auto testItem = item->GetClass()->TypeName == NAME_Megasphere
									? GetDefaultByName(NAME_BlueArmorForMegasphere)
									: item;

			if (testItem != nullptr)
			{
				const auto basicArmor = _player->mo->FindInventory(NAME_BasicArmor);
				if (basicArmor != nullptr)
				{
					int total = basicArmor->IntVar(NAME_Amount);

					// Decide more carefully how to pick up armor in case it's a downgrade.
					if (testItem->IsKindOf(NAME_BasicArmorPickup)
						&& total * basicArmor->FloatVar(NAME_SavePercent) >= testItem->IntVar(NAME_SaveAmount) * testItem->FloatVar(NAME_SavePercent))
					{
						return false;
					}

					if (testItem->IsKindOf(NAME_BasicArmorBonus) && total >= testItem->IntVar(NAME_MaxSaveAmount) + testItem->IntVar(NAME_BonusMax))
						return false;
				}

				if (testItem->IsKindOf(NAME_HexenArmor) && !IsHexenArmorValid(_player->mo, testItem))
					return false;
			}

			return true;
		}
	}

	const auto heldItem = _player->mo->FindInventory(item->GetClass());
	return heldItem == nullptr || heldItem->IntVar(NAME_Amount) < heldItem->IntVar(NAME_MaxAmount);
}

// Simple check to see if a given Actor is within view of the bot.
bool DBot::IsActorInView(AActor* const mo, const DAngle& fov)
{
	const DAngle viewFOV = fov <= nullAngle ? DAngle180 : fov;
	return mo != nullptr
			&& absangle(_player->mo->Angles.Yaw, _player->mo->AngleTo(mo)) <= viewFOV
			&& P_CheckSight(_player->mo, mo, SF_SEEPASTSHOOTABLELINES | SF_IGNOREWATERBOUNDARY);
}

// Sets the bot's FriendPlayer value to the player index it wants to stick with. Ignores bots
// since that can cause them to swarm around each other like a hive of angry bees.
unsigned int DBot::FindPartner()
{
	unsigned int newFriend = Level->PlayerNum(_player) + 1;
	double closest = std::numeric_limits<double>::infinity();
	for (unsigned int i = 0; i < MAXPLAYERS; ++i)
	{
		AActor* const client = Level->Players[i]->mo;
		if (Level->PlayerInGame(i) && _player != Level->Players[i] && Level->Players[i]->Bot == nullptr
			&& Level->Players[i]->health > 0 && _player->mo->IsTeammate(client))
		{
			const double dist = _player->mo->Distance3DSquared(client);
			if (dist < closest)
			{
				closest = dist;
				newFriend = i + 1u;
			}
		}
	}

	return newFriend;
}

// Attempts to set the bot's target. If not in deathmatch mode, tries to get a monster within 16 blockmap units.
AActor* DBot::FindTarget(const DAngle& fov)
{
	const DAngle viewFOV = fov <= nullAngle ? DAngle180 : fov;
	if (!deathmatch)
		return P_RoughMonsterSearch(_player->mo, 16, false, false, viewFOV.Degrees());

	constexpr double DarknessRange = 320.0 * 320.0; // Bots can see roughly 10m in front of them in darkness
	constexpr int DarknessThreshold = 50;

	AActor *target = nullptr;
	double closest = std::numeric_limits<double>::infinity();
	for (unsigned int i = 0; i < MAXPLAYERS; ++i)
	{
		AActor* const client = Level->Players[i]->mo;
		if (Level->PlayerInGame(i) && _player != Level->Players[i]
			&& Level->Players[i]->health > 0 && !_player->mo->IsTeammate(client))
		{
			const double dist = _player->mo->Distance3DSquared(client);
			if (dist < closest
				&& (dist <= DarknessRange || client->Sector->GetLightLevel() >= DarknessThreshold)
				&& IsActorInView(client, viewFOV))
			{
				closest = dist;
				target = client;
			}
		}
	}

	return target;
}

// Fires off a series of tracers to emulate a missile moving down along a path. Collision checking
// of the Actor type is intentionally kept lazy since more robust solutions can be written from
// ZScript.
bool DBot::CheckShotPath(const DVector3& dest, const FName& projectileType, const double minDistance)
{
	const PClassActor* missileType = nullptr;
	if (projectileType != NAME_None)
		missileType = PClass::FindActor(projectileType);

	double radius = 0.0, height = 0.0;
	const bool isProjectile = missileType != nullptr;
	if (isProjectile)
	{
		// Don't bother testing against missiles that don't move.
		const auto def = GetDefaultByType(missileType);
		if (def->Speed < EQUAL_EPSILON)
			return false;

		radius = def->radius;
		height = def->Height;
	}

	const DVector3 origin = _player->mo->PosAtZ(_player->mo->Center() - _player->mo->Floorclip + _player->mo->AttackOffset());
	const DVector3 dir = dest - origin + Level->Displacements.getOffset(Level->PointInSector(dest.XY())->PortalGroup, _player->mo->Sector->PortalGroup);
	const double dist = dir.Length();
	if (minDistance >= EQUAL_EPSILON && dist <= minDistance)
		return false;

	// The trajectory check here fires off a series of five line traces in a cross pattern:
	// * One from the center. If not a projectile, stops here.
	// * Two from the furthest extents left/right of the center.
	// * One from the bottom/front center.
	// * One from the top/back center.
	
	const DAngle yaw = dir.Angle();
	const DAngle pitch = dir.Pitch();
	const double middle = height * 0.5;

	int flags = TRF_ABSPOSITION;
	if (isProjectile)
		flags |= (TRF_THRUHITSCAN | TRF_SOLIDACTORS);

	FLineTraceData data = {};
	DVector3 pos = origin.plusZ(middle);
	if (P_LineTrace(_player->mo, yaw, dist, pitch, flags, pos.Z, pos.X, pos.Y, &data)
		&& (data.HitType != TRACE_HitActor || (minDistance >= EQUAL_EPSILON && data.Distance <= minDistance) 
			|| _player->mo->IsFriend(data.HitActor)))
	{
		return false;
	}

	if (!isProjectile)
		return true;

	// Get the radius accounting for the AABB's shape.
	const DVector2 cs = yaw.ToVector();
	const double projRadius = fabs(radius * cs.X) + fabs(radius * cs.Y);
	const DVector2 offset = cs * projRadius;

	pos = origin + DVector3(offset.Y, -offset.X, middle);
	if (P_LineTrace(_player->mo, yaw, dist, pitch, flags, pos.Z, pos.X, pos.Y, &data)
		&& (data.HitType != TRACE_HitActor || (minDistance >= EQUAL_EPSILON && data.Distance <= minDistance)
			|| _player->mo->IsFriend(data.HitActor)))
	{
		return false;
	}

	pos = origin + DVector3(-offset.Y, offset.X, middle);
	if (P_LineTrace(_player->mo, yaw, dist, pitch, flags, pos.Z, pos.X, pos.Y, &data)
		&& (data.HitType != TRACE_HitActor || (minDistance >= EQUAL_EPSILON && data.Distance <= minDistance)
			|| _player->mo->IsFriend(data.HitActor)))
	{
		return false;
	}

	// If the bot is aiming far enough up/down, go forward and backward instead of top and bottom.
	constexpr double PitchLimit = 60.0;
	if (fabs(pitch.Degrees()) <= PitchLimit)
	{
		pos = origin;
		if (P_LineTrace(_player->mo, yaw, dist, pitch, flags, pos.Z, pos.X, pos.Y, &data)
			&& (data.HitType != TRACE_HitActor || (minDistance >= EQUAL_EPSILON && data.Distance <= minDistance)
				|| _player->mo->IsFriend(data.HitActor)))
		{
			return false;
		}

		pos = origin.plusZ(height);
		if (P_LineTrace(_player->mo, yaw, dist, pitch, flags, pos.Z, pos.X, pos.Y, &data)
			&& (data.HitType != TRACE_HitActor || (minDistance >= EQUAL_EPSILON && data.Distance <= minDistance)
				|| _player->mo->IsFriend(data.HitActor)))
		{
			return false;
		}
	}
	else
	{
		pos = origin + DVector3(offset.X, offset.Y, middle);
		if (P_LineTrace(_player->mo, yaw, dist, pitch, flags, pos.Z, pos.X, pos.Y, &data)
			&& (data.HitType != TRACE_HitActor || (minDistance >= EQUAL_EPSILON && data.Distance <= minDistance)
				|| _player->mo->IsFriend(data.HitActor)))
		{
			return false;
		}

		pos = origin + DVector3(-offset.X, -offset.Y, middle);
		if (P_LineTrace(_player->mo, yaw, dist, pitch, flags, pos.Z, pos.X, pos.Y, &data)
			&& (data.HitType != TRACE_HitActor || (minDistance >= EQUAL_EPSILON && data.Distance <= minDistance)
				|| _player->mo->IsFriend(data.HitActor)))
		{
			return false;
		}
	}

	return true;
}
