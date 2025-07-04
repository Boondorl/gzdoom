
#include "networkpage.h"
#include "launcherwindow.h"
#include "gstrings.h"
#include "c_cvars.h"
#include "i_net.h"
#include "i_interface.h"
#include <zwidget/core/resourcedata.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <zwidget/widgets/lineedit/lineedit.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/tabwidget/tabwidget.h>


NetworkPage::NetworkPage(LauncherWindow* launcher, WadStuff* wads, int numwads, FStartupSelectionInfo& info) : Widget(launcher), Launcher(launcher)
{
	ParametersEdit = new LineEdit(this);
	ParametersLabel = new TextLabel(this);
	SaveFileEdit = new LineEdit(this);
	SaveFileLabel = new TextLabel(this);
	SaveFileCheckbox = new CheckboxLabel(this);
	SaveArgsCheckbox = new CheckboxLabel(this);
	IWADsList = new ListView(this);

	SaveFileCheckbox->SetChecked(info.bSaveNetFile);
	if (!info.DefaultNetSaveFile.IsEmpty())
		SaveFileEdit->SetText(info.DefaultNetSaveFile.GetChars());

	SaveArgsCheckbox->SetChecked(info.bSaveNetArgs);
	if (!info.DefaultNetArgs.IsEmpty())
		ParametersEdit->SetText(info.DefaultNetArgs.GetChars());

	StartPages = new TabWidget(this);
	HostPage = new HostSubPage(this, info);
	JoinPage = new JoinSubPage(this, info);

	StartPages->AddTab(HostPage, "Host");
	StartPages->AddTab(JoinPage, "Join");

	for (int i = 0; i < numwads; i++)
	{
		const char* filepart = strrchr(wads[i].Path.GetChars(), '/');
		if (filepart == nullptr)
			filepart = wads[i].Path.GetChars();
		else
			filepart++;

		FString work;
		if (*filepart) work.Format("%s (%s)", wads[i].Name.GetChars(), filepart);
		else work = wads[i].Name.GetChars();

		IWADsList->AddItem(work.GetChars());
	}

	if (info.DefaultNetIWAD >= 0 && info.DefaultNetIWAD < numwads)
	{
		IWADsList->SetSelectedItem(info.DefaultNetIWAD);
		IWADsList->ScrollToItem(info.DefaultNetIWAD);
	}

	switch (info.DefaultNetPage)
	{
	case 1:
		StartPages->SetCurrentWidget(JoinPage);
		break;
	default:
		StartPages->SetCurrentWidget(HostPage);
		break;
	}
}

void NetworkPage::SetValues(FStartupSelectionInfo& info) const
{
	info.DefaultNetIWAD = IWADsList->GetSelectedItem();
	info.DefaultNetArgs = ParametersEdit->GetText();

	info.bHosting = IsInHost();
	if (info.bHosting)
	{
		info.DefaultNetPage = 0;
		HostPage->SetValues(info);
	}
	else
	{
		info.DefaultNetPage = 1;
		JoinPage->SetValues(info);
	}

	info.bSaveNetFile = SaveFileCheckbox->GetChecked();
	info.bSaveNetArgs = SaveArgsCheckbox->GetChecked();
	const auto save = SaveFileEdit->GetText();
	if (!save.empty())
		info.AdditionalNetArgs.AppendFormat(" -loadgame %s", save.c_str());
	info.DefaultNetSaveFile = save;
}

void NetworkPage::UpdatePlayButton()
{
	Launcher->UpdatePlayButton();
}

bool NetworkPage::IsInHost() const
{
	return StartPages->GetCurrentIndex() >= 0 ? StartPages->GetCurrentWidget() == HostPage : false;
}

void NetworkPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	const double wSize = w * 0.45 - 2.5;

	double y = h - SaveArgsCheckbox->GetPreferredHeight();
	SaveArgsCheckbox->SetFrameGeometry(0.0, y, wSize, SaveArgsCheckbox->GetPreferredHeight());

	y -= SaveFileCheckbox->GetPreferredHeight();
	SaveFileCheckbox->SetFrameGeometry(0.0, y, wSize, SaveFileCheckbox->GetPreferredHeight());

	y -= ParametersLabel->GetPreferredHeight() + 4.0;
	ParametersEdit->SetFrameGeometry(0.0, y, wSize, ParametersLabel->GetPreferredHeight() + 2.0);
	y -= ParametersLabel->GetPreferredHeight();
	ParametersLabel->SetFrameGeometry(0.0, y, wSize, ParametersLabel->GetPreferredHeight());

	y -= SaveFileLabel->GetPreferredHeight() + 4.0;
	SaveFileEdit->SetFrameGeometry(0.0, y, wSize, SaveFileLabel->GetPreferredHeight() + 2.0);
	y -= SaveFileLabel->GetPreferredHeight();
	SaveFileLabel->SetFrameGeometry(0.0, y, 110.0, SaveFileLabel->GetPreferredHeight());
	y -= 5.0;

	IWADsList->SetFrameGeometry(0.0, 0.0, wSize, y);

	const double xOfs = w * 0.45 + 2.5;
	StartPages->SetFrameGeometry(xOfs, 0.0, w - xOfs, h);
}

