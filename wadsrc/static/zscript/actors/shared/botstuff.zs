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

	MDIR_RIGHT = MDIR_FORWARDS,
	MDIR_LEFT = MDIR_BACKWARDS,
	
	MDIR_UP = MDIR_FORWARDS,
	MDIR_DOWN = MDIR_BACKWARDS
}

class Bot : Thinker native
{
	const DEF_REACTION_TICS = int(0.25 * TICRATE);
	const DEF_SIGHT_FOV = 60.0;
	const DEF_ITEM_RANGE = 480.0;
	const DARKNESS_THRESHOLD = 50;
	const MIN_RESPAWN_TIME = int(0.5 * TICRATE);
	const MAX_RESPAWN_TIME = int(1.5 * TICRATE);
	const SIGHT_COOL_DOWN_TICS = int(2.0 * TICRATE);
	const TARGET_COOL_DOWN_TICS = int(3.0 * TICRATE);
	const EVADE_COOL_DOWN_TICS = int(1.0 * TICRATE);
	const GOAL_COOL_DOWN_TICS = int(4.0 * TICRATE);
	const BURST_DELAY_TICS = int(0.15 * TICRATE);
	const PARTNER_WALK_RANGE_SQ = 640.0 * 640.0;
	const PARTNER_BACK_OFF_RANGE_SQ = 128.0 * 128.0;
	const MAX_FIRE_TRACKING_RANGE = 256.0;
	const MAX_ANGLE = 180.0;
	const MAX_TURN_SPEED = 4.0;
	const MAX_TURN_SPEED_BONUS = MAX_TURN_SPEED * 3.0;
	const BASE_IMPRECISION = 7.5;

	native @EntityProperties Properties;

	Actor Evade;

	protected Vector3 aimPos;
	protected int targetCoolDown;
	protected int evadeCoolDown;
	protected int goalCoolDown;
	protected int fireCoolDown;
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

	native bool IsActorInView(Actor mo, double fov = DEF_SIGHT_FOV);
	native bool CheckShotPath(Vector3 dest, Name projectileType = 'None', double minDistance = 0.0);
	native Actor FindTarget(double fov = DEF_SIGHT_FOV);
	native uint FindPartner();
	native bool IsValidItem(Inventory item);

	native clearscope double GetJumpHeight() const;
	native bool FakeCheckPosition(Vector2 dest, out FCheckPosition tm = null, bool actorsOnly = false);
	native bool CanReach(Actor mo, bool jump = true);
	native bool CheckMove(Vector2 dest, bool jump = true);
	native bool Move(bool running = true, bool jump = true);
	native void NewMoveDirection(Actor goal = null, bool runAway = false, bool running = true, bool jump = true);

	clearscope PlayerPawn GetPawn() const
	{
		return GetPlayer().Mo;
	}

	clearscope Actor GetTarget() const
	{
		return GetPawn().Target;
	}

	clearscope PlayerPawn GetPartner() const
	{
		let pawn = GetPawn();
		let partner = pawn.FriendPlayer > 0u && pawn.FriendPlayer <= MAXPLAYERS ? Players[pawn.FriendPlayer - 1u].Mo : null;
		if (partner == pawn)
			partner = null;

		return partner;
	}

	clearscope Actor GetGoal() const
	{
		return GetPawn().Goal;
	}

	clearscope bool IsTargetValid(Actor target) const
	{
		let pawn = GetPawn();
		return target && target != pawn && target.Health > 0 && target.bShootable && pawn.IsHostile(target);
	}

	// Buddha is intentionally ignored here.
	clearscope bool IsTargetDamageable(Actor target) const
	{
		return target && !target.bNonshootable && !target.bInvulnerable && !target.bNoDamage
				&& (!target.Player || !(target.Player.Cheats & (CF_GODMODE | CF_GODMODE2)));
	}

	clearscope bool IsPartnerValid(Actor partner) const
	{
		let pawn = GetPawn();
		return partner && partner != pawn && partner.Health > 0 && pawn.IsFriend(partner);
	}

	clearscope EntityProperties GetWeaponInfo(Weapon weap) const
	{
		return weap ? GetEntityInfo(weap.GetClassName(), 'Weapon') : null;
	}

