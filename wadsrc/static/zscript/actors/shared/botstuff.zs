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

	MDIR_RIGHT = MDIR_BACKWARDS,
	MDIR_LEFT = MDIR_FORWARDS
}

class Bot : Thinker native
{
	const ANG_TO_CMD = 65536.0 / 360.0;
	const FIRE_FOV = 60.0;
	const COMBAT_TIME = 2 * TICRATE;
	const ROAM_TIME = 4 * TICRATE;
	const STRAFE_COOL_DOWN = 5;
	const MIN_RESPAWN_TIME = int(0.25 * TICRATE);
	const MAX_RESPAWN_TIME = int(1.0 * TICRATE);
	const MAX_MONSTER_RANGE = 768.0;
	const EVADE_RANGE = 672.0;
	const COMBAT_RANGE = 512.0;
	const PARTNER_RANGE = 224.0;
	const DARKNESS_THRESHOLD = 50;
	const MAX_TURN = 15.0;

	native EntityProperties Properties;

	Actor Evade;
	double DestAngle;

	protected int combatTics;
	protected int reactionTics;
	protected int roamTics;
	protected int strafeTics;
	protected EBotMoveDirection strafeDir;
	protected Vector3 prevPos;
	protected bool bHasToReact;

	native clearscope static EntityProperties GetWeaponInfo(class<Weapon> weap);
	native clearscope static bool IsSectorDangerous(Sector sec);
	native clearscope static int GetBotCount();

	native clearscope PlayerInfo GetPlayer() const;
	native void SetMove(EBotMoveDirection forward, EBotMoveDirection side, bool running);
	native void SetButtons(EButtons cmd, bool set);
	native bool IsActorInView(Actor mo, double fov = 90.0);
	native bool CanReach(Actor mo);
	native bool CheckMissileTrajectory(Vector3 dest, double minDistance = 0.0, double maxDistance = 320.0);
	native void FindEnemy(double fov = 0.0);
	native void FindPartner();
	native bool IsValidItem(Inventory item);
	native void PitchTowardsActor(Actor mo);
	native bool FakeCheckPosition(Vector2 dest, out FCheckPosition tm = null, bool actorsOnly = false);
	native void Roam();
	native bool CheckMove(Vector2 dest);
	native bool Move();
	native bool TryWalk();
	native void NewChaseDir();
	native EBotMoveDirection PickStrafeDirection();

	clearscope Actor GetActor() const
	{
		return GetPlayer().mo;
	}

	clearscope Actor GetTarget() const
	{
		return GetActor().target;
	}

	clearscope Actor GetPartner() const
	{
		Actor player = GetActor();
		return player.friendPlayer > 0u ? players[player.friendPlayer - 1u].mo : null;
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
		GetActor().friendPlayer = pNum < 0 ? 0 : pNum + 1;
	}

	void SetGoal(Actor goal)
	{
		GetActor().goal = goal;
	}

	override void Tick()
	{
		Actor player = GetActor();
		Actor target = GetTarget();
		Actor goal = GetGoal();

		Actor mo;
		let it = ThinkerIterator.Create("Actor", STAT_DEFAULT);
		while (mo = Actor(it.Next()))
		{
			if (mo.bIsMonster)
			{
				if (mo.health > 0 && mo != target
					&& player.Distance2D(mo) < MAX_MONSTER_RANGE
					&& player.CheckSight(mo, SF_SEEPASTBLOCKEVERYTHING))
				{
					SetTarget(mo);
				}
			}
			else if (bMissile)
			{
				if (!Evade && mo.bWarnBot && mo.target != player && IsActorInView(mo))
					Evade = mo;
			}
			else if (mo.bSpecial && (!goal || !goal.bSpecial) && IsValidItem(mo))
			{
				goal = mo;
				SetGoal(goal);
				roamTics = ROAM_TIME;
			}
		}
	}

