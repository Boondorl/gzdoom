struct EntityProperties native
{
	// TODO: Return a list of properties? Is that even useful?
	//       Default getters?
	native void SetString(Name key, string value);
	native void SetBool(Name key, bool value);
	native void SetInt(Name key, int value);
	native void SetDouble(Name key, double value);
	native void RemoveProperty(Name key);
	native void ResetProperty(Name key);
	native void ResetAllProperties();

	native clearscope bool HasProperty(Name key) const;
	native clearscope string GetString(Name key, string def = "") const;
	native clearscope bool GetBool(Name key, bool def = false) const;
	native clearscope int GetInt(Name key, int def = 0) const;
	native clearscope double GetDouble(Name key, double def = 0.0) const;
}

enum EBotMoveDirection
{
	MDIR_BACKWARDS = -1,
	MDIR_NONE,
	MDIR_FORWARDS,
	MDIR_NO_CHANGE,

	MDIR_LEFT = MDIR_FORWARDS,
	MDIR_RIGHT = MDIR_BACKWARDS,
	
	MDIR_UP = MDIR_FORWARDS,
	MDIR_DOWN = MDIR_BACKWARDS
}

class Bot : Thinker native
{
	const SIGHT_COOL_DOWN_TICS = int(2.0 * TICRATE);
	const REACTION_TICS = int(0.3 * TICRATE);
	const TARGET_COOL_DOWN_TICS = int(3.0 * TICRATE);
	const EVADE_RANGE_SQ = 640.0 * 640.0;
	const EVADE_COOL_DOWN_TICS = int(1.0 * TICRATE);
	const ITEM_RANGE_SQ = 480.0 * 480.0;
	const GOAL_COOL_DOWN_TICS = int(5.0 * TICRATE);
	const PARTNER_WALK_RANGE_SQ = 320.0 * 320.0;
	const PARTNER_BACK_OFF_RANGE_SQ = 128.0 * 128.0;
	const MAX_ANGLE = 180.0;
	const MAX_TURN_SPEED = 3.0;
	const MAX_TURN_SPEED_BONUS = MAX_TURN_SPEED * 4.0;
	const MAX_FIRE_ANGLE = Cos(5.0);
	const MAX_FIRE_TRACKING_RANGE = 256.0;
	const MIN_RESPAWN_TIME = int(0.5 * TICRATE);
	const MAX_RESPAWN_TIME = int(1.5 * TICRATE);

	native @EntityProperties Properties;

	Actor Evade;

	protected int targetCoolDown;
	protected int evadeCoolDown;
	protected int goalCoolDown;
	protected int reactionCoolDown;
	protected int lastSeenCoolDown;

	native clearscope static EntityProperties GetEntityInfo(Name entity, Name baseClass = 'Actor');
	native clearscope static bool IsSectorDangerous(Sector sec);
	native clearscope static int GetBotCount();

	native clearscope PlayerInfo GetPlayer() const;
	native void SetMove(EBotMoveDirection forward = MDIR_NO_CHANGE, EBotMoveDirection side = MDIR_NO_CHANGE, EBotMoveDirection up = MDIR_NO_CHANGE, bool running = true);
	native void SetButtons(EButtons cmd, bool set);
	native void SetAngle(double destAngle);
	native void SetPitch(double destPitch);
	native void SetRoll(double destRoll);

	native bool IsActorInView(Actor mo, double fov = 60.0);
	native bool CheckShotPath(Vector3 dest, Name projectileType = 'None', double minDistance = 0.0, double maxDistance = 320.0);
	native Actor FindTarget(double fov = 60.0);
	native uint FindPartner();
	native bool IsValidItem(Inventory item);

	native bool FakeCheckPosition(Vector2 dest, out FCheckPosition tm = null, bool actorsOnly = false);
	native bool CanReach(Actor mo, double maxDistance = 320.0, bool jump = true);
	native bool CheckMove(Vector2 dest, bool jump = true);
	native bool Move(bool running = true, bool jump = true);
	native void NewMoveDirection(Actor goal = null, bool running = true, bool jump = true);

	clearscope PlayerPawn GetPawn() const
	{
		return GetPlayer().mo;
	}

