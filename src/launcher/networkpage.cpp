
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

EXTERN_CVAR(Bool, net_ticbalance)
EXTERN_CVAR(Bool, net_extratic)

NetworkPage::NetworkPage(LauncherWindow* launcher, WadStuff* wads, int numwads, int defNetIWAD) : Widget(launcher), Launcher(launcher)
{
	ParametersEdit = new LineEdit(this);
	ParametersLabel = new TextLabel(this);
	SaveFileEdit = new LineEdit(this);
	SaveFileLabel = new TextLabel(this);
	IWADsList = new ListView(this);

	StartPages = new TabWidget(this);
	HostPage = new HostSubPage(this);
	JoinPage = new JoinSubPage(this);

	StartPages->AddTab(HostPage, "Host");
	StartPages->AddTab(JoinPage, "Join");

	StartPages->SetCurrentWidget(HostPage);

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

	if (defNetIWAD >= 0 && defNetIWAD < numwads)
	{
		IWADsList->SetSelectedItem(defNetIWAD);
		IWADsList->ScrollToItem(defNetIWAD);
	}
}

void NetworkPage::Save()
{
	if (!hosting && !joining)
		return;

	FString args = ParametersEdit->GetText();
	args.AppendFormat("\\/"); // Make sure these don't get saved into the cvar.
	if (hosting)
		HostPage->BuildCommand(args);
	else
		JoinPage->BuildCommand(args);

	const auto save = SaveFileEdit->GetText();
	if (!save.empty())
		args.AppendFormat(" -loadgame %s", save.c_str());

	ParametersEdit->SetText(args.GetChars());
}

void NetworkPage::StartGame(bool host)
{
	if (host)
	{
		hosting = true;
		joining = false;
	}
	else
	{
		hosting = false;
		joining = true;
	}

	Launcher->Start();
}

bool NetworkPage::IsStarting() const
{
	return hosting || joining;
}

int NetworkPage::GetSelectedGame() const
{
	return IWADsList->GetSelectedItem();
}

std::string NetworkPage::GetExtraArgs() const
{
	return ParametersEdit->GetText();
}

void NetworkPage::SetExtraArgs(const std::string& args)
{
	ParametersEdit->SetText(args);
}

void NetworkPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	const double wSize = w * 0.45 - 2.5;
	double y = h - (ParametersLabel->GetPreferredHeight() + 2.0);

	ParametersEdit->SetFrameGeometry(0.0, y, wSize, ParametersLabel->GetPreferredHeight() + 2.0);
	y -= ParametersLabel->GetPreferredHeight();
	ParametersLabel->SetFrameGeometry(0.0, y, wSize, ParametersLabel->GetPreferredHeight());

	y -= SaveFileLabel->GetPreferredHeight() + 7.0;
	SaveFileEdit->SetFrameGeometry(0.0, y, wSize, SaveFileLabel->GetPreferredHeight() + 2.0);
	y -= SaveFileLabel->GetPreferredHeight();
	SaveFileLabel->SetFrameGeometry(0.0, y, wSize, SaveFileLabel->GetPreferredHeight());
	y -= 5.0;

	IWADsList->SetFrameGeometry(0.0, 0.0, wSize, y);

	const double xOfs = w * 0.45 + 2.5;
	StartPages->SetFrameGeometry(xOfs, 0.0, w - xOfs, h);
}

void NetworkPage::UpdateLanguage()
{
	ParametersLabel->SetText(GStrings.GetString("PICKER_ADDPARM"));
	SaveFileLabel->SetText("Load Save File:");

	StartPages->SetTabText(HostPage, "Host");
	StartPages->SetTabText(JoinPage, "Join");
	HostPage->UpdateLanguage();
	JoinPage->UpdateLanguage();
}

