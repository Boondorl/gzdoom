
#include "netstartwindow.h"
#include "version.h"
#include "engineerrors.h"
#include "gstrings.h"
#include <zwidget/core/timer.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>

NetStartWindow* NetStartWindow::Instance = nullptr;

void NetStartWindow::NetInit(const char* message)
{
	Size screenSize = GetScreenSize();
	double windowWidth = 300.0;
	double windowHeight = 150.0;

	if (!Instance)
	{
		Instance = new NetStartWindow();
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
	
}

void NetStartWindow::NetUpdate(int client, int status)
{

}

void NetStartWindow::NetDisconnect(int client)
{

}

void NetStartWindow::NetProgress(int cur, int limit)
{
	if (Instance)
	{
		Instance->maxpos = limit;
		Instance->SetProgress(cur);
	}
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

NetStartWindow::NetStartWindow() : Widget(nullptr, WidgetType::Window)
{
	SetWindowBackground(Colorf::fromRgba8(51, 51, 51));
	SetWindowBorderColor(Colorf::fromRgba8(51, 51, 51));
	SetWindowCaptionColor(Colorf::fromRgba8(33, 33, 33));
	SetWindowCaptionTextColor(Colorf::fromRgba8(226, 223, 219));
	SetWindowTitle(GAMENAME);

	MessageLabel = new TextLabel(this);
	ProgressLabel = new TextLabel(this);
	AbortButton = new PushButton(this);
	ForceStartButton = new PushButton(this);

	MessageLabel->SetTextAlignment(TextLabelAlignment::Center);
	ProgressLabel->SetTextAlignment(TextLabelAlignment::Center);

	AbortButton->OnClick = [=]() { OnClose(); };
	ForceStartButton->OnClick = [=]() { ForceStart(); };

	AbortButton->SetText("Abort");
	ForceStartButton->SetText("Start Game");

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
	y += labelheight;

	y = GetHeight() - 15.0 - AbortButton->GetPreferredHeight();
	AbortButton->SetFrameGeometry((w + 10.0) * 0.5, y, 100.0, AbortButton->GetPreferredHeight());
	ForceStartButton->SetFrameGeometry((w - 210.0) * 0.5, y, 100.0, ForceStartButton->GetPreferredHeight());
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