	clearscope double GetRange(EntityProperties props, Name prop, double propDef = 0.0, Name mod = 'None', double modDef = 1.0) const
	{
		double value = props && prop != 'None' ? props.GetDouble(prop, propDef) : propDef;
		value *= mod != 'None' ? Properties.GetDouble(mod, modDef) : modDef;
		return value;
	}

	clearscope double GetRangeSquared(EntityProperties props, Name prop, double propDef = 0.0, Name mod = 'None', double modDef = 1.0) const
	{
		double range = GetRange(props, prop, propDef, mod, modDef);
		return range * range;
	}

	void SetTarget(Actor target)
	{
		GetPawn().Target = target;
	}

	void SetPartner(uint pNum)
	{
		let pawn = GetPawn();
		if (!pNum || pNum > MAXPLAYERS || !PlayerInGame[pNum - 1u])
			pawn.FriendPlayer = pawn.PlayerNumber() + 1;
		else
			pawn.FriendPlayer = pNum;
	}

	void SetGoal(Actor goal)
	{
		GetPawn().Goal = goal;
	}

	virtual void BotThink()
	{
		if (GetPlayer().PlayerState == PST_DEAD)
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
		TryFire();
	}

	virtual void BotDeathThink()
	{
		// Bots will automatically respawn in deathmatch but for coop
		// play they still need to hit the use button.
		if (GetPlayer().Respawn_Time <= Level.Time)
			SetButtons(BT_USE, true);
	}

	virtual void SearchForPartner()
	{
		let partner = GetPartner();
		if (partner)
		{
			if (IsPartnerValid(partner))
				return;

			if (partner == GetGoal())
				SetGoal(null);
			if (partner == Evade)
				Evade = null;

			SetPartner(0u);
		}

		if (!deathmatch || teamplay)
			SetPartner(FindPartner());
	}

	virtual void SearchForTarget()
	{
		if (lastSeenCoolDown > 0)
			--lastSeenCoolDown;
		else
			fireCoolDown = Properties.GetInt('ReactionTime', DEF_REACTION_TICS);

		let pawn = GetPawn();
		Actor target = GetTarget();
		double viewFOV = Properties.GetDouble('ViewFOV', DEF_SIGHT_FOV);
		if (target && (!IsTargetValid(target)
						|| (lastSeenCoolDown <= 0 && !IsActorInView(target, viewFOV))))
		{
			if (target == GetGoal())
				SetGoal(null);
			if (target == Evade)
				Evade = null;

			SetTarget(null);
			targetCoolDown = 0;
		}

		target = GetTarget();
		if (target && IsActorInView(target, viewFOV))
			lastSeenCoolDown = SIGHT_COOL_DOWN_TICS;

		if (targetCoolDown > 0)
			--targetCoolDown;

		let player = GetPlayer();
		Actor attacker = IsTargetValid(player.Attacker) ? player.Attacker : null;
		if (!target || (targetCoolDown <= 0 && (attacker || (deathmatch && target.Player))) || !IsTargetDamageable(target))
		{
			if (attacker)
				target = attacker;
			else
				target = FindTarget(viewFOV);

			if (target)
			{
				SetTarget(target);
				targetCoolDown = TARGET_COOL_DOWN_TICS;
				lastSeenCoolDown = SIGHT_COOL_DOWN_TICS;
				fireCoolDown = 0;
			}
		}

		player.Attacker = null;
	}