void NetworkPage::UpdateLanguage()
{
	ParametersLabel->SetText(GStrings.GetString("PICKER_ADDPARM"));
	SaveFileLabel->SetText("Load Save File:");
	SaveFileCheckbox->SetText("Remember Save File");
	SaveArgsCheckbox->SetText("Remember Parameters");

	StartPages->SetTabText(HostPage, "Host");
	StartPages->SetTabText(JoinPage, "Join");
	HostPage->UpdateLanguage();
	JoinPage->UpdateLanguage();
}

HostSubPage::HostSubPage(NetworkPage* main, FStartupSelectionInfo& info) : Widget(nullptr), MainTab(main)
{
	NetModesLabel = new TextLabel(this);
	AutoNetmodeCheckbox = new CheckboxLabel(this);
	PacketServerCheckbox = new CheckboxLabel(this);
	PeerToPeerCheckbox = new CheckboxLabel(this);

	AutoNetmodeCheckbox->SetRadioStyle(true);
	PacketServerCheckbox->SetRadioStyle(true);
	PeerToPeerCheckbox->SetRadioStyle(true);
	AutoNetmodeCheckbox->FuncChanged = [this](bool on) { if (on) { PacketServerCheckbox->SetChecked(false); PeerToPeerCheckbox->SetChecked(false); }};
	PacketServerCheckbox->FuncChanged = [this](bool on) { if (on) { AutoNetmodeCheckbox->SetChecked(false); PeerToPeerCheckbox->SetChecked(false); }};
	PeerToPeerCheckbox->FuncChanged = [this](bool on) { if (on) { PacketServerCheckbox->SetChecked(false); AutoNetmodeCheckbox->SetChecked(false); }};
	
	switch (info.DefaultNetMode)
	{
	case 0:
		PeerToPeerCheckbox->SetChecked(true);
		break;
	case 1:
		PacketServerCheckbox->SetChecked(true);
		break;
	default:
		AutoNetmodeCheckbox->SetChecked(true);
		break;
	}

	TicDupList = new ListView(this);
	TicDupLabel = new TextLabel(this);
	ExtraTicCheckbox = new CheckboxLabel(this);

	std::vector<double> widths = { 30.0, 30.0 };
	TicDupList->SetColumnWidths(widths);
	TicDupList->AddItem("35.0");
	TicDupList->UpdateItem("Hz", 0, 1);
	TicDupList->AddItem("17.5");
	TicDupList->UpdateItem("Hz", 1, 1);
	TicDupList->AddItem("11.6");
	TicDupList->UpdateItem("Hz", 2, 1);
	TicDupList->SetSelectedItem(info.DefaultNetTicDup);
	ExtraTicCheckbox->SetChecked(info.DefaultNetExtraTic);

	GameModesLabel = new TextLabel(this);
	CoopCheckbox = new CheckboxLabel(this);
	DeathmatchCheckbox = new CheckboxLabel(this);
	AltDeathmatchCheckbox = new CheckboxLabel(this);
	TeamDeathmatchCheckbox = new CheckboxLabel(this);
	TeamLabel = new TextLabel(this);
	TeamEdit = new LineEdit(this);

	// These are intentionally not radio buttons, they just act similarly for clarity in the UI.
	CoopCheckbox->FuncChanged = [this](bool on) { if (on) { DeathmatchCheckbox->SetChecked(false); TeamDeathmatchCheckbox->SetChecked(false); }};
	DeathmatchCheckbox->FuncChanged = [this](bool on) { if (on) { CoopCheckbox->SetChecked(false); TeamDeathmatchCheckbox->SetChecked(false); }};
	TeamDeathmatchCheckbox->FuncChanged = [this](bool on) { if (on) { CoopCheckbox->SetChecked(false); DeathmatchCheckbox->SetChecked(false); }};

	switch (info.DefaultNetGameMode)
	{
	case 0:
		CoopCheckbox->SetChecked(true);
		break;
	case 1:
		DeathmatchCheckbox->SetChecked(true);
		break;
	case 2:
		TeamDeathmatchCheckbox->SetChecked(true);
		break;
	}
	AltDeathmatchCheckbox->SetChecked(info.DefaultNetAltDM);

	TeamEdit->SetMaxLength(3);
	TeamEdit->SetNumericMode(true);
	TeamEdit->SetTextInt(info.DefaultNetHostTeam);

	MaxPlayersEdit = new LineEdit(this);
	PortEdit = new LineEdit(this);
	MaxPlayersLabel = new TextLabel(this);
	PortLabel = new TextLabel(this);

	MaxPlayersEdit->SetMaxLength(2);
	MaxPlayersEdit->SetNumericMode(true);
	MaxPlayersEdit->SetTextInt(info.DefaultNetPlayers);
	PortEdit->SetMaxLength(5);
	PortEdit->SetNumericMode(true);
	if (info.DefaultNetHostPort > 0)
		PortEdit->SetTextInt(info.DefaultNetHostPort);

	MaxPlayerHintLabel = new TextLabel(this);
	PortHintLabel = new TextLabel(this);
	TeamHintLabel = new TextLabel(this);

	MaxPlayerHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
	PortHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
	TeamHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
}