	virtual void BotThink()
	{
		Actor target = GetTarget();
		if (target && target.health <= 0)
			SetTarget(null);

		PlayerInfo player = GetPlayer();
		if (player.playerState != PST_DEAD)
		{
			Actor mo = GetActor();
			double curYaw = mo.angle;
			double curPitch = mo.pitch;

			SearchForPartner();
			SearchForTarget();
			HandleMovement();
			AdjustAngles();

			player.cmd.yaw = Actor.DeltaAngle(curYaw, mo.angle) * ANG_TO_CMD;
			player.cmd.pitch = Actor.DeltaAngle(curPitch, mo.pitch) * ANG_TO_CMD;
			if (player.cmd.pitch == -32768)
				player.cmd.pitch = -32767;

			mo.angle = curYaw;
			mo.pitch = curPitch;
		}

		if (combatTics > 0)
			--combatTics;
		if (reactionTics > 0)
			--reactionTics;
		if (roamTics > 0)
			--roamTics;
		if (strafeTics > 0)
			--strafeTics;

		if (player.playerState == PST_DEAD && player.respawn_time <= level.time)
			SetButtons(BT_USE, true);
	}

	virtual void SearchForPartner()
	{
		if (teamplay || !deathmatch)
			FindPartner();
	}

	virtual void SearchForTarget()
	{
		Actor mo = GetActor();
		Actor target = GetTarget();

		Actor prevTarget;
		if (target && target.health > 0 && mo.CheckSight(target))
			prevTarget = target;

		if (deathmatch || !target)
		{
			FindTarget(target ? 360.0 : 0.0);
			target = GetTarget();
			if (!target)
				target = prevTarget;
		}

		if (target && (target.health < 0 || !target.bShootable || mo.IsFriend(target)))
			target = null;

		SetTarget(target);
	}

	virtual void AdjustAngles()
	{
		PlayerInfo player = GetPlayer();
		Actor mo = GetActor();
		Actor target = GetTarget();
		Actor goal = GetGoal();

		double turn = MAX_TURN;
		if (player.readyWeapon)
		{
			EntityProperties weapInfo = GetWeaponInfo(player.readyWeapon.GetClass());
			if (weapInfo)
			{
				class<Actor> projType = weapInfo.GetString('ProjectileType');
				if (target && !goal && !projType && weapInfo.GetFloat('MoveCombatDist') > 0.0
					&& IsActorInView(target, FIRE_FOV + 5.0))
				{
					turn = 3.0;
				}
			}
		}

		double delta = Actor.DeltaAngle(mo.angle, DestYaw);
		if (abs(delta) < 5.0 && !target)
			return;

		mo.angle += clamp(delta / 3.0, -turn, turn);
	}