	virtual void CheckEvade()
	{
		let player = GetPlayer();
		let pawn = GetPawn();
		let partner = GetPartner();
		Actor target = GetTarget();

		double evasiveness = Properties.GetDouble('Evasiveness', 1.0);
		if (Evade)
		{
			if (Evade == target)
			{
				double minRange = GetRangeSquared(GetWeaponInfo(player.ReadyWeapon), 'MinCombatRange', mod: 'Timidness');
				
				let weapInfo = target.Player ? GetWeaponInfo(target.Player.ReadyWeapon) : null;
				double runRange = GetRangeSquared(weapInfo, 'FrightenRange', modDef: evasiveness);

				double dist = pawn.Distance3DSquared(target);
				if ((minRange <= 0.0 || dist > minRange) && (runRange <= 0.0 || dist > runRange))
					Evade = null;
			}
			else if (Evade == partner)
			{
				if (pawn.Distance3DSquared(partner) > PARTNER_BACK_OFF_RANGE_SQ)
					Evade = null;
			}
			else
			{
				double runRange = GetRangeSquared(GetEntityInfo(Evade.GetClassName()), 'FrightenRange', modDef: evasiveness);
				if (runRange <= 0.0 || pawn.Distance3DSquared(Evade) > runRange)
					Evade = null;
			}
		}

		if (!Evade)
			evadeCoolDown = 0;

		if (evadeCoolDown > 0)
		{
			--evadeCoolDown;
		}
		else if (evasiveness > 0.0)
		{
			Actor mo;
			Actor closest;
			double closestDist = double.infinity;
			double viewFOV = Properties.GetDouble('ViewFOV', DEF_SIGHT_FOV);
			let it = ThinkerIterator.Create("Actor", STAT_DEFAULT);
			while (mo = Actor(it.Next()))
			{
				if ((!mo.bIsMonster && !mo.bMissile)
					|| ((mo.bIsMonster && pawn.IsFriend(mo)) || (mo.bMissile && mo.Target && (mo.Target == pawn || pawn.IsFriend(mo.Target)))))
				{
					continue;
				}

				double runRange = GetRangeSquared(GetEntityInfo(mo.GetClassName()), 'FrightenRange', modDef: evasiveness);
				if (runRange <= 0.0)
					continue;

				double dist = pawn.Distance3DSquared(mo);
				if (dist < closestDist && dist <= runRange && IsActorInView(mo, viewFOV))
				{
					closestDist = dist;
					closest = mo;
				}
			}

			if (closest)
			{
				Evade = closest;
				evadeCoolDown = EVADE_COOL_DOWN_TICS;
				NewMoveDirection(Evade, true);
			}
		}

		if (Evade)
			return;

		if (target)
		{
			double minRange = GetRangeSquared(GetWeaponInfo(player.ReadyWeapon), 'MinCombatRange', mod: 'Timidness');
				
			let weapInfo = target.Player ? GetWeaponInfo(target.Player.ReadyWeapon) : null;
			double runRange = GetRangeSquared(weapInfo, 'FrightenRange', modDef: evasiveness);

			double dist = pawn.Distance3DSquared(target);
			if ((minRange > 0.0 && dist <= minRange) || (runRange > 0.0 && dist <= runRange))
			{
				Evade = target;
				evadeCoolDown = 0;
				NewMoveDirection(Evade, true);
				return;
			}
		}

		if (partner && pawn.Distance3DSquared(partner) <= PARTNER_BACK_OFF_RANGE_SQ)
		{
			Evade = partner;
			evadeCoolDown = 0;
			NewMoveDirection(Evade, true, false);
		}
	}

