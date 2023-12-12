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
	const ANG_TO_CMD = 65536.0 / 360.0;
	const TARGET_COOL_DOWN_TICS = int(3.0 * TICRATE);
	const EVADE_RANGE_SQ = 640.0 * 640.0;
	const EVADE_COOL_DOWN_TICS = int(1.0 * TICRATE);
	const ITEM_RANGE_SQ = 480.0 * 480.0;
	const GOAL_COOL_DOWN_TICS = int(5.0 * TICRATE);
	const MAX_TURN_SPEED = 3.0;
	const MIN_RESPAWN_TIME = int(0.25 * TICRATE);
	const MAX_RESPAWN_TIME = int(1.0 * TICRATE);

	native EntityProperties Properties;

	Actor Evade;

	protected int partnerCoolDown;
	protected int targetCoolDown;
	protected int evadeCoolDown;
	protected int goalCoolDown;
	protected int reactionCoolDown;
	protected int strafeCoolDown;
	protected EBotMoveDirection curStrafeDir;

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
	native bool CheckMissileTrajectory(Vector3 dest, double minDistance = 0.0, double maxDistance = 320.0);
	native Actor FindTarget(double fov = 60.0);
	native uint FindPartner();
	native bool IsValidItem(Inventory item);

	native bool FakeCheckPosition(Vector2 dest, out FCheckPosition tm = null, bool actorsOnly = false);
	native bool CanReach(Actor mo, double maxDistance = 320.0, bool jump = true);
	native bool CheckMove(Vector2 dest, bool jump = true);
	native bool Move(bool jump = true);
	native void NewChaseDir(bool jump = true);
	native EBotMoveDirection PickStrafeDirection(EBotMoveDirection startDir = MDIR_NONE, bool jump = true);

	clearscope PlayerPawn GetActor() const
	{
		return GetPlayer().mo;
	}

	clearscope Actor GetTarget() const
	{
		return GetActor().target;
	}

	clearscope PlayerPawn GetPartner() const
	{
		let player = GetActor();
		return player.friendPlayer > 0u && player.friendPlayer <= MAXPLAYERS ? players[player.friendPlayer - 1u].mo : null;
	}

	clearscope Actor GetGoal() const
	{
		return GetActor().goal;
	}

	void SetTarget(Actor target)
	{
		GetActor().target = target;
	}

	void SetPartner(int pNum)
	{
		GetActor().friendPlayer = pNum < 0 || pNum >= MAXPLAYERS ? 0 : pNum + 1;
	}

	void SetGoal(Actor goal)
	{
		GetActor().goal = goal;
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
		// TODO: Check partner status
		if (!deathmatch || teamplay)
			FindPartner();
	}

	virtual void SearchForTarget()
	{
		let mo = GetActor();
		Actor target = GetTarget();
		if (target && (target.health <= 0 || !target.bShootable || mo.IsFriend(target)
						|| (/*outOfCombatCoolDown <= 0 && */!IsActorInView(target))))
		{
			SetTarget(null);
			targetCoolDown = 0;
		}

		if (targetCoolDown > 0)
			--targetCoolDown;

		if (!GetTarget() || (deathmatch && targetCoolDown <= 0))
		{
			FindTarget();
			if (GetTarget())
				targetCoolDown = TARGET_COOL_DOWN_TICS;
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

		let pawn = GetActor();

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
		let mo = GetActor();
		bool isLowHealth = player.health <= int(0.25 * mo.GetMaxHealth(true));
		if (curItem && curItem.bIsHealth && isLowHealth)
			return;

		Inventory item;
		Inventory closest;
		double closestDist = double.infinity;
		let it = ThinkerIterator.Create("Inventory", STAT_DEFAULT);
		while (item = Inventory(it.Next()))
		{
			double dist = mo.Distance3DSquared(item);
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
			let weapInfo = GetEntityInfo(player.readyWeapon.GetClassName(), 'Weapon');
			double combatRange = weapInfo ? weapInfo.GetDouble('CombatRange') : 0.0;
			if (combatRange > 0.0)
			{
				double dist = mo.Distance3D(target);
				bool isMelee = combatRange <= DEFMELEERANGE * 2.0;

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
			goalCoolDown = GOAL_COOL_DOWN_TICS;
			return;
		}

		SetGoal(null);
		goalCoolDown = GOAL_COOL_DOWN_TICS;
	}

	virtual void AdjustAngles()
	{
		let pawn = GetActor();

		Vector3 destPos;
		Vector3 viewPos = pawn.pos.PlusZ(pawn.viewHeight - pawn.floorClip);

		Actor target = GetTarget();
		Actor goal = GetGoal();
		if (target)
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
			double turn = clamp(Actor.DeltaAngle(pawn.angle, diff.xy.Angle()), -MAX_TURN_SPEED, MAX_TURN_SPEED);
			SetAngle(pawn.angle + turn);

			turn = clamp(Actor.DeltaAngle(pawn.pitch, -atan2(diff.z, diff.xy.Length())), -MAX_TURN_SPEED, MAX_TURN_SPEED);
			SetPitch(pawn.pitch + turn);
		}
	}

	virtual void HandleMovement()
	{
		if (--GetActor().movecount < 0 || !Move())
			NewChaseDir();
	}

	virtual bool TryFire()
	{
		return false;
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
		return damage;
	}

	virtual void BotDied(Actor source, Actor inflictor, EDmgFlags dmgFlags = 0, Name meansOfDeath = 'None')
	{
		partnerCoolDown = evadeCoolDown = targetCoolDown = goalCoolDown = reactionCoolDown = strafeCoolDown = 0;
		curStrafeDir = MDIR_NONE;
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