	virtual void HandleMovement()
	{
		PlayerInfo player = GetPlayer();
		Actor mo = GetActor();
		Actor target = GetTarget();
		Actor partner = GetPartner();
		Actor goal = GetGoal();

		EntityProperties weapInfo = player.readyWeapon ? GetWeaponInfo(player.readyWeapon.GetClass()) : null;
		double combatDist = weapInfo ? weapInfo.GetFloat('MoveCombatDist') : 0.0;

		if (Evade &&
			((Evade.default.bMissile && !Evade.bMissile) || !IsActorInView(Evade)))
		{
			strafeDir = MDIR_NONE;
			Evade = null;
		}

		bool doRoam;
		if (Evade && mo.Distance2D(Evade) < EVADE_RANGE)
		{
			PitchTowardsActor(Evade);
			DestYaw = mo.AngleTo(Evade);
			SetMove(MDIR_BACKWARDS, strafeDir, true);

			if (strafeTics <= 0)
			{
				strafeTics = STRAFE_COOL_DOWN;
				strafeDir = PickStrafeDirection(strafeDir);
			}

			if (target && IsActorInView(target, FIRE_FOV))
				TryFire();
		}
		else if (target && mo.CheckSight(target, 0))
		{
			PitchTowardsActor(target);

			if (goal.bSpecial && goal is "Inventory")
			{
				if (((player.health < Properties.GetInt('ISP') && (goal.bIsHealth || goal.bBigPowerup))
						|| goal < COMBAT_RANGE*0.25 || combatDist <= 0.0)
					&& ((goal ? mo.Distance2D(goal) : 0.0) < COMBAT_RANGE || combatDist <= 0.0)
					&& CanReach(goal))
				{
					doRoam = true;
				}
			}

			if (!doRoam)
			{
				SetGoal(null);
				if (combatDist <= 0)
					mo.bDropOff = false;

				if (!target.bIsMonster)
					combatTics = COMBAT_TIME;

				bool stuck;
				if (strafeTics <= 0
					&& ((mo.pos.xy - prevPos.xy).LengthSquared() < 1.0 || Random[Bot](0, 29) == 10))
				{
					stuck = true;
					strafeDir = PickStrafeDirection();
				}

				DestYaw = mo.AngleTo(target);

				EBotMoveDirection fDir = MDIR_NONE;
				if (player.readyWeapon || mo.Distance2D(target) > combatDist)
					fDir = MDIR_FORWARDS;
				else if (!stuck)
					fDir = MDIR_BACKWARDS;

				SetMove(fDir, strafeDir, !target.bIsMonster);
				TryFire();
			}
		}
		else if (partner && !target && (!goal || goal == partner))
		{
			PitchTowardsActor(partner);
			if (!CanReach(partner))
			{
				if (partner == goal && Random[Bot]() < 32)
					SetGoal(null);

				doRoam = true;
			}

			if (!doRoam)
			{
				DestYaw = mo.AngleTo(partner);
				double dist = mo.Distance2D(partner);
				if (dist > PARTNER_RANGE * 2.0)
					SetMove(MDIR_FORWARDS, MDIR_NO_CHANGE, true);
				else if (dist > PARTNER_RANGE)
					SetMove(MDIR_FORWARDS, MDIR_NO_CHANGE, false);
				else if (dist < PARTNER_RANGE - PARTNER_RANGE/3.0)
					SetMove(MDIR_BACKWARDS, MDIR_NO_CHANGE, false);
			}
		}
		else
		{
			doRoam = true;
			bHasToReact = true;
		}

		if (doRoam)
		{
			if (target && IsActorInView(target))
				TryFire();

			if (goal
				&& ((goal.default.bShootable && goal.health <= 0) || (goal is "Inventory" && !goal.bSpecial)))
			{
				SetGoal(null);
			}

			if (!goal)
			{
				if (target && combatTics > 0)
				{
					if (target.player)
					{
						EntityProperties targInfo = target.player.readyWeapon ? GetWeaponInfo(target.player.readyWeapon.GetClass()) : null;
						if (((targInfo && targInfo.GetBool('bExplosive')) || Random[Bot](0, 99) > Properties.GetInt('ISP'))
							&& combatDist > 0.0)
						{
							SetGoal(target);
						}
						else
						{
							DestYaw = mo.AngleTo(target);
						}
					}
					else
					{
						SetGoal(target);
					}
				}
				else
				{
					bool foundDest;
					int r = Random[Bot]();
					if (r < 128)
					{
						Inventory item;
						let it = ThinkerIterator.Create("Inventory", STAT_DEFAULT);
						if (item = Inventory(it.Next()))
						{
							r &= 63;
							Inventory prev;
							do
							{
								prev = item;
								item = Inventory(it.Next());
							} while (item && --r > 0)

							SetGoatl(item ? item : prev);
							foundDest = true;
						}
					}

					if (!foundDest && partner && (r < 179 || mo.CheckSight(partner)))
					{
						SetGoal(partner);
						foundDest = true;
					}

					if (!foundDest)
					{
						int start = r & (MAXPLAYERS-1);
						int end = start - 1;
						if (end < 0)
							end = MAXPLAYERS-1;

						for (int i = start; i != end; i = (i+1) & (MAXPLAYERS-1))
						{
							if (playerInGame[i] && players[i].health > 0)
							{
								SetGoal(players[i].mo);
								break;
							}
						}
					}
				}

				if (GetGoal())
					roamTics = ROAM_TIME;
			}

			if (GetGoal())
				Roam();
		}

		if (roamTics <= 0 && GetGoal())
			SetGoal(null);

		if (combatTics < COMBAT_TIME / 2)
			mo.bDropOff = true;

		prevPos = mo.pos;
	}

