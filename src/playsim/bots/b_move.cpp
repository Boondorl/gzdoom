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

#include "d_player.h" // b_bot.h is defined in here.
#include "dsectoreffect.h"
#include "a_floor.h"
#include "a_ceiling.h"
#include "p_enemy.h"
#include "p_maputl.h"

static FRandom pr_bottrywalk("BotTryWalk");
static FRandom pr_botnewchasedir("BotNewChaseDir");
static FRandom pr_botpickstrafedir("BotPickStrafeDir");

// Borrow some movement tables from p_enemy.cpp.
extern dirtype_t opposite[9];
extern dirtype_t diags[4];

bool P_CheckPosition(AActor* thing, const DVector2& pos, FCheckPosition& tm, bool actorsonly);

// Checks if a sector contains a hazard.
bool DBot::IsSectorDangerous(const sector_t* const sec)
{
    if (sec->damageamount > 0)
        return true;

    if (sec->floordata != nullptr && sec->floordata->IsKindOf(RUNTIME_CLASS(DFloor)))
    {
        const DFloor* const floor = dynamic_cast<DFloor*>(sec->floordata.Get());
        if (floor->m_Crush > 0)
            return true;
    }

    if (sec->ceilingdata != nullptr && sec->ceilingdata->IsKindOf(RUNTIME_CLASS(DCeiling)))
    {
        const DCeiling* const ceiling = dynamic_cast<DCeiling*>(sec->ceilingdata.Get());
        if (ceiling->getCrush() > 0)
            return true;
    }

    return false;
}

// Make sure that bots can't pick up any items when checking their position since they're
// considered actual players and ZDoom throws every interaction inside of P_CheckPosition...
bool DBot::FakeCheckPosition(const DVector2& pos, FCheckPosition& tm, const bool actorsOnly)
{
    const ActorFlags savedFlags = _player->mo->flags;
    _player->mo->flags &= ~MF_PICKUP;
    const bool res = P_CheckPosition(_player->mo, pos, tm, actorsOnly);
    _player->mo->flags = savedFlags;
    return res;
}

// Checks to see if the bot is capable of reaching a target actor taking
// level geometry into account. This is mostly for stepping up stairs and
// avoiding running directly into hazards, but won't take large dropoffs into account.
bool DBot::CanReach(AActor* const target)
{
    if (target == nullptr || _player->mo == target
        || target->ceilingz - target->floorz < _player->mo->Height)
    {
        return false;
    }

    DVector3 dir = _player->mo->Vec3To(target);
    const double dist = dir.XY().Length();
    if (fabs(dir.Z) <= _player->mo->Height && dist < _player->mo->radius * 2.0)
        return true;

    constexpr double MaxRange = 320.0; // Don't check further than ~10m on the xy axes.
    if (dist > MaxRange)
        dir *= MaxRange / dist;

    // Intentionally ignore portals here.
    const DVector2 dest = _player->mo->Pos().XY() + dir.XY();
    constexpr double JumpHeight = 8.0;
    constexpr int MaxBlocks = 3;

    int blockCounter = 0;
    const bool doHazardCheck = !IsSectorDangerous(_player->mo->Sector);
    const sector_t* prevSec = _player->mo->Sector;
    double prevZ = _player->mo->floorz;
    const intercept_t* in = nullptr;
    FPathTraverse it(Level, _player->mo->X(), _player->mo->Y(), dest.X, dest.Y, PT_ADDLINES | PT_ADDTHINGS);
    while ((in = it.Next()) != nullptr)
    {
        if (in->isaline)
        {
            const line_t* const line = in->d.line;
            if (!(line->flags & ML_TWOSIDED) || (line->flags & (ML_BLOCKING | ML_BLOCKEVERYTHING | ML_BLOCK_PLAYERS)))
            {
                return false;
            }
            else
            {
                //Determine if going to use backsector/frontsector.
                sector_t* const sec = (line->backsector == prevSec) ? line->frontsector : line->backsector;
                if (doHazardCheck && IsSectorDangerous(sec))
                    return false;

                const DVector2 hitPos = it.InterceptPoint(in);
                const double z = _player->mo->Z() + dir.Z * in->frac;

                const double ceilZ = NextHighestCeilingAt(sec, hitPos.X, hitPos.Y, z, z + _player->mo->Height);
                const double floorZ = NextLowestFloorAt(sec, hitPos.Y, hitPos.Y, z);

                if (floorZ <= prevZ + _player->mo->MaxStepHeight + JumpHeight
                    && ceilZ - floorZ >= _player->mo->Height)
                {
                    prevZ = floorZ;
                    prevSec = sec;
                    continue;
                }

                return false;
            }
        }
        else
        {
            AActor* const thing = in->d.thing;
            if (thing == _player->mo)
                continue;

            if (thing == target)
                break;

            if (!(thing->flags & MF_SOLID))
                continue;

            // Ignore living things since they'll likely move out of the way.
            if (thing->player != nullptr || (thing->flags3 & MF3_ISMONSTER))
            {
                // Unless there's too many in the way.
                if (++blockCounter >= MaxBlocks)
                    return false;

                continue;
            }

            return false;
        }
    }

    // This is done last in case there was a valid stairway leading up to the target.
    return target->Z() <= prevZ + _player->mo->Height;
}

// Attempts to move towards the bot's goal. Will automatically update
// the player's movedir and yaw to the desired direction.
void DBot::Roam()
{
	if (CanReach(_player->mo->goal))
	{
        _player->mo->Angles.Yaw = _player->mo->AngleTo(_player->mo->goal);
	}
	else if (_player->mo->movedir < DI_NODIR)
	{
		// No point doing this with floating point angles...
		const unsigned int angle = _player->mo->Angles.Yaw.BAMs() & static_cast<unsigned int>(7 << 29);
		const int delta = angle - (_player->mo->movedir << 29);

		if (delta > 0)
            _player->mo->Angles.Yaw -= DAngle45;
		else if (delta < 0)
            _player->mo->Angles.Yaw += DAngle45;
	}

	if (--_player->mo->movecount < 0 || !Move())
		NewChaseDir();
}