HostSubPage::HostSubPage(NetworkPage* main) : Widget(nullptr), MainTab(main)
{
	HostButton = new PushButton(this);
	HostButton->OnClick = [=]() { OnHostButtonClicked(); };

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
	AutoNetmodeCheckbox->SetChecked(true);

	TicDupList = new ListView(this);
	TicDupLabel = new TextLabel(this);
	ExtraTicCheckbox = new CheckboxLabel(this);
	BalanceTicsCheckbox = new CheckboxLabel(this);

	std::vector<double> widths = { 35.0, 35.0 };
	TicDupList->SetColumnWidths(widths);
	TicDupList->AddItem("35.0");
	TicDupList->UpdateItem("Hz", 0, 1);
	TicDupList->AddItem("17.5");
	TicDupList->UpdateItem("Hz", 1, 1);
	TicDupList->AddItem("11.6");
	TicDupList->UpdateItem("Hz", 2, 1);
	TicDupList->SetSelectedItem(0);
	ExtraTicCheckbox->SetChecked(net_extratic);
	BalanceTicsCheckbox->SetChecked(net_ticbalance);

	GameModesLabel = new TextLabel(this);
	CoopCheckbox = new CheckboxLabel(this);
	DeathmatchCheckbox = new CheckboxLabel(this);
	TeamDeathmatchCheckbox = new CheckboxLabel(this);
	TeamLabel = new TextLabel(this);
	TeamEdit = new LineEdit(this);

	TeamEdit->SetMaxLength(3);
	TeamEdit->SetNumericMode(true);

	// These are intentionally not radio buttons, they just act similarly for clarity in the UI.
	CoopCheckbox->FuncChanged = [this](bool on) { if (on) { DeathmatchCheckbox->SetChecked(false); TeamDeathmatchCheckbox->SetChecked(false); }};
	DeathmatchCheckbox->FuncChanged = [this](bool on) { if (on) { CoopCheckbox->SetChecked(false); TeamDeathmatchCheckbox->SetChecked(false); }};
	TeamDeathmatchCheckbox->FuncChanged = [this](bool on) { if (on) { CoopCheckbox->SetChecked(false); DeathmatchCheckbox->SetChecked(false); }};

	MaxPlayersEdit = new LineEdit(this);
	PortEdit = new LineEdit(this);
	MaxPlayersLabel = new TextLabel(this);
	PortLabel = new TextLabel(this);

	MaxPlayersEdit->SetMaxLength(2);
	MaxPlayersEdit->SetNumericMode(true);
	MaxPlayersEdit->SetText("8");
	PortEdit->SetMaxLength(5);
	PortEdit->SetNumericMode(true);

	MaxPlayerHintLabel = new TextLabel(this);
	PortHintLabel = new TextLabel(this);
	TeamHintLabel = new TextLabel(this);
}

void HostSubPage::OnHostButtonClicked()
{
	MainTab->StartGame(true);
}

void HostSubPage::BuildCommand(FString& args)
{
	if (PacketServerCheckbox->GetChecked())
		args.AppendFormat(" -netmode 1");
	else if (PeerToPeerCheckbox->GetChecked())
		args.AppendFormat(" -netmode 0");

	net_extratic = ExtraTicCheckbox->GetChecked();
	net_ticbalance = BalanceTicsCheckbox->GetChecked();
	const int dup = TicDupList->GetSelectedItem();
	if (dup > 0)
		args.AppendFormat(" -dup %d", dup + 1);

	args.AppendFormat(" -host %d", clamp<int>(MaxPlayersEdit->GetTextInt(), 1, MAXPLAYERS));
	const int port = PortEdit->GetTextInt();
	if (port > 0)
		args.AppendFormat(" -port %d", port);

	if (CoopCheckbox->GetChecked())
	{
		args.AppendFormat(" -coop");
	}
	else if (DeathmatchCheckbox->GetChecked())
	{
		args.AppendFormat(" -deathmatch");
	}
	else if (TeamDeathmatchCheckbox->GetChecked())
	{
		args.AppendFormat(" -deathmatch +teamplay 1");
		int team = 255;
		if (!TeamEdit->GetText().empty())
		{
			team = TeamEdit->GetTextInt();
			if (team < 0 || team > 255)
				team = 255;
		}
		args.AppendFormat(" +team %d", team);
	}
}