	virtual bool TryFire()
	{
		Actor target = GetTarget();
		if (!target || !target.bShootable || target.health <= 0)
			return false;

		PlayerInfo player = GetPlayer();
		if (!player.readyWeapon)
			return false;

		if (player.damageCount > Properties.GetInt('ISP'))
		{
			bHasToReact = true;
			return false;
		}

		EntityProperties weapInfo = GetWeaponInfo(player.readyWeapon.GetClass());
		if (bHasToReact && (!weapInfo || !weapInfo.GetBool('bNoReactionTime')))
			reactionTics = (100 - Properties.GetInt('ReactionTime') + 1) / (Random[Bot](0, 2) + 3);

		bHasToReact = false;
		if (reactionTics > 0)
			return false;

		Actor mo = GetActor();
		double dist = mo.Distance3D(target);
		class<Actor> projType;
		if (weapInfo)
			projType = weapInfo.GetString('ProjectileType');

		if (weapInfo && weapInfo.GetFloat('MoveCombatDist') <= 0.0)
		{
			if (dist > DEFMELEERANGE*4.0)
				return false;
		}
		else if (weapInfo && weapInfo.GetBool('bBFG'))
		{
			if (Random[Bot](0, 199) > Properties.GetInt('Reaction') || !IsActorInView(target, FIRE_FOV))
				return false;
		}
		else if (projType)
		{
			double speed = GetDefaultByType(projType).speed;
			if (speed <= 0.0)
				return false;

			if (!IsActorInView(target, FIRE_FOV))
				return false;

			Vector3 targetPos = level.Vec3Offset(target.pos, target.vel * int(dist/speed));
			if (!CheckMissileTrajectory(targetPos, weapInfo.GetFloat('ExplosiveDist')))
				return false;

			DestYaw = (targetPos - mo.pos).xy.Angle();
		}
		else
		{
			DestYaw = mo.AngleTo(target);
			int aimPenalty;
			if (target.bShadow)
				aimPenalty += Random[Bot](0, 24) + 10;
			if (target.curSector.GetLightLevel() < DARKNESS_THRESHOLD)
				aimPenalty += Random[Bot](0, 39);
			if (player.damageCount)
				aimPenalty += player.damageCount;

			int aim = max(Properties.GetInt('Aiming') - aimPenalty, 0);
			double inaccuracy = max((FIRE_FOV * 0.5) - (aim * FIRE_FOV/200.0), 0.0);
			DestYaw += inaccuracy * RandomPick[Bot](-1, 1);

			if (!IsActorInView(target, FIRE_FOV * 0.5))
				return false;
		}

		SetButtons(BT_ATTACK, true);
		return true;
	}

	virtual void BotSpawned()
	{
		SetPartner(-1);
		DestAngle = GetActor().angle;
		bHasToReact = true;
	}

	virtual void BotRespawned()
	{
		SetTarget(null);
		DestAngle = GetActor().angle;
	}

	virtual void BotDied(Actor source, Actor inflictor, EDmgFlags dmgFlags = 0, Name meansOfDeath = 'None')
	{
		combatTics = reactionTics = roamTics = strafeTics = 0;
		Evade = null;
		SetGoal(null);
		SetPartner(-1);
		bHasToReact = true;
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