	clearscope Actor GetTarget() const
	{
		return GetPawn().target;
	}

	clearscope PlayerPawn GetPartner() const
	{
		let pawn = GetPawn();
		return pawn.friendPlayer > 0u && pawn.friendPlayer <= MAXPLAYERS ? players[pawn.friendPlayer - 1u].mo : null;
	}

	clearscope Actor GetGoal() const
	{
		return GetPawn().goal;
	}

	void SetTarget(Actor target)
	{
		GetPawn().target = target;
	}

	void SetPartner(uint pNum)
	{
		GetPawn().friendPlayer = pNum > MAXPLAYERS ? 0u : pNum;
	}

	void SetGoal(Actor goal)
	{
		GetPawn().goal = goal;
	}

	virtual void BotThink()
	{
		if (GetPlayer().playerState == PST_DEAD)
		{
			BotDeathThink();
			return;
		}

		SearchForPartner();
		SearchForTarget();
		CheckEvade();
		UpdateGoal();
		
		AdjustAngles();
		HandleMovement();
	}

	virtual void BotDeathThink()
	{
		if (GetPlayer().respawn_time <= level.time)
			SetButtons(BT_USE, true);
	}

	virtual void SearchForPartner()
	{
		let partner = GetPartner();
		if (partner && partner.health > 0)
			return;

		if (!deathmatch || teamplay)
			SetPartner(FindPartner());
	}

	virtual void SearchForTarget()
	{
		if (lastSeenCoolDown > 0)
			--lastSeenCoolDown;
		else
			reactionCoolDown = REACTION_TICS;

		let pawn = GetPawn();
		Actor target = GetTarget();
		if (target && (target.health <= 0 || !target.bShootable || pawn.IsFriend(target)
						|| (lastSeenCoolDown <= 0 && !IsActorInView(target))))
		{
			SetTarget(null);
			targetCoolDown = 0;
		}

		target = GetTarget();
		if (target && IsActorInView(target))
			lastSeenCoolDown = SIGHT_COOL_DOWN_TICS;

		if (targetCoolDown > 0)
			--targetCoolDown;

		if (!target || (deathmatch && target.player && targetCoolDown <= 0))
		{
			target = FindTarget();
			if (target)
			{
				SetTarget(target);
				targetCoolDown = TARGET_COOL_DOWN_TICS;
				lastSeenCoolDown = SIGHT_COOL_DOWN_TICS;
			}
		}
	}

	virtual void CheckEvade()
	{
		if (!Evade || !Evade.bMissile)
		{
			Evade = null;
			evadeCoolDown = 0;
		}

		if (evadeCoolDown > 0)
		{
			--evadeCoolDown;
			return;
		}

		let pawn = GetPawn();

		Actor mo;
		Actor closest;
		double closestDist = double.infinity;
		let it = ThinkerIterator.Create("Actor", STAT_DEFAULT);
		while (mo = Actor(it.Next()))
		{
			if (!mo.bMissile || (mo.target && pawn.IsFriend(mo.target)))
				continue;

			let entity = GetEntityInfo(mo.GetClassName());
			if (!entity || !entity.GetBool('bWarn'))
				continue;

			double dist = pawn.Distance3DSquared(mo);
			if (dist <= EVADE_RANGE_SQ && dist < closestDist && IsActorInView(mo))
			{
				closestDist = dist;
				closest = mo;
			}
		}

		if (closest)
		{
			Evade = closest;
			evadeCoolDown = EVADE_COOL_DOWN_TICS;
		}
	}