	virtual void UpdateGoal()
	{
		let curItem = Inventory(GetGoal());
		if (curItem && !IsValidItem(curItem))
		{
			curItem = null;
			SetGoal(null);
		}

		let player = GetPlayer();
		let pawn = GetPawn();
		bool isLowHealth = player.Health <= int(0.25 * pawn.GetMaxHealth(true));
		if (curItem && curItem.bIsHealth && isLowHealth)
			return;

		Actor target = GetTarget();
		if (target && player.ReadyWeapon)
		{
			let weapInfo = GetWeaponInfo(player.ReadyWeapon);
			double chaseRange = GetRangeSquared(weapInfo, 'ChaseRange', mod: 'Aggressiveness');
			double combatRange = GetRangeSquared(weapInfo, 'MaxCombatRange', mod: 'Confidence');

			double dist = pawn.Distance3DSquared(target);
			if ((combatRange > 0.0 && dist > combatRange)
				|| (chaseRange > 0.0 && dist <= chaseRange && CanReach(target)))
			{
				SetGoal(target);
				goalCoolDown = 0;
				return;
			}
		}

		if (target && target == GetGoal())
			SetGoal(null);

		if (!GetGoal())
			goalCoolDown = 0;

		if (goalCoolDown > 0)
			--goalCoolDown;

		// Bots won't steal items as much in coop.
		double itemRange = GetRange(Properties, 'ScavengeRange', DEF_ITEM_RANGE, 'Timidness');
		if (itemRange > 0.0 && goalCoolDown <= 0 && (deathmatch || !Random[BotItem]()))
		{
			Inventory closest;
			double closestDist = double.infinity;
			double viewFOV = Properties.GetDouble('ViewFOV', DEF_SIGHT_FOV);
			double rangeSq = itemRange * itemRange;
			let it = BlockThingsIterator.Create(pawn, itemRange);
			while (it.Next())
			{
				Inventory item = Inventory(it.thing);
				if (!item)
					continue;

				double dist = pawn.Distance3DSquared(item);
				if (item.bBigPowerup || (item.bIsHealth && isLowHealth))
					dist *= 0.5625; // 0.75 * 0.75

				if (dist <= rangeSq && dist < closestDist && IsValidItem(item)
					&& IsActorInView(item, viewFOV) && CanReach(item))
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
		}
		
		let partner = GetPartner();
		Actor goal = GetGoal();
		if (partner && (!goal || partner == goal))
		{
			if (pawn.Distance3DSquared(partner) > PARTNER_WALK_RANGE_SQ
				&& pawn.CheckSight(partner, SF_IGNOREVISIBILITY|SF_SEEPASTSHOOTABLELINES|SF_IGNOREWATERBOUNDARY)
				&& CanReach(partner))
			{
				SetGoal(partner);
			}
			else
			{
				SetGoal(null);
			}

			goalCoolDown = 0;
		}
	}

	virtual void AdjustAngles()
	{
		let player = GetPlayer();
		let pawn = GetPawn();

		bool aimingAtTarget;
		Vector3 viewPos = pawn.Pos.PlusZ(pawn.ViewHeight - pawn.FloorClip);

		Actor target = GetTarget();
		Actor goal = GetGoal();
		if (target && (lastSeenCoolDown > 0 || IsActorInView(target, Properties.GetDouble('ViewFOV', DEF_SIGHT_FOV))))
		{
			aimingAtTarget = true;
			aimPos = target.Pos.PlusZ(target.Height * 0.75 - target.FloorClip);

			class<Actor> proj;
			let weapInfo = GetWeaponInfo(player.ReadyWeapon);
			if (weapInfo)
				proj = weapInfo.GetString('ProjectileType');

			double dist = Level.Vec3Diff(viewPos, aimPos).Length();
			double maxTrackingRange = MAX_FIRE_TRACKING_RANGE * Properties.GetDouble('Predictiveness', 1.0);
			if (proj && maxTrackingRange > 0.0 && dist < maxTrackingRange)
			{
				let def = GetDefaultByType(proj);
				if (def.Speed > 0.0)
				{
					int tics = int(dist / def.Speed);
					double multi = 1.0 - dist / maxTrackingRange;
					aimPos.XY = Level.Vec2Offset(aimPos.XY, target.Vel.XY * tics * multi);
					aimPos.Z += target.Vel.Z * 0.2 * multi;
				}
			}
		}
		else if (goal is "Inventory")
		{
			aimPos = goal.Pos.PlusZ(goal.Height * 0.5 - goal.FloorClip);
		}
		else
		{
			double moveAng = pawn.MoveDir < 8 ? pawn.MoveDir * 45.0 : pawn.Angle;
			aimPos = viewPos + (moveAng.ToVector(), 0.0);
		}

		Vector3 diff = Level.Vec3Diff(viewPos, aimPos);
		if (!(diff ~== (0.0, 0.0, 0.0)))
		{
			double speed = Max(pawn.Speed, 1.0);
			double turnMulti = 1.0;
			if (player.AttackDown)
				turnMulti *= 0.5;
			if (aimingAtTarget)
			{
				if (target.bShadow)
					turnMulti *= 0.5;
				if (target.CurSector.GetLightLevel() <= DARKNESS_THRESHOLD)
					turnMulti *= 0.5;
			}

			// Turning is allowed to be faster if the angle is wider.
			double delta = Actor.DeltaAngle(pawn.Angle, diff.XY.Angle());
			double maxTurn = (MAX_TURN_SPEED + MAX_TURN_SPEED_BONUS * Abs(delta) / MAX_ANGLE) * speed * turnMulti;
			double turn = Clamp(delta, -maxTurn, maxTurn);
			SetAngle(pawn.Angle + turn);

			delta = Actor.DeltaAngle(pawn.Pitch, -Atan2(diff.Z, diff.XY.Length()));
			maxTurn = (MAX_TURN_SPEED + MAX_TURN_SPEED_BONUS * Abs(delta) / MAX_ANGLE) * speed * turnMulti;
			turn = Clamp(delta, -maxTurn, maxTurn);
			SetPitch(pawn.Pitch + turn);
		}
	}

	virtual void HandleMovement()
	{
		Actor goal = GetGoal();

		bool running = Evade || goal || GetTarget();
		if (--GetPawn().MoveCount < 0 || !Move(running))
		{
			bool avoidingPartner = Evade == GetPartner();
			if (Evade && (!avoidingPartner || !goal))
				NewMoveDirection(Evade, true, !avoidingPartner);
			else
				NewMoveDirection(goal, running: running);
		}
	}

	virtual bool TryFire()
	{
		if (fireCoolDown > 0)
		{
			--fireCoolDown;
			return false;
		}

		let player = GetPlayer();
		Actor target = GetTarget();
		if (!target || !player.ReadyWeapon || !IsActorInView(target, Properties.GetDouble('ViewFOV', DEF_SIGHT_FOV)))
			return false;

		let pawn = GetPawn();
		Vector3 origPos = pawn.Pos.PlusZ(pawn.ViewHeight - pawn.FloorClip);
		Vector3 dir = Level.Vec3Diff(origPos, aimPos);
		double dist = dir.Length();

		double minRange, maxRange;
		class<Actor> proj;
		int maxRefire = -1;
		let weapInfo = GetWeaponInfo(player.ReadyWeapon);
		if (weapInfo)
		{
			maxRefire = int(weapInfo.GetInt('MaxRefire', -1) * Properties.GetDouble('Aggressiveness', 1.0));
			minRange = weapInfo.GetDouble('ExplosiveRange');
			maxRange = weapInfo.GetDouble('MaxCombatRange') * Properties.GetDouble('Confidence', 1.0);
			proj = weapInfo.GetString('ProjectileType');
		}

		if (maxRefire >= 0 && player.Refire >= maxRefire)
		{
			fireCoolDown = BURST_DELAY_TICS;
			return false;
		}

		if ((minRange > 0.0 && dist <= minRange) || (maxRange > 0.0 && dist > maxRange))
			return false;

		if (!(dist ~== 0.0))
		{
			// By making this value larger, bots will start shooting before they've aligned
			// their shots, causing them to miss. They're also more likely to keep spraying
			// causing accuracy loss from refiring.
			double imprecisionMulti = 1.0;
			if (target.CurSector.GetLightLevel() <= DARKNESS_THRESHOLD)
				imprecisionMulti += 1.0;
			if (target.bShadow)
				imprecisionMulti += 2.0;

			Vector3 facing = (pawn.Angle.ToVector() * Cos(pawn.Pitch), -Sin(pawn.Pitch));
			if (facing dot (dir / dist) < Cos((BASE_IMPRECISION + Properties.GetDouble('Imprecision')) * imprecisionMulti))
				return false;
		}

		if (!CheckShotPath(aimPos, proj ? proj.GetClassName() : 'None', minRange))
			return false;

		SetButtons(BT_ATTACK, true);
		return true;
	}

	virtual string ModifySpawnProperty(Name property, string value)
	{
		return value;
	}

	virtual void BotRespawned()
	{
		SetTarget(null);

		let player = GetPlayer();
		let pawn = GetPawn();

		player.ReadyWeapon = null;
		player.PendingWeapon = pawn.PickNewWeapon(null);

		pawn.MoveDir = int(pawn.Angle / 45.0);
	}

	virtual int BotDamaged(Actor inflictor, Actor source, int damage, Name damageType, EDmgFlags flags = 0, double angle = 0.0)
	{
		if (source == GetTarget())
			lastSeenCoolDown = SIGHT_COOL_DOWN_TICS;

		return damage;
	}

	virtual void BotDied(Actor source, Actor inflictor, EDmgFlags dmgFlags = 0, Name meansOfDeath = 'None')
	{
		aimPos = (0.0, 0.0, 0.0);
		evadeCoolDown = targetCoolDown = goalCoolDown = fireCoolDown = lastSeenCoolDown = 0;
		Evade = null;
		SetGoal(null);
		SetPartner(0u);
		GetPlayer().Respawn_Time += Random[BotRespawn](MIN_RESPAWN_TIME, MAX_RESPAWN_TIME);
	}

	virtual void BotKilled(Actor victim) {}
	virtual void AddedInventory(Inventory item) {}
	virtual void RemovedInventory(Inventory item) {}
	virtual void UsedInventory(Inventory item, bool useFailed) {}
	virtual void Morphed() {}
	virtual void Unmorphed() {}
	virtual void FiredWeapon(bool altFire) {}
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