void HostSubPage::UpdateLanguage()
{
	HostButton->SetText("Host Game");

	NetModesLabel->SetText("Networking Mode:");
	AutoNetmodeCheckbox->SetText("Auto (recommended)");
	PacketServerCheckbox->SetText("Packet-Server");
	PeerToPeerCheckbox->SetText("Peer-to-Peer");

	TicDupLabel->SetText("Packet Ticrate:");
	ExtraTicCheckbox->SetText("Double Send Packets");
	BalanceTicsCheckbox->SetText("Stabilize Connections (recommended)");

	GameModesLabel->SetText("Game Modes:");
	CoopCheckbox->SetText("Co-op");
	DeathmatchCheckbox->SetText("Deathmatch");
	TeamDeathmatchCheckbox->SetText("Team Deathmatch");
	TeamLabel->SetText("Team:");

	MaxPlayersLabel->SetText("Max Players:");
	PortLabel->SetText("Host Port:");

	MaxPlayerHintLabel->SetText("Max 64");
	PortHintLabel->SetText("Default 5029");
	TeamHintLabel->SetText("Max 254");
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
	y += MaxPlayersLabel->GetPreferredHeight() + YPadding;

	PortLabel->SetFrameGeometry(0.0, y, LabelOfsSize, PortLabel->GetPreferredHeight());
	PortEdit->SetFrameGeometry(PortLabel->GetWidth(), y, 60.0, PortLabel->GetPreferredHeight() + 2.0);

	const double hintOfs = PortLabel->GetWidth() + PortEdit->GetWidth() + 30.0;
	MaxPlayerHintLabel->SetFrameGeometry(hintOfs, YPadding, w - hintOfs, MaxPlayerHintLabel->GetPreferredHeight());
	PortHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, PortHintLabel->GetPreferredHeight());

	y += PortLabel->GetPreferredHeight() + YPadding;

	const double optionsTop = y;
	TicDupLabel->SetFrameGeometry(0.0, y, 100.0, TicDupLabel->GetPreferredHeight());
	y += TicDupLabel->GetPreferredHeight();
	TicDupList->SetFrameGeometry(0.0, y, 100.0, TicDupLabel->GetPreferredHeight() * (TicDupList->GetItemAmount() + 1));
	y += TicDupList->GetHeight() + ExtraTicCheckbox->GetPreferredHeight();

	ExtraTicCheckbox->SetFrameGeometry(0.0, y, w, ExtraTicCheckbox->GetPreferredHeight());
	y += ExtraTicCheckbox->GetPreferredHeight();

	BalanceTicsCheckbox->SetFrameGeometry(0.0, y, w, BalanceTicsCheckbox->GetPreferredHeight());
	y += BalanceTicsCheckbox->GetPreferredHeight() + YPadding;

	const double optionsBottom = y;
	y = optionsTop;

	constexpr double NetModeXOfs = 110.0;
	NetModesLabel->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, NetModesLabel->GetPreferredHeight());
	y += NetModesLabel->GetPreferredHeight();

	AutoNetmodeCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, AutoNetmodeCheckbox->GetPreferredHeight());
	y += AutoNetmodeCheckbox->GetPreferredHeight();

	PacketServerCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, PacketServerCheckbox->GetPreferredHeight());
	y += PacketServerCheckbox->GetPreferredHeight();

	PeerToPeerCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, PeerToPeerCheckbox->GetPreferredHeight());

	y = max<int>(optionsBottom, y) + 5.0;
	GameModesLabel->SetFrameGeometry(0.0, y, w, GameModesLabel->GetPreferredHeight());
	y += GameModesLabel->GetPreferredHeight();

	CoopCheckbox->SetFrameGeometry(0.0, y, w, CoopCheckbox->GetPreferredHeight());
	y += CoopCheckbox->GetPreferredHeight();

	DeathmatchCheckbox->SetFrameGeometry(0.0, y, w, DeathmatchCheckbox->GetPreferredHeight());
	y += DeathmatchCheckbox->GetPreferredHeight();

	TeamDeathmatchCheckbox->SetFrameGeometry(0.0, y, 140.0, TeamDeathmatchCheckbox->GetPreferredHeight());
	TeamLabel->SetFrameGeometry(TeamDeathmatchCheckbox->GetWidth() + 5.0, y, 45.0, TeamLabel->GetPreferredHeight());
	TeamEdit->SetFrameGeometry(TeamDeathmatchCheckbox->GetWidth() + TeamLabel->GetWidth() + 5.0, y, 45.0, TeamLabel->GetPreferredHeight() + 2.0);
	y += TeamLabel->GetPreferredHeight() + 2.0;

	TeamHintLabel->SetFrameGeometry(TeamDeathmatchCheckbox->GetWidth() + TeamLabel->GetWidth() + 5.0, y, w, TeamHintLabel->GetPreferredHeight());

	HostButton->SetFrameGeometry(0.0, h - 30.0, 100.0, 30.0);
}

