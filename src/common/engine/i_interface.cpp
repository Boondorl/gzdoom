#include "i_interface.h"
#include "st_start.h"
#include "gamestate.h"
#include "startupinfo.h"
#include "c_cvars.h"
#include "gstrings.h"
#include "version.h"

static_assert(sizeof(void*) == 8,
	"Only LP64/LLP64 builds are officially supported. "
	"Please do not attempt to build for other platforms; "
	"even if the program succeeds in a MAP01 smoke test, "
	"there are e.g. known visual artifacts "
	"<https://forum.zdoom.org/viewtopic.php?f=7&t=75673> "
	"that lead to a bad user experience.");

// Some global engine variables taken out of the backend code.
FStartupScreen* StartWindow;
SystemCallbacks sysCallbacks;
FString endoomName;
bool batchrun;
float menuBlurAmount;

bool AppActive = true;
int chatmodeon;
gamestate_t 	gamestate = GS_STARTUP;
bool ToggleFullscreen;
int 			paused;
bool			pauseext;

FStartupInfo GameStartupInfo;

CVAR(Bool, vid_fps, false, 0)
CVAR(Bool, queryiwad, QUERYIWADDEFAULT, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultiwad, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultargs, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultnetiwad, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultnetargs, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetplayers, 8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnethostport, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetticdup, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetmode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetgamemode, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultnetaddress, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetjoinport, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetpage, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// These have to be passed by the iwad selector since it has no way of knowing the info that's been loaded in.
FStartupSelectionInfo::FStartupSelectionInfo(int defIWAD, int defNetIWAD) : DefaultIWAD(defIWAD), DefaultNetIWAD(defNetIWAD)
{
	DefaultArgs = defaultargs;

	DefaultNetArgs = defaultnetargs;

	DefaultNetPage = defaultnetpage;
	DefaultNetPlayers = defaultnetplayers;
	DefaultNetHostPort = defaultnethostport;
	DefaultNetTicDup = defaultnetticdup;
	DefaultNetMode = defaultnetmode;
	DefaultNetGameMode = defaultnetgamemode;

	DefaultNetAddress = defaultnetaddress;
	DefaultNetJoinPort = defaultnetjoinport;
}

// IWAD selection has to be saved outside of this using the return value.
int FStartupSelectionInfo::SaveInfo()
{
	DefaultArgs.StripLeftRight();

	DefaultNetArgs.StripLeftRight();
	AdditionalNetArgs.StripLeftRight();
	DefaultNetAddress.StripLeftRight();

	if (bNetStart)
	{
		defaultnetargs = DefaultNetArgs.GetChars();
		defaultnetpage = DefaultNetPage;

		if (bHosting)
		{
			defaultnetplayers = DefaultNetPlayers;
			defaultnethostport = DefaultNetHostPort;
			defaultnetticdup = DefaultNetTicDup;
			defaultnetmode = DefaultNetMode;
			defaultnetgamemode = DefaultNetGameMode;
		}
		else
		{
			defaultnetaddress = DefaultNetAddress.GetChars();
			defaultnetjoinport = DefaultNetJoinPort;
		}

		return DefaultNetIWAD;
	}

	defaultargs = DefaultArgs.GetChars();
	return DefaultIWAD;
}

EXTERN_CVAR(Bool, ui_generic)

CUSTOM_CVAR(String, language, "auto", CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
{
	GStrings.UpdateLanguage(self);
	UpdateGenericUI(ui_generic);
	if (sysCallbacks.LanguageChanged) sysCallbacks.LanguageChanged(self);
}