void HostSubPage::SetValues(FStartupSelectionInfo& info) const
{
	info.AdditionalNetArgs = "";
	if (PacketServerCheckbox->GetChecked())
	{
		info.AdditionalNetArgs.AppendFormat(" -netmode 1");
		info.DefaultNetMode = 1;
	}
	else if (PeerToPeerCheckbox->GetChecked())
	{
		info.AdditionalNetArgs.AppendFormat(" -netmode 0");
		info.DefaultNetMode = 0;
	}
	else
	{
		info.DefaultNetMode = -1;
	}

	info.DefaultNetExtraTic = ExtraTicCheckbox->GetChecked();
	if (info.DefaultNetExtraTic)
		info.AdditionalNetArgs.AppendFormat(" -extratic");

	const int dup = TicDupList->GetSelectedItem();
	if (dup > 0)
		info.AdditionalNetArgs.AppendFormat(" -dup %d", dup + 1);
	info.DefaultNetTicDup = dup;

	info.DefaultNetPlayers = clamp<int>(MaxPlayersEdit->GetTextInt(), 1, MAXPLAYERS);
	info.AdditionalNetArgs.AppendFormat(" -host %d", info.DefaultNetPlayers);
	const int port = clamp<int>(PortEdit->GetTextInt(), 0, UINT16_MAX);
	if (port > 0)
	{
		info.AdditionalNetArgs.AppendFormat(" -port %d", port);
		info.DefaultNetHostPort = port;
	}
	else
	{
		info.DefaultNetHostPort = 0;
	}

	info.DefaultNetAltDM = AltDeathmatchCheckbox->GetChecked();
	if (CoopCheckbox->GetChecked())
	{
		info.AdditionalNetArgs.AppendFormat(" -coop");
		info.DefaultNetGameMode = 0;
	}
	else if (DeathmatchCheckbox->GetChecked())
	{
		if (AltDeathmatchCheckbox->GetChecked())
			info.AdditionalNetArgs.AppendFormat(" -altdeath");
		else
			info.AdditionalNetArgs.AppendFormat(" -deathmatch");

		info.DefaultNetGameMode = 1;
	}
	else if (TeamDeathmatchCheckbox->GetChecked())
	{
		if (AltDeathmatchCheckbox->GetChecked())
			info.AdditionalNetArgs.AppendFormat(" -altdeath");
		else
			info.AdditionalNetArgs.AppendFormat(" -deathmatch");
		info.AdditionalNetArgs.AppendFormat(" +teamplay 1");
		info.DefaultNetGameMode = 2;

		int team = 255;
		if (!TeamEdit->GetText().empty())
		{
			team = TeamEdit->GetTextInt();
			if (team < 0 || team > 255)
				team = 255;
		}
		info.AdditionalNetArgs.AppendFormat(" +team %d", team);
		info.DefaultNetHostTeam = team;
	}
	else
	{
		info.DefaultNetGameMode = -1;
	}
}

