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
#include "p_enemy.h"
#include "p_maputl.h"

static FRandom pr_bottrywalk("BotTryWalk");
static FRandom pr_botnewchasedir("BotNewChaseDir");
static FRandom pr_botpickstrafedir("BotPickStrafeDir");

extern bool P_CheckPosition(AActor* thing, const DVector2& pos, FCheckPosition& tm, bool actorsonly);

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
bool DBot::CanReach(AActor* const mo, const bool doJump)
{
    if (mo == nullptr)
        return false;

    if (_player->mo->flags & MF_NOCLIP)
        return true;

    if (mo->ceilingz - mo->floorz < _player->mo->Height)
        return false;

    // Intentionally ignore portals here.
    const DVector3 dir = _player->mo->Vec3To(mo);
    const DVector2 dest = _player->mo->Pos().XY() + dir.XY();
    const double jumpHeight = doJump && Level->IsJumpingAllowed() ? max(_player->mo->FloatVar(NAME_JumpZ), 0.0) : 0.0;
    constexpr int MaxBlocks = 3;

    int blockCounter = 0;
    const bool doHazardCheck = !IsSectorDangerous(_player->mo->Sector);
    const sector_t* prevSec = _player->mo->Sector;
    double prevZ = _player->mo->floorz;
    const intercept_t* in = nullptr;
    FPathTraverse it = { Level, _player->mo->X(), _player->mo->Y(), dest.X, dest.Y, PT_ADDLINES | PT_ADDTHINGS };
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
                const double hitZ = _player->mo->Z() + dir.Z * in->frac;

                const double ceilZ = NextHighestCeilingAt(sec, hitPos.X, hitPos.Y, hitZ, hitZ + _player->mo->Height);
                const double floorZ = NextLowestFloorAt(sec, hitPos.Y, hitPos.Y, hitZ);

                if (floorZ <= prevZ + _player->mo->MaxStepHeight + jumpHeight
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

            if (thing == mo)
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
    return mo->Z() <= prevZ + _player->mo->MaxStepHeight + jumpHeight;
}

// Check to ensure the spot ahead of the bot is a valid place that can be walked. Tries
// to prevent walking over ledges and will automatically jump as well.
bool DBot::CheckMove(const DVector2& pos, const bool doJump)
{
    // No jump check since the bot will just warp up ledges anyway.
    if (_player->mo->flags & MF_NOCLIP)
        return true;

    const double jumpHeight = doJump && Level->IsJumpingAllowed() ? max(_player->mo->FloatVar(NAME_JumpZ), 0.0) : 0.0;
    FCheckPosition tm = {};
    if (!FakeCheckPosition(pos, tm)
        || tm.ceilingz - tm.floorz < _player->mo->Height
        || tm.floorz > _player->mo->floorz + _player->mo->MaxStepHeight + jumpHeight
        || tm.ceilingz < _player->mo->Top()
        || (!(_player->mo->flags & (MF_DROPOFF | MF_FLOAT)) && tm.floorz - tm.dropoffz > _player->mo->MaxDropOffHeight)
        || (!IsSectorDangerous(_player->mo->Sector) && IsSectorDangerous(tm.sector))) // Only do a hazard check if we're not currently in a hazard zone.
    {
        return false;
    }

    // Check if it's jumpable.
    if (doJump && tm.floorz > _player->mo->floorz + _player->mo->MaxStepHeight)
        SetButtons(BT_JUMP, true);

    return true;
}

// Try and move the bot in its current movedir.
bool DBot::Move(const bool running, const bool doJump)
{
	if (_player->mo->movedir >= DI_NODIR)
	{
		_player->mo->movedir = DI_NODIR;
		return false;
	}

    const DVector2 pos = { _player->mo->X() + (_player->mo->radius - 1.0) * xspeed[_player->mo->movedir],
                            _player->mo->Y() + (_player->mo->radius - 1.0) * yspeed[_player->mo->movedir] };

	if (!CheckMove(pos, doJump))
        return false;

    constexpr double MinForward = 60.0;
    constexpr double MaxForward = 120.0;
    constexpr double MinSide = 30.0;
    constexpr double MaxSide = 150.0;

    const double delta = deltaangle(_player->mo->Angles.Yaw, DAngle45 * _player->mo->movedir).Degrees();
    const double absAng = fabs(delta);

    EBotMoveDirection forw = absAng <= MinForward || absAng >= MaxForward ? MDIR_FORWARDS : MDIR_NO_CHANGE;
    EBotMoveDirection side = absAng >= MinSide || absAng <= MaxSide ? MDIR_LEFT : MDIR_NO_CHANGE;
    if (side == MDIR_LEFT && delta < 0.0)
        side = MDIR_RIGHT;
    if (forw == MDIR_FORWARDS && absAng > 90.0)
        forw = MDIR_BACKWARDS;

    SetMove(forw, side, MDIR_NO_CHANGE, running);
    return true;
}

// Similar to Move() but will also set a cool down on the random turning if it could move.
// Only used when trying to pick a new direction to move.
bool DBot::TryWalk(const bool running, const bool doJump)
{
    if (!Move(running, doJump))
        return false;

    constexpr int CoolDown = TICRATE;
    _player->mo->movecount = pr_bottrywalk() % CoolDown;
    return true;
}

void DBot::NewMoveDirection(AActor* const goal, const bool runAway, const bool running, const bool doJump)
{
    int baseDir = 0;
    if (goal != nullptr)
    {
        constexpr double AngToDir = 1.0 / 45.0;

        double desired = _player->mo->AngleTo(goal).Degrees();
        while (desired < 0.0)
            desired += 360.0;

        baseDir = static_cast<int>(desired * AngToDir);
        if (runAway)
            baseDir = (baseDir + 4) % 8;
    }
    else
    {
        baseDir = pr_botnewchasedir() & 7;
    }

    // Try and walk straight towards the goal, slowly shifting sides unless it needs to
    // turn around entirely.
    if (baseDir == _player->mo->movedir && (pr_botnewchasedir() & 1))
        baseDir = ((pr_botnewchasedir() & 1) * 2 - 1 + baseDir) % 8;

    _player->mo->movedir = baseDir;
    if (TryWalk(running, doJump))
        return;

    _player->mo->movedir = (baseDir + 1) % 8;
    if (TryWalk(running, doJump))
        return;

    _player->mo->movedir = (baseDir - 1) % 8;
    if (TryWalk(running, doJump))
        return;

    _player->mo->movedir = (baseDir + 2) % 8;
    if (TryWalk(running, doJump))
        return;

    _player->mo->movedir = (baseDir - 2) % 8;
    if (TryWalk(running, doJump))
        return;

    _player->mo->movedir = (baseDir + 3) % 8;
    if (TryWalk(running, doJump))
        return;

    _player->mo->movedir = (baseDir - 3) % 8;
    if (TryWalk(running, doJump))
        return;

    _player->mo->movedir = (baseDir + 4) % 8;
    if (TryWalk(running, doJump))
        return;

    // Couldn't move at all.
    _player->mo->movedir = DI_NODIR;
}
