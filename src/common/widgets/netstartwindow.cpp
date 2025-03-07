
#include "netstartwindow.h"
#include "version.h"
#include "engineerrors.h"
#include "gstrings.h"
#include <zwidget/core/timer.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>

NetStartWindow* NetStartWindow::Instance = nullptr;

void NetStartWindow::NetInit(const char* message, bool host)
{
	Size screenSize = GetScreenSize();
	double windowWidth = 450.0;
	double windowHeight = 600.0;

	if (!Instance)
	{
		Instance = new NetStartWindow(host);
		Instance->SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);
		Instance->Show();
	}

	Instance->SetMessage(message);
}

void NetStartWindow::NetMessage(const char* message)
{
	if (Instance)
		Instance->SetMessage(message);
}

void NetStartWindow::NetConnect(int client, const char* name, unsigned flags, int status)
{
	if (!Instance)
		return;

	std::string value = "";
	if (flags & 1)
		value.append("*");
	if (flags & 2)
		value.append("H");

	Instance->LobbyWindow->UpdateItem(value, client, 1);
	Instance->LobbyWindow->UpdateItem(name, client, 2);
	
	value = "";
	if (status == 1)
		value = "CONNECTING";
	else if (status == 2)
		value = "WAITING";
	else if (status == 3)
		value = "READY";

	Instance->LobbyWindow->UpdateItem(value, client, 3);
}

void NetStartWindow::NetUpdate(int client, int status)
{
	if (!Instance)
		return;

	std::string value = "";
	if (status == 1)
		value = "CONNECTING";
	else if (status == 2)
		value = "WAITING";
	else if (status == 3)
		value = "READY";

	Instance->LobbyWindow->UpdateItem(value, client, 3);
}

void NetStartWindow::NetDisconnect(int client)
{
	if (Instance)
	{
		for (size_t i = 1u; i < Instance->LobbyWindow->GetColumnAmount(); ++i)
			Instance->LobbyWindow->UpdateItem("", client, i);
	}
}

void NetStartWindow::NetProgress(int cur, int limit)
{
	if (!Instance)
		return;

	Instance->maxpos = limit;
	Instance->SetProgress(cur);
	for (size_t start = Instance->LobbyWindow->GetItemAmount(); start < Instance->maxpos; ++start)
		Instance->LobbyWindow->AddItem(std::to_string(start));
}

void NetStartWindow::NetDone()
{
	delete Instance;
	Instance = nullptr;
}

void NetStartWindow::NetClose()
{
	if (Instance != nullptr)
		Instance->OnClose();
}

bool NetStartWindow::ShouldStartNet()
{
	if (Instance != nullptr)
		return Instance->shouldstart;

	return false;
}

bool NetStartWindow::NetLoop(bool (*loopCallback)(void*), void* data)
{
	if (!Instance)
		return false;

	Instance->timer_callback = loopCallback;
	Instance->userdata = data;
	Instance->CallbackException = {};

	DisplayWindow::RunLoop();

	Instance->timer_callback = nullptr;
	Instance->userdata = nullptr;

	if (Instance->CallbackException)
		std::rethrow_exception(Instance->CallbackException);

	return Instance->exitreason;
}

NetStartWindow::NetStartWindow(bool host) : Widget(nullptr, WidgetType::Window)
{
	SetWindowBackground(Colorf::fromRgba8(51, 51, 51));
	SetWindowBorderColor(Colorf::fromRgba8(51, 51, 51));
	SetWindowCaptionColor(Colorf::fromRgba8(33, 33, 33));
	SetWindowCaptionTextColor(Colorf::fromRgba8(226, 223, 219));
	SetWindowTitle(GAMENAME);

	MessageLabel = new TextLabel(this);
	ProgressLabel = new TextLabel(this);
	LobbyWindow = new ListView(this);
	AbortButton = new PushButton(this);

	MessageLabel->SetTextAlignment(TextLabelAlignment::Center);
	ProgressLabel->SetTextAlignment(TextLabelAlignment::Center);

	AbortButton->OnClick = [=]() { OnClose(); };
	AbortButton->SetText("Abort");

	if (host)
	{
		ForceStartButton = new PushButton(this);
		ForceStartButton->OnClick = [=]() { ForceStart(); };
		ForceStartButton->SetText("Start Game");
	}

	// Client number, flags, name, status.
	LobbyWindow->SetColumnWidths({ 30.0, 30.0, 200.0, 50.0 });

	CallbackTimer = new Timer(this);
	CallbackTimer->FuncExpired = [=]() { OnCallbackTimerExpired(); };
	CallbackTimer->Start(500);
}

void NetStartWindow::SetMessage(const std::string& message)
{
	MessageLabel->SetText(message);
}

void NetStartWindow::SetProgress(int newpos)
{
	if (pos != newpos && maxpos > 1)
	{
		pos = newpos;
		FString message;
		message.Format("%d/%d", pos, maxpos);
		ProgressLabel->SetText(message.GetChars());
	}
}

void NetStartWindow::OnClose()
{
	exitreason = false;
	DisplayWindow::ExitLoop();
}

void NetStartWindow::ForceStart()
{
	shouldstart = true;
}

void NetStartWindow::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();

	double y = 15.0;
	double labelheight = MessageLabel->GetPreferredHeight();
	MessageLabel->SetFrameGeometry(Rect::xywh(5.0, y, w - 10.0, labelheight));
	y += labelheight;

	labelheight = ProgressLabel->GetPreferredHeight();
	ProgressLabel->SetFrameGeometry(Rect::xywh(5.0, y, w - 10.0, labelheight));
	y += labelheight + 5.0;

	labelheight = (GetHeight() - 30.0 - AbortButton->GetPreferredHeight()) - y;
	LobbyWindow->SetFrameGeometry(Rect::xywh(5.0, y, w - 10.0, labelheight));

	y = GetHeight() - 15.0 - AbortButton->GetPreferredHeight();
	if (ForceStartButton != nullptr)
	{
		AbortButton->SetFrameGeometry((w + 10.0) * 0.5, y, 100.0, AbortButton->GetPreferredHeight());
		ForceStartButton->SetFrameGeometry((w - 210.0) * 0.5, y, 100.0, ForceStartButton->GetPreferredHeight());
	}
	else
	{
		AbortButton->SetFrameGeometry((w - 100.0) * 0.5, y, 100.0, AbortButton->GetPreferredHeight());
	}
}

void NetStartWindow::OnCallbackTimerExpired()
{
	if (timer_callback)
	{
		bool result = false;
		try
		{
			result = timer_callback(userdata);
		}
		catch (...)
		{
			CallbackException = std::current_exception();
		}

		if (result)
		{
			exitreason = true;
			DisplayWindow::ExitLoop();
		}
	}
}