void HostSubPage::UpdateLanguage()
{
	NetModesLabel->SetText("Networking Mode:");
	AutoNetmodeCheckbox->SetText("Auto (recommended)");
	PacketServerCheckbox->SetText("Packet-Server");
	PeerToPeerCheckbox->SetText("Peer-to-Peer");

	TicDupLabel->SetText("Packet Ticrate:");
	ExtraTicCheckbox->SetText("Send Backup Packets");

	GameModesLabel->SetText("Game Modes:");
	CoopCheckbox->SetText("Co-op");
	DeathmatchCheckbox->SetText("Deathmatch");
	AltDeathmatchCheckbox->SetText("Use Alternate Deathmatch Rules");
	TeamDeathmatchCheckbox->SetText("Team Deathmatch");
	TeamLabel->SetText("Team:");

	MaxPlayersLabel->SetText("Max Players:");
	PortLabel->SetText("Host Port:");

	MaxPlayerHintLabel->SetText("Max 64");
	PortHintLabel->SetText("Default 5029");
	TeamHintLabel->SetText("Max 255");
}

void HostSubPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	constexpr double YPadding = 5.0;
	constexpr double LabelOfsSize = 90.0;

	double y = YPadding;

	MaxPlayersLabel->SetFrameGeometry(0.0, y, LabelOfsSize, MaxPlayersLabel->GetPreferredHeight());
	MaxPlayersEdit->SetFrameGeometry(MaxPlayersLabel->GetWidth(), y, 30.0, MaxPlayersLabel->GetPreferredHeight() + 2.0);
	y += MaxPlayersLabel->GetPreferredHeight() + 4.0;

	PortLabel->SetFrameGeometry(0.0, y, LabelOfsSize, PortLabel->GetPreferredHeight());
	PortEdit->SetFrameGeometry(PortLabel->GetWidth(), y, 60.0, PortLabel->GetPreferredHeight() + 2.0);

	const double hintOfs = PortLabel->GetWidth() + PortEdit->GetWidth() + 30.0;
	MaxPlayerHintLabel->SetFrameGeometry(hintOfs, YPadding, w - hintOfs, MaxPlayerHintLabel->GetPreferredHeight());
	PortHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, PortHintLabel->GetPreferredHeight());

	y += PortLabel->GetPreferredHeight() + 2.0 + YPadding;

	const double optionsTop = y;
	TicDupLabel->SetFrameGeometry(0.0, y, 100.0, TicDupLabel->GetPreferredHeight());
	y += TicDupLabel->GetPreferredHeight();
	TicDupList->SetFrameGeometry(0.0, y, 100.0, TicDupLabel->GetPreferredHeight() * (TicDupList->GetItemAmount() + 1));
	y += TicDupList->GetHeight() + ExtraTicCheckbox->GetPreferredHeight() + 2.0;

	ExtraTicCheckbox->SetFrameGeometry(0.0, y, w, ExtraTicCheckbox->GetPreferredHeight());
	y += ExtraTicCheckbox->GetPreferredHeight();

	const double optionsBottom = y;
	y = optionsTop;

	constexpr double NetModeXOfs = 115.0;
	NetModesLabel->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, NetModesLabel->GetPreferredHeight());
	y += NetModesLabel->GetPreferredHeight();

	AutoNetmodeCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, AutoNetmodeCheckbox->GetPreferredHeight());
	y += AutoNetmodeCheckbox->GetPreferredHeight();

	PacketServerCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, PacketServerCheckbox->GetPreferredHeight());
	y += PacketServerCheckbox->GetPreferredHeight();

	PeerToPeerCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, PeerToPeerCheckbox->GetPreferredHeight());

	y = max<int>(optionsBottom, y) + YPadding;
	GameModesLabel->SetFrameGeometry(0.0, y, w, GameModesLabel->GetPreferredHeight());
	y += GameModesLabel->GetPreferredHeight();

	CoopCheckbox->SetFrameGeometry(0.0, y, w, CoopCheckbox->GetPreferredHeight());
	y += CoopCheckbox->GetPreferredHeight();

	DeathmatchCheckbox->SetFrameGeometry(0.0, y, w, DeathmatchCheckbox->GetPreferredHeight());
	y += DeathmatchCheckbox->GetPreferredHeight();

	TeamDeathmatchCheckbox->SetFrameGeometry(0.0, y, w, TeamDeathmatchCheckbox->GetPreferredHeight());
	y += TeamDeathmatchCheckbox->GetPreferredHeight() + 2.0;

	TeamLabel->SetFrameGeometry(14.0, y, 45.0, TeamLabel->GetPreferredHeight());
	TeamEdit->SetFrameGeometry(TeamLabel->GetWidth() + 19.0, y, 45.0, TeamLabel->GetPreferredHeight() + 2.0);
	TeamHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, TeamHintLabel->GetPreferredHeight());
	y += TeamLabel->GetPreferredHeight() + 4.0;

	AltDeathmatchCheckbox->SetFrameGeometry(0.0, y, w, AltDeathmatchCheckbox->GetPreferredHeight());

	MainTab->UpdatePlayButton();
}