// Check to ensure the spot ahead of the bot is a valid place that can be walked. Tries
// to prevent walking over ledges and will automatically jump as well.
bool DBot::CheckMove(const DVector2& pos)
{
    // No jump check since the bot will just warp up ledges anyway.
    if (_player->mo->flags & MF_NOCLIP)
        return true;

    constexpr double JumpHeight = 8.0;
    FCheckPosition tm = {};
    if (!FakeCheckPosition(pos, tm)
        || tm.ceilingz - tm.floorz < _player->mo->Height
        || tm.floorz > _player->mo->floorz + _player->mo->MaxStepHeight + JumpHeight
        || tm.ceilingz < _player->mo->Top()
        || (!(_player->mo->flags & (MF_DROPOFF | MF_FLOAT)) && tm.floorz - tm.dropoffz > _player->mo->MaxDropOffHeight)
        || (!IsSectorDangerous(_player->mo->Sector) && IsSectorDangerous(tm.sector))) // Only do a hazard check if we're not currently in a hazard zone.
    {
        return false;
    }

    // Check if it's jumpable.
    if (tm.floorz > _player->mo->floorz + _player->mo->MaxStepHeight)
        SetButtons(BT_JUMP, true);

    return true;
}

// Try and move the bot in its current movedir.
bool DBot::Move()
{
	if (_player->mo->movedir >= DI_NODIR)
	{
		_player->mo->movedir = DI_NODIR;
		return false;
	}

    const DVector2 pos = DVector2(_player->mo->X() + 8 * xspeed[_player->mo->movedir], _player->mo->Y() + 8 * yspeed[_player->mo->movedir]);
	if (!CheckMove(pos))
        return false;

    SetMove(MDIR_FORWARDS, MDIR_NO_CHANGE, true);
	return true;
}

// Similar to Move() but will also set a cool down on the random turning if it could move.
// Only used when trying to pick a new direction to move.
bool DBot::TryWalk()
{
    if (!Move())
        return false;

    _player->mo->movecount = pr_bottrywalk() & 60;
    return true;
}

void DBot::NewChaseDir()
{
    if (_player->mo->goal == nullptr)
		return;

    const dirtype_t curDir = static_cast<dirtype_t>(_player->mo->movedir);
    const dirtype_t backDir = opposite[curDir];

	const DVector2 delta = _player->mo->Vec2To(_player->mo->goal);
    dirtype_t d[] = { DI_NODIR, DI_NODIR, DI_NODIR };
    if (delta.X > 10.0)
        d[1] = DI_EAST;
    else if (delta.X < -10.0)
        d[1] = DI_WEST;

    if (delta.Y < -10.0)
        d[2] = DI_SOUTH;
    else if (delta.Y > 10.0)
        d[2] = DI_NORTH;

    // Try to walk straight at it.
    if (d[1] != DI_NODIR && d[2] != DI_NODIR)
    {
		_player->mo->movedir = diags[((delta.Y < 0.0) << 1) + (delta.X > 0.0)];
        if (_player->mo->movedir != backDir && TryWalk())
            return;
    }

    // Try a more indirect path.
	if (fabs(delta.Y) > fabs(delta.X) || pr_botnewchasedir() > 200)
	{
        const dirtype_t temp = d[1];
		d[1] = d[2];
		d[2] = temp;
	}

    if (d[1] == curDir)
        d[1] = DI_NODIR;
    if (d[2] == backDir)
        d[2] = DI_NODIR;

    if (d[1] != DI_NODIR)
    {
        _player->mo->movedir = d[1];
        if (TryWalk())
            return;
    }

    if (d[2] != DI_NODIR)
    {
        _player->mo->movedir = d[2];
        if (TryWalk())
            return;
    }

    // No path, so pick another direction.
    if (curDir != DI_NODIR)
    {
        _player->mo->movedir = curDir;
        if (TryWalk())
            return;
    }

    if (pr_botnewchasedir() & 1)
    {
        for (int i = DI_EAST; i <= DI_SOUTHEAST; ++i)
        {
            if (i != backDir)
            {
                _player->mo->movedir = i;
                if (TryWalk())
                    return;
            }
        }
    }
    else
    {
        for (int i = DI_SOUTHEAST; i >= DI_EAST; --i)
        {
            if (i != backDir)
            {
                _player->mo->movedir = i;
                if (TryWalk())
                    return;
            }
        }
    }

    // Out of options, so turn around entirely.
    if (backDir != DI_NODIR)
    {
        _player->mo->movedir = backDir;
        if (TryWalk())
            return;
    }

    // Couldn't move at all.
    _player->mo->movedir = DI_NODIR;
}

// Choose whether to strafe left or right. Will also allow for jumps in those directions.
EBotMoveDirection DBot::PickStrafeDirection(const EBotMoveDirection startDir)
{
    const DVector2 facing = _player->mo->Angles.Yaw.ToVector(_player->mo->radius * 2.0);

    int dir = startDir != MDIR_NONE ? startDir : 2 * (pr_botpickstrafedir() & 1) - 1; // Pick a random starting direction.
    DVector2 offset = DVector2(facing.Y * -dir, facing.X * dir);
    if (!CheckMove(_player->mo->Pos().XY() + offset))
    {
        dir = -dir;
        offset = -offset;
        if (!CheckMove(_player->mo->Pos().XY() + offset))
            dir = 0;
    }

    return static_cast<EBotMoveDirection>(dir);
}