JoinSubPage::JoinSubPage(NetworkPage* main) : Widget(nullptr), MainTab(main)
{
	JoinButton = new PushButton(this);
	JoinButton->OnClick = [=]() { OnJoinButtonClicked(); };

	AddressEdit = new LineEdit(this);
	AddressPortEdit = new LineEdit(this);
	AddressLabel = new TextLabel(this);
	AddressPortLabel = new TextLabel(this);

	AddressPortEdit->SetMaxLength(5);
	AddressPortEdit->SetNumericMode(true);

	TeamDeathmatchLabel = new TextLabel(this);
	TeamLabel = new TextLabel(this);
	TeamEdit = new LineEdit(this);

	TeamEdit->SetMaxLength(3);
	TeamEdit->SetNumericMode(true);

	PortHintLabel = new TextLabel(this);
	TeamHintLabel = new TextLabel(this);
}

void JoinSubPage::OnJoinButtonClicked()
{
	MainTab->StartGame(false);
}

void JoinSubPage::BuildCommand(FString& args)
{
	FString addr = AddressEdit->GetText();
	const int port = AddressPortEdit->GetTextInt();
	if (port > 0)
		addr.AppendFormat(":%d", port);

	args.AppendFormat(" -join %s", addr.GetChars());

	int team = 255;
	if (!TeamEdit->GetText().empty())
	{
		team = TeamEdit->GetTextInt();
		if (team < 0 || team > 255)
			team = 255;
	}
	args.AppendFormat(" +team %d", team);
}

void JoinSubPage::UpdateLanguage()
{
	JoinButton->SetText("Join Game");
	AddressLabel->SetText("Host IP:");
	AddressPortLabel->SetText("Host Port:");

	TeamDeathmatchLabel->SetText("Team Deathmatch:");
	TeamLabel->SetText("Team:");

	PortHintLabel->SetText("Default 5029");
	TeamHintLabel->SetText("Max 254");
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
	y += AddressLabel->GetPreferredHeight() + YPadding;

	AddressPortLabel->SetFrameGeometry(0.0, y, LabelOfsSize, AddressPortLabel->GetPreferredHeight());
	AddressPortEdit->SetFrameGeometry(AddressPortLabel->GetWidth(), y, 60.0, AddressPortLabel->GetPreferredHeight() + 2.0);

	const double hintOfs = AddressPortLabel->GetWidth() + AddressPortEdit->GetWidth() + 30.0;
	PortHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, PortHintLabel->GetPreferredHeight());
	y += AddressLabel->GetPreferredHeight() + 17.0;

	TeamDeathmatchLabel->SetFrameGeometry(0.0, y, w, TeamDeathmatchLabel->GetPreferredHeight());
	y += TeamLabel->GetPreferredHeight() + 1.0;

	TeamLabel->SetFrameGeometry(0.0, y, 45.0, TeamLabel->GetPreferredHeight());
	TeamEdit->SetFrameGeometry(TeamLabel->GetWidth(), y, 45.0, TeamLabel->GetPreferredHeight() + 2.0);
	TeamHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, TeamHintLabel->GetPreferredHeight());

	JoinButton->SetFrameGeometry(0.0, h - 30.0, 100.0, 30.0);
}