JoinSubPage::JoinSubPage(NetworkPage* main, FStartupSelectionInfo& info) : Widget(nullptr), MainTab(main)
{
	AddressEdit = new LineEdit(this);
	AddressPortEdit = new LineEdit(this);
	AddressLabel = new TextLabel(this);
	AddressPortLabel = new TextLabel(this);

	AddressEdit->SetText(info.DefaultNetAddress.GetChars());
	AddressPortEdit->SetMaxLength(5);
	AddressPortEdit->SetNumericMode(true);
	if (info.DefaultNetJoinPort > 0)
		AddressPortEdit->SetTextInt(info.DefaultNetJoinPort);

	TeamDeathmatchLabel = new TextLabel(this);
	TeamLabel = new TextLabel(this);
	TeamEdit = new LineEdit(this);

	TeamEdit->SetMaxLength(3);
	TeamEdit->SetNumericMode(true);
	TeamEdit->SetTextInt(info.DefaultNetJoinTeam);

	AddressPortHintLabel = new TextLabel(this);
	TeamHintLabel = new TextLabel(this);

	AddressPortHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
	TeamHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
}

void JoinSubPage::SetValues(FStartupSelectionInfo& info) const
{
	FString addr = AddressEdit->GetText();
	info.DefaultNetAddress = addr;
	const int port = clamp<int>(AddressPortEdit->GetTextInt(), 0, UINT16_MAX);
	if (port > 0)
	{
		addr.AppendFormat(":%d", port);
		info.DefaultNetJoinPort = port;
	}
	else
	{
		info.DefaultNetJoinPort = 0;
	}

	info.AdditionalNetArgs = "";
	info.AdditionalNetArgs.AppendFormat(" -join %s", addr);

	int team = 255;
	if (!TeamEdit->GetText().empty())
	{
		team = TeamEdit->GetTextInt();
		if (team < 0 || team > 255)
			team = 255;
	}
	info.AdditionalNetArgs.AppendFormat(" +team %d", team);
	info.DefaultNetJoinTeam = team;
}

void JoinSubPage::UpdateLanguage()
{
	AddressLabel->SetText("Host IP:");
	AddressPortLabel->SetText("Host Port:");

	TeamDeathmatchLabel->SetText("Team Deathmatch:");
	TeamLabel->SetText("Team:");

	AddressPortHintLabel->SetText("Default 5029");
	TeamHintLabel->SetText("Max 255");
}

void JoinSubPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	constexpr double YPadding = 5.0;
	constexpr double LabelOfsSize = 70.0;

	double y = YPadding;

	AddressLabel->SetFrameGeometry(0.0, y, LabelOfsSize, AddressLabel->GetPreferredHeight());
	AddressEdit->SetFrameGeometry(AddressLabel->GetWidth(), y, 120.0, AddressLabel->GetPreferredHeight() + 2.0);
	y += AddressLabel->GetPreferredHeight() + 4.0;

	AddressPortLabel->SetFrameGeometry(0.0, y, LabelOfsSize, AddressPortLabel->GetPreferredHeight());
	AddressPortEdit->SetFrameGeometry(AddressPortLabel->GetWidth(), y, 60.0, AddressPortLabel->GetPreferredHeight() + 2.0);

	const double hintOfs = AddressPortLabel->GetWidth() + AddressPortEdit->GetWidth() + 30.0;
	AddressPortHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, AddressPortHintLabel->GetPreferredHeight());
	y += AddressLabel->GetPreferredHeight() + 2.0 + YPadding;

	TeamDeathmatchLabel->SetFrameGeometry(0.0, y, w, TeamDeathmatchLabel->GetPreferredHeight());
	y += TeamLabel->GetPreferredHeight() + 2.0;

	TeamLabel->SetFrameGeometry(0.0, y, 45.0, TeamLabel->GetPreferredHeight());
	TeamEdit->SetFrameGeometry(TeamLabel->GetWidth(), y, 45.0, TeamLabel->GetPreferredHeight() + 2.0);
	TeamHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, TeamHintLabel->GetPreferredHeight());

	MainTab->UpdatePlayButton();
}
