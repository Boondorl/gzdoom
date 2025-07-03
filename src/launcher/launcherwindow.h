#pragma once

#include <zwidget/core/widget.h>
#include "tarray.h"
#include "zstring.h"

class TabWidget;
class LauncherBanner;
class LauncherButtonbar;
class PlayGamePage;
class SettingsPage;
class NetworkPage;
struct WadStuff;
struct FStartupSelectionInfo;

class LauncherWindow : public Widget
{
public:
	static int ExecModal(WadStuff* wads, int numwads, FStartupSelectionInfo& info, int* autoloadflags);

	LauncherWindow(WadStuff* wads, int numwads, FStartupSelectionInfo& info, int* autoloadflags);
	void UpdateLanguage();

	void Start();
	void Exit();
	bool IsInMultiplayer() const;
	bool IsHosting() const;
	void UpdatePlayButton();

private:
	void OnClose() override;
	void OnGeometryChanged() override;

	LauncherBanner* Banner = nullptr;
	TabWidget* Pages = nullptr;
	LauncherButtonbar* Buttonbar = nullptr;

	PlayGamePage* PlayGame = nullptr;
	SettingsPage* Settings = nullptr;
	NetworkPage* Network = nullptr;

	FStartupSelectionInfo* Info = nullptr;

	int ExecResult = -1;
};