	virtual void UpdateGoal()
	{
		Actor goal = GetGoal();
		if (!goal)
			goalCoolDown = 0;

		let curItem = Inventory(goal);
		if (curItem && !curItem.bSpecial)
		{
			SetGoal(null);
			curItem = null;
			goalCoolDown = 0;
		}

		if (goalCoolDown > 0)
		{
			--goalCoolDown;
			return;
		}

		PlayerInfo player = GetPlayer();
		let pawn = GetPawn();
		bool isLowHealth = player.health <= int(0.25 * pawn.GetMaxHealth(true));
		if (curItem && curItem.bIsHealth && isLowHealth)
			return;

		Inventory item;
		Inventory closest;
		double closestDist = double.infinity;
		let it = ThinkerIterator.Create("Inventory", STAT_DEFAULT);
		while (item = Inventory(it.Next()))
		{
			double dist = pawn.Distance3DSquared(item);
			if (item.bIsHealth && isLowHealth)
				dist *= 0.75;

			if (dist <= ITEM_RANGE_SQ && dist < closestDist && IsValidItem(item)
				&& IsActorInView(item) && CanReach(item))
			{
				closestDist = dist;
				closest = item;
			}
		}

		if (closest)
		{
			SetGoal(closest);
			goalCoolDown = GOAL_COOL_DOWN_TICS;
			return;
		}

		Actor target = GetTarget();
		if (target && player.readyWeapon)
		{
			bool isMelee;
			double combatRange;
			let weapInfo = GetEntityInfo(player.readyWeapon.GetClassName(), 'Weapon');
			if (weapInfo)
			{
				isMelee = weapInfo.GetBool('Melee');
				combatRange = weapInfo.GetDouble('MaxCombatRange');
			}

			if (combatRange > 0.0)
			{
				double dist = pawn.Distance3D(target);
				if ((!isMelee && dist > combatRange)
					|| (isMelee && dist < combatRange * 2.0 && CanReach(target)))
				{
					SetGoal(target);
					goalCoolDown = GOAL_COOL_DOWN_TICS;
					return;
				}
			}
		}

		Actor partner = GetPartner();
		if (partner)
		{
			SetGoal(partner);
			goalCoolDown = 0;
			return;
		}

		SetGoal(null);
		goalCoolDown = 0;
	}

	virtual void AdjustAngles()
	{
		let pawn = GetPawn();

		Vector3 destPos;
		Vector3 viewPos = pawn.pos.PlusZ(pawn.viewHeight - pawn.floorClip);

		Actor target = GetTarget();
		Actor goal = GetGoal();
		if (target && (lastSeenCoolDown <= 0 || IsActorInView(target)))
		{
			destPos = target.pos.PlusZ(target.height * 0.75 - target.floorClip);
		}
		else if (goal)
		{
			destPos = goal is "Inventory"
						? goal.pos.PlusZ(goal.height * 0.5)
						: goal.pos.PlusZ(goal.height * 0.75);

			destPos.z -= goal.floorClip;
		}
		else
		{
			destPos = viewPos + (pawn.angle.ToVector(), 0.0);
		}

		Vector3 diff = level.Vec3Diff(viewPos, destPos);
		if (!(diff ~== (0.0, 0.0, 0.0)))
		{
			double delta = Actor.DeltaAngle(pawn.angle, diff.xy.Angle());
			double bonus = MAX_TURN_SPEED_BONUS * abs(delta) / MAX_ANGLE;
			double turn = clamp(delta, -MAX_TURN_SPEED - bonus, MAX_TURN_SPEED + bonus);
			SetAngle(pawn.angle + turn);

			turn = clamp(Actor.DeltaAngle(pawn.pitch, -atan2(diff.z, diff.xy.Length())), -MAX_TURN_SPEED, MAX_TURN_SPEED);
			SetPitch(pawn.pitch + turn);
		}
	}

	virtual void HandleMovement()
	{
		TryFire();

		// Dodging takes priority
		let pawn = GetPawn();
		if (Evade)
		{
			double dir = pawn.AngleTo(Evade);
			pawn.moveDir = (int(dir / 45.0) + 4) % 8;
			pawn.moveCount = 0;

			Move();
			return;
		}

		// Too close
		// TODO: Put these in the evade check
		let partner = GetPartner();
		Actor target = GetTarget();
		Actor goal = GetGoal();
		if (partner && partner == goal)
		{
			if (!target && pawn.Distance3DSquared(partner) <= PARTNER_BACK_OFF_RANGE_SQ)
			{
				double dir = pawn.AngleTo(partner);
				pawn.moveDir = (int(dir / 45.0) + 4) % 8;
				pawn.moveCount = 0;

				Move();
				return;
			}
		}

		let player = GetPlayer();
		if (target && player.ReadyWeapon)
		{
			double minRange;
			let weapInfo = GetEntityInfo(player.ReadyWeapon.GetClassName(), 'Weapon');
			if (weapInfo)
				minRange = weapInfo.GetDouble('MinCombatRange');

			if (minRange > 0.0 && pawn.Distance3DSquared(target) <= minRange)
			{
				double dir = pawn.AngleTo(target);
				pawn.moveDir = (int(dir / 45.0) + 4) % 8;
				pawn.moveCount = 0;

				Move();
				return;
			}
		}

		if (--pawn.moveCount < 0 || !Move())
		{
			if (partner && partner == goal && !pawn.CheckSight(partner, 15))
				NewMoveDirection(null);
			else
				NewMoveDirection(goal);
		}
	}

