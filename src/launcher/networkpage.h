#pragma once

#include <zwidget/core/widget.h>
#include "gstrings.h"

class LauncherWindow;
class CheckboxLabel;
class LineEdit;
class ListView;
class TextLabel;
class PushButton;
class TabWidget;
struct WadStuff;

class HostSubPage;
class JoinSubPage;

class NetworkPage : public Widget
{
public:
	NetworkPage(LauncherWindow* launcher, WadStuff* wads, int numwads, int defNetIWAD);

	void UpdateLanguage();
	void Save();
	void StartGame(bool host);

	bool IsStarting() const;
	int GetSelectedGame() const;
	std::string GetExtraArgs() const;
	void SetExtraArgs(const std::string& args);

private:
	void OnGeometryChanged() override;

	LauncherWindow* Launcher = nullptr;

	// Direct hook into the play page for these so their editing is synchronized.
	LineEdit* ParametersEdit = nullptr;
	ListView* IWADsList = nullptr;
	TextLabel* ParametersLabel = nullptr;

	HostSubPage* HostPage = nullptr;
	JoinSubPage* JoinPage = nullptr;
	TabWidget* StartPages = nullptr;
	
	LineEdit* SaveFileEdit = nullptr;
	TextLabel* SaveFileLabel = nullptr;

	bool hosting = false, joining = false;
};

class HostSubPage : public Widget
{
public:
	HostSubPage(NetworkPage* main);

	void UpdateLanguage();
	void BuildCommand(FString& args);

private:
	void OnGeometryChanged() override;
	void OnHostButtonClicked();

	NetworkPage* MainTab = nullptr;

	PushButton* HostButton = nullptr;

	TextLabel* NetModesLabel = nullptr;
	CheckboxLabel* AutoNetmodeCheckbox = nullptr;
	CheckboxLabel* PeerToPeerCheckbox = nullptr;
	CheckboxLabel* PacketServerCheckbox = nullptr;
	ListView* TicDupList = nullptr;
	TextLabel* TicDupLabel = nullptr;
	CheckboxLabel* ExtraTicCheckbox = nullptr;
	CheckboxLabel* BalanceTicsCheckbox = nullptr;

	LineEdit* MaxPlayersEdit = nullptr;
	LineEdit* PortEdit = nullptr;
	TextLabel* MaxPlayersLabel = nullptr;
	TextLabel* PortLabel = nullptr;
};

class JoinSubPage : public Widget
{
public:
	JoinSubPage(NetworkPage* main);

	void UpdateLanguage();
	void BuildCommand(FString& args);

private:
	void OnGeometryChanged() override;
	void OnJoinButtonClicked();

	NetworkPage* MainTab = nullptr;

	PushButton* JoinButton = nullptr;

	LineEdit* AddressEdit = nullptr;
	LineEdit* AddressPortEdit = nullptr;
	TextLabel* AddressLabel = nullptr;
	TextLabel* AddressPortLabel = nullptr;
};
