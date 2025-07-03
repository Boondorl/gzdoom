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

class LauncherWindow : public Widget
{
public:
	static int ExecModal(WadStuff* wads, int numwads, int* defaultiwad, int* defNetIWAD, int* autoloadflags, FString * extraArgs = nullptr, FString* netArgs = nullptr);

	LauncherWindow(WadStuff* wads, int numwads, int* defaultiwad, int* defNetIWAD, int* autoloadflags);
	void UpdateLanguage();

	void Start();
	void Exit();

private:
	void OnClose() override;
	void OnGeometryChanged() override;

	LauncherBanner* Banner = nullptr;
	TabWidget* Pages = nullptr;
	LauncherButtonbar* Buttonbar = nullptr;

	PlayGamePage* PlayGame = nullptr;
	SettingsPage* Settings = nullptr;
	NetworkPage* Network = nullptr;

	int* DefaultIWAD = nullptr;
	int* DefaultNetIWAD = nullptr;

	int ExecResult = 0;
};