	virtual bool TryFire()
	{
		if (reactionCoolDown > 0)
			--reactionCoolDown;

		let player = GetPlayer();
		Actor target = GetTarget();
		if (!target || reactionCoolDown > 0 || !player.ReadyWeapon || player.AttackDown || !IsActorInView(target))
			return false;

		let pawn = GetPawn();
		Vector3 origPos = pawn.Pos.PlusZ(pawn.ViewHeight - pawn.FloorClip);
		Vector3 destPos = target.Pos.PlusZ(target.Height * 0.75 - target.FloorClip);
		Vector3 dir = level.Vec3Diff(origPos, destPos);
		double dist = dir.Length();

		double minRange, maxRange;
		class<Actor> proj;
		let weapInfo = GetEntityInfo(player.ReadyWeapon.GetClassName(), 'Weapon');
		if (weapInfo)
		{
			minRange = weapInfo.GetDouble('ExplosiveRange');
			maxRange = weapInfo.GetDouble('MaxCombatRange');
			proj = weapInfo.GetString('ProjectileType');
		}

		if ((minRange > 0.0 && dist <= minRange) || (maxRange > 0.0 && dist > maxRange))
			return false;

		if (!(dist ~== 0.0))
		{
			Vector3 facing = (pawn.Angle.ToVector() * Cos(pawn.Pitch), -Sin(pawn.Pitch));
			if (facing dot (dir / dist) < MAX_FIRE_ANGLE)
				return false;
		}

		if (proj && dist < MAX_FIRE_TRACKING_RANGE)
		{
			double speed = Actor.GetDefaultSpeed(proj);
			if (speed > 0.0)
			{
				int tics = int(dist / speed);
				double multi = 1.0 - dist / MAX_FIRE_TRACKING_RANGE;
				destPos.XY = level.Vec2Offset(destPos.XY, target.Vel.XY * tics * multi);
				destPos.Z += target.Vel.Z * 0.2 * multi;
			}
		}

		if (!CheckShotPath(destPos, proj ? proj.GetClassName() : 'None', minRange))
			return false;

		dir = level.Vec3Diff(origPos, destPos);
		SetAngle(dir.XY.Angle() + FRandom[BotAccuracy](-2.5, 2.5));
		SetButtons(BT_ATTACK, true);
		return true;
	}

	virtual void BotSpawned()
	{
		SetPartner(-1);
	}

	virtual void BotRespawned()
	{
		SetTarget(null);
	}

	virtual int BotDamaged(Actor inflictor, Actor source, int damage, Name damageType, EDmgFlags flags = 0, double angle = 0.0)
	{
		if (source == GetTarget())
			lastSeenCoolDown = SIGHT_COOL_DOWN_TICS;

		return damage;
	}

	virtual void BotDied(Actor source, Actor inflictor, EDmgFlags dmgFlags = 0, Name meansOfDeath = 'None')
	{
		evadeCoolDown = targetCoolDown = goalCoolDown = reactionCoolDown = 0;
		Evade = null;
		SetGoal(null);
		SetPartner(-1);
		GetPlayer().respawn_time += Random[BotRespawn](MIN_RESPAWN_TIME, MAX_RESPAWN_TIME);
	}
}

// Deprecated: Bots no longer use this.
class CajunBodyNode : Actor
{
	Default
	{
		+NOSECTOR
		+NOGRAVITY
		+INVISIBLE
	}
}

// Deprecated: Bots no longer use this.
class CajunTrace : Actor
{
	Default
	{
		Speed 12;
		Radius 6;
		Height 8;
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOGRAVITY
		+NOTELEPORT
	}
}
