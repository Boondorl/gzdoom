class ACSDelegate play
{
    enum EInvasionState
    {
        IS_WAITINGFORPLAYERS,
        IS_FIRSTCOUNTDOWN,
        IS_INPROGRESS,
        IS_BOSSFIGHT,
        IS_WAVECOMPLETE,
        IS_COUNTDOWN,
        IS_MISSIONFAILED,
    }

    virtual int GetInvasionWave() { return -1; }
    virtual EInvasionState GetInvasionState() { return -1; }
}
