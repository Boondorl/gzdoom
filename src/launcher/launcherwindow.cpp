#include "launcherwindow.h"
#include "launcherbanner.h"
#include "launcherbuttonbar.h"
#include "playgamepage.h"
#include "settingspage.h"
#include "networkpage.h"
#include "v_video.h"
#include "version.h"
#include "i_interface.h"
#include "gstrings.h"
#include "c_cvars.h"
#include <zwidget/core/resourcedata.h>
#include <zwidget/window/window.h>
#include <zwidget/widgets/tabwidget/tabwidget.h>

int LauncherWindow::ExecModal(WadStuff* wads, int numwads, int* defaultiwad, int* defNetIWAD, int* autoloadflags, FString * extraArgs, FString* netArgs)
{
	Size screenSize = GetScreenSize();
	double windowWidth = 615.0;
	double windowHeight = 700.0;

	auto launcher = std::make_unique<LauncherWindow>(wads, numwads, defaultiwad, defNetIWAD, autoloadflags);
	launcher->SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);
	if(extraArgs) launcher->PlayGame->SetExtraArgs(extraArgs->GetChars());
	if (netArgs) launcher->Network->SetExtraArgs(netArgs->GetChars());
	launcher->Show();

	DisplayWindow::RunLoop();

	if(extraArgs && launcher->ExecResult > 0) *extraArgs = launcher->PlayGame->GetExtraArgs();
	if (netArgs && launcher->ExecResult < 0) *netArgs = launcher->Network->GetExtraArgs();

	return launcher->ExecResult;
}

LauncherWindow::LauncherWindow(WadStuff* wads, int numwads, int* defaultiwad, int* defNetIWAD, int* autoloadflags) : Widget(nullptr, WidgetType::Window)
{
	SetWindowTitle(GAMENAME);

	DefaultIWAD = defaultiwad;
	DefaultNetIWAD = defNetIWAD;

	Banner = new LauncherBanner(this);
	Pages = new TabWidget(this);
	Buttonbar = new LauncherButtonbar(this);

	PlayGame = new PlayGamePage(this, wads, numwads, defaultiwad ? *defaultiwad : 0);
	Settings = new SettingsPage(this, autoloadflags);
	Network = new NetworkPage(this, wads, numwads, defNetIWAD ? *defNetIWAD : 0);

	Pages->AddTab(PlayGame, "Play");
	Pages->AddTab(Settings, "Settings");
	Pages->AddTab(Network, "Multiplayer");

	UpdateLanguage();

	Pages->SetCurrentWidget(PlayGame);
	PlayGame->SetFocus();
}

void LauncherWindow::Start()
{
	Settings->Save();
	Network->Save();

	if (DefaultIWAD)
		*DefaultIWAD = PlayGame->GetSelectedGame();
	if (DefaultNetIWAD)
		*DefaultNetIWAD = Network->GetSelectedGame();

	ExecResult = Network->IsStarting() ? -1 : 1;
	DisplayWindow::ExitLoop();
}

void LauncherWindow::Exit()
{
	ExecResult = 0;
	DisplayWindow::ExitLoop();
}

void LauncherWindow::UpdateLanguage()
{
	Pages->SetTabText(PlayGame, GStrings.GetString("PICKER_TAB_PLAY"));
	Pages->SetTabText(Settings, GStrings.GetString("OPTMNU_TITLE"));
	Pages->SetTabText(Network, "Multiplayer");
	PlayGame->UpdateLanguage();
	Settings->UpdateLanguage();
	Network->UpdateLanguage();
	Buttonbar->UpdateLanguage();
}

void LauncherWindow::OnClose()
{
	Exit();
}

void LauncherWindow::OnGeometryChanged()
{
	double top = 0.0;
	double bottom = GetHeight();

	Banner->SetFrameGeometry(0.0, top, GetWidth(), Banner->GetPreferredHeight());
	top += Banner->GetPreferredHeight();

	bottom -= Buttonbar->GetPreferredHeight();
	Buttonbar->SetFrameGeometry(0.0, bottom, GetWidth(), Buttonbar->GetPreferredHeight());

	Pages->SetFrameGeometry(0.0, top, GetWidth(), std::max(bottom - top, 0.0));
}
