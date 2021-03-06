/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "App.h"
#include "GSFrame.h"
#include "AppAccelerators.h"
#include "AppSaveStates.h"

#include "GS.h"
#include "MSWstuff.h"

#include <wx/utils.h>
#include <memory>

static const KeyAcceleratorCode FULLSCREEN_TOGGLE_ACCELERATOR_GSPANEL=KeyAcceleratorCode( WXK_RETURN ).Alt();

//#define GSWindowScaleDebug

void GSPanel::InitDefaultAccelerators()
{
	// Note: these override GlobalAccels ( Pcsx2App::InitDefaultGlobalAccelerators() )
	// For plain letters or symbols, replace e.g. WXK_F1 with e.g. wxKeyCode('q') or wxKeyCode('-')
	// For plain letter keys with shift, use e.g. AAC( wxKeyCode('q') ).Shift() and NOT wxKeyCode('Q')
	// For a symbol with shift (e.g. '_' which is '-' with shift) use AAC( wxKeyCode('-') ).Shift()

	typedef KeyAcceleratorCode AAC;

	if (!m_Accels) m_Accels = std::unique_ptr<AcceleratorDictionary>(new AcceleratorDictionary);

	m_Accels->Map( AAC( WXK_F1 ),				"States_FreezeCurrentSlot" );
	m_Accels->Map( AAC( WXK_F3 ),				"States_DefrostCurrentSlot");
	m_Accels->Map( AAC( WXK_F3 ).Shift(),		"States_DefrostCurrentSlotBackup");
	m_Accels->Map( AAC( WXK_F2 ),				"States_CycleSlotForward" );
	m_Accels->Map( AAC( WXK_F2 ).Shift(),		"States_CycleSlotBackward" );

	m_Accels->Map( AAC( WXK_F4 ),				"Framelimiter_MasterToggle");
	m_Accels->Map( AAC( WXK_F4 ).Shift(),		"Frameskip_Toggle");
	m_Accels->Map( AAC( WXK_TAB ),				"Framelimiter_TurboToggle" );
	m_Accels->Map( AAC( WXK_TAB ).Shift(),		"Framelimiter_SlomoToggle" );

	m_Accels->Map( AAC( WXK_F6 ),				"GSwindow_CycleAspectRatio" );

	m_Accels->Map( AAC( WXK_NUMPAD_ADD ).Cmd(),			"GSwindow_ZoomIn" );	//CTRL on Windows/linux, CMD on OSX
	m_Accels->Map( AAC( WXK_NUMPAD_SUBTRACT ).Cmd(),	"GSwindow_ZoomOut" );
	m_Accels->Map( AAC( WXK_NUMPAD_MULTIPLY ).Cmd(),	"GSwindow_ZoomToggle" );

	m_Accels->Map( AAC( WXK_NUMPAD_ADD ).Cmd().Alt(),			"GSwindow_ZoomInY" );	//CTRL on Windows/linux, CMD on OSX
	m_Accels->Map( AAC( WXK_NUMPAD_SUBTRACT ).Cmd().Alt(),	"GSwindow_ZoomOutY" );
	m_Accels->Map( AAC( WXK_NUMPAD_MULTIPLY ).Cmd().Alt(),	"GSwindow_ZoomResetY" );

	m_Accels->Map( AAC( WXK_UP ).Cmd().Alt(),	"GSwindow_OffsetYminus" );
	m_Accels->Map( AAC( WXK_DOWN ).Cmd().Alt(),	"GSwindow_OffsetYplus" );
	m_Accels->Map( AAC( WXK_LEFT ).Cmd().Alt(),	"GSwindow_OffsetXminus" );
	m_Accels->Map( AAC( WXK_RIGHT ).Cmd().Alt(),	"GSwindow_OffsetXplus" );
	m_Accels->Map( AAC( WXK_NUMPAD_DIVIDE ).Cmd().Alt(),	"GSwindow_OffsetReset" );

	m_Accels->Map( AAC( WXK_ESCAPE ),			"Sys_SuspendResume" );
	m_Accels->Map( AAC( WXK_F8 ),				"Sys_TakeSnapshot" ); // also shift and ctrl-shift will be added automatically
	m_Accels->Map( AAC( WXK_F9 ),				"Sys_RenderswitchToggle");

	m_Accels->Map( AAC( WXK_F10 ),				"Sys_LoggingToggle" );
	m_Accels->Map( AAC( WXK_F11 ),				"Sys_FreezeGS" );
	m_Accels->Map( AAC( WXK_F12 ),				"Sys_RecordingToggle" );

	m_Accels->Map( FULLSCREEN_TOGGLE_ACCELERATOR_GSPANEL,		"FullscreenToggle" );
}

GSPanel::GSPanel( wxWindow* parent )
	: wxWindow()
	, m_HideMouseTimer( this )
	, m_coreRunning(false)
{
	m_CursorShown	= true;
	m_HasFocus		= false;

	if ( !wxWindow::Create(parent, wxID_ANY) )
		throw Exception::RuntimeError().SetDiagMsg( L"GSPanel constructor explode!!" );

	SetName( L"GSPanel" );

	InitDefaultAccelerators();

	SetBackgroundColour(wxColour((unsigned long)0));
	if( g_Conf->GSWindow.AlwaysHideMouse )
	{
		SetCursor( wxCursor(wxCURSOR_BLANK) );
		m_CursorShown = false;
	}

	Bind(wxEVT_CLOSE_WINDOW, &GSPanel::OnCloseWindow, this);
	Bind(wxEVT_SIZE, &GSPanel::OnResize, this);
	Bind(wxEVT_KEY_UP, &GSPanel::OnKeyDownOrUp, this);
	Bind(wxEVT_KEY_DOWN, &GSPanel::OnKeyDownOrUp, this);

	Bind(wxEVT_SET_FOCUS, &GSPanel::OnFocus, this);
	Bind(wxEVT_KILL_FOCUS, &GSPanel::OnFocusLost, this);

	Bind(wxEVT_TIMER, &GSPanel::OnHideMouseTimeout, this, m_HideMouseTimer.GetId());

	// Any and all events which should result in the mouse cursor being made visible
	// are connected here.  If I missed one, feel free to add it in! --air
	Bind(wxEVT_LEFT_DOWN, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_LEFT_UP, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_DOWN, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_UP, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_RIGHT_DOWN, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_RIGHT_UP, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MOTION, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_LEFT_DCLICK, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_DCLICK, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_RIGHT_DCLICK, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MOUSEWHEEL, &GSPanel::OnMouseEvent, this);

	Bind(wxEVT_LEFT_DCLICK, &GSPanel::OnLeftDclick, this);
}

GSPanel::~GSPanel() throw()
{
	//CoreThread.Suspend( false );		// Just in case...!
}

void GSPanel::DoShowMouse()
{
	if( g_Conf->GSWindow.AlwaysHideMouse ) return;

	if( !m_CursorShown )
	{
		SetCursor( wxCursor( wxCURSOR_DEFAULT ) );
		m_CursorShown = true;
	}
	m_HideMouseTimer.Start( 1750, true );
}

void GSPanel::DoResize()
{
	if( GetParent() == NULL ) return;
	wxSize client = GetParent()->GetClientSize();
	wxSize viewport = client;

	if ( !client.GetHeight() || !client.GetWidth() )
		return;

	double clientAr = (double)client.GetWidth()/(double)client.GetHeight();

	extern AspectRatioType iniAR;
	extern bool switchAR;
	double targetAr = clientAr;

	if (g_Conf->GSWindow.AspectRatio != iniAR) {
		switchAR = false;
	}

	if (!switchAR) {
		if (g_Conf->GSWindow.AspectRatio == AspectRatio_4_3)
			targetAr = 4.0 / 3.0;
		else if (g_Conf->GSWindow.AspectRatio == AspectRatio_16_9)
			targetAr = 16.0 / 9.0;
	} else {
		targetAr = 4.0 / 3.0;
	}

	double arr = targetAr / clientAr;

	if( arr < 1 )
		viewport.x = (int)( (double)viewport.x*arr + 0.5);
	else if( arr > 1 )
		viewport.y = (int)( (double)viewport.y/arr + 0.5);

	float zoom = g_Conf->GSWindow.Zoom.ToFloat()/100.0;
	if( zoom == 0 )//auto zoom in untill black-bars are gone (while keeping the aspect ratio).
		zoom = std::max( (float)arr, (float)(1.0/arr) );

	viewport.Scale(zoom, zoom*g_Conf->GSWindow.StretchY.ToFloat()/100.0 );
	if (viewport == client && EmuConfig.Gamefixes.FMVinSoftwareHack && g_Conf->GSWindow.IsFullscreen)
		viewport.x += 1; //avoids crash on some systems switching HW><SW in fullscreen aspect ratio's with FMV Software switch.
	SetSize( viewport );
	CenterOnParent();
	
	int cx, cy;
	GetPosition(&cx, &cy);
	float unit = .01*(float)std::min(viewport.x, viewport.y);
	SetPosition( wxPoint( cx + unit*g_Conf->GSWindow.OffsetX.ToFloat(), cy + unit*g_Conf->GSWindow.OffsetY.ToFloat() ) );
#ifdef GSWindowScaleDebug
	Console.WriteLn(Color_Yellow, "GSWindowScaleDebug: zoom %f, viewport.x %d, viewport.y %d", zoom, viewport.GetX(), viewport.GetY());
#endif
}

void GSPanel::OnResize(wxSizeEvent& event)
{
	if( IsBeingDeleted() ) return;
	DoResize();
	//Console.Error( "Size? %d x %d", GetSize().x, GetSize().y );
	//event.
}

void GSPanel::OnCloseWindow(wxCloseEvent& evt)
{
	CoreThread.Suspend();
	evt.Skip();		// and close it.
}

void GSPanel::OnMouseEvent( wxMouseEvent& evt )
{
	if( IsBeingDeleted() ) return;

	// Do nothing for left-button event
	if (!evt.Button(wxMOUSE_BTN_LEFT)) {
		evt.Skip();
		DoShowMouse();
	}

#if defined(__unix__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad plugin. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	if( (PADWriteEvent != NULL) && (GSopen2 != NULL) ) {
		keyEvent event;
		// FIXME how to handle double click ???
		if (evt.ButtonDown()) {
			event.evt = 4; // X equivalent of ButtonPress
			event.key = evt.GetButton();
		} else if (evt.ButtonUp()) {
			event.evt = 5; // X equivalent of ButtonRelease
			event.key = evt.GetButton();
		} else if (evt.Moving() || evt.Dragging()) {
			event.evt = 6; // X equivalent of MotionNotify
			long x,y;
			evt.GetPosition(&x, &y);

			wxCoord w, h;
			wxWindowDC dc( this );
			dc.GetSize(&w, &h);

			// Special case to allow continuous mouvement near the border
			if (x < 10)
				x = 0;
			else if (x > (w-10))
				x = 0xFFFF;

			if (y < 10)
				y = 0;
			else if (y > (w-10))
				y = 0xFFFF;

			// For compatibility purpose with the existing structure. I decide to reduce
			// the position to 16 bits.
			event.key = ((y & 0xFFFF) << 16) | (x & 0xFFFF);

		} else {
			event.key = 0;
			event.evt = 0;
		}

		PADWriteEvent(event);
	}
#endif
}

void GSPanel::OnHideMouseTimeout( wxTimerEvent& evt )
{
	if( IsBeingDeleted() || !m_HasFocus ) return;
	if( CoreThread.GetExecutionMode() != SysThreadBase::ExecMode_Opened ) return;

	SetCursor( wxCursor( wxCURSOR_BLANK ) );
	m_CursorShown = false;
}

void GSPanel::OnKeyDownOrUp( wxKeyEvent& evt )
{

	// HACK: Legacy PAD plugins expect PCSX2 to ignore keyboard messages on the GS Window while
	// the PAD plugin is open, so ignore here (PCSX2 will direct messages routed from PAD directly
	// to the APP level message handler, which in turn routes them right back here -- yes it's
	// silly, but oh well).

#if defined(__unix__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad plugin. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	if( (PADWriteEvent != NULL) && (GSopen2 != NULL) ) {
		keyEvent event;
		event.key = evt.GetRawKeyCode();
		if (evt.GetEventType() == wxEVT_KEY_UP)
			event.evt = 3; // X equivalent of KEYRELEASE;
		else if (evt.GetEventType() == wxEVT_KEY_DOWN)
			event.evt = 2; // X equivalent of KEYPRESS;
		else
			event.evt = 0;

		PADWriteEvent(event);
	}
#endif

#ifdef __WXMSW__
	// Not sure what happens on Linux, but on windows this method is called only when emulation
	// is paused and the GS window is not hidden (and therefore the event doesn't arrive from
	// the pad plugin and doesn't go through Pcsx2App::PadKeyDispatch). On such case (paused).
	// It needs to handle two issues:
	// 1. It's called both for key down and key up (linux apparently needs it this way) - but we
	//    don't want to execute the command twice (normally commands execute on key down only).
	// 2. It has wx keycode which is upper case for ascii chars, but our command handlers expect
	//    lower case for non-special keys.

	// ignore key up events
	if (evt.GetEventType() == wxEVT_KEY_UP)
		return;

	// Make ascii keys lower case - this apparently works correctly also with modifiers (shift included)
	if (evt.m_keyCode >= 'A' && evt.m_keyCode <= 'Z')
		evt.m_keyCode += (int)'a' - 'A';
#endif

	if( (PADopen != NULL) && CoreThread.IsOpen() ) return;
	DirectKeyCommand( evt );
}

void GSPanel::DirectKeyCommand( const KeyAcceleratorCode& kac )
{
	const GlobalCommandDescriptor* cmd = NULL;

	std::unordered_map<int, const GlobalCommandDescriptor*>::const_iterator iter(m_Accels->find(kac.val32));
	if (iter == m_Accels->end())
		return;

	cmd = iter->second;

	DbgCon.WriteLn( "(gsFrame) Invoking command: %s", cmd->Id );
	cmd->Invoke();
	
	if( cmd->AlsoApplyToGui && !g_ConfigPanelChanged)
		AppApplySettings();
}

void GSPanel::DirectKeyCommand( wxKeyEvent& evt )
{
	DirectKeyCommand(KeyAcceleratorCode( evt ));
}

void GSPanel::UpdateScreensaver()
{
	bool prevent = g_Conf->GSWindow.DisableScreenSaver
	               && m_HasFocus && m_coreRunning;
	ScreensaverAllow(!prevent);
}

void GSPanel::OnFocus( wxFocusEvent& evt )
{
	evt.Skip();
	m_HasFocus = true;

	if( g_Conf->GSWindow.AlwaysHideMouse )
	{
		SetCursor( wxCursor(wxCURSOR_BLANK) );
		m_CursorShown = false;
	}
	else
		DoShowMouse();

#if defined(__unix__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad plugin. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	if( (PADWriteEvent != NULL) && (GSopen2 != NULL) ) {
		keyEvent event = {0, 9}; // X equivalent of FocusIn;
		PADWriteEvent(event);
	}
#endif
	//Console.Warning("GS frame > focus set");

	UpdateScreensaver();
}

void GSPanel::OnFocusLost( wxFocusEvent& evt )
{
	evt.Skip();
	m_HasFocus = false;
	DoShowMouse();
#if defined(__unix__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad plugin. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	if( (PADWriteEvent != NULL) && (GSopen2 != NULL) ) {
		keyEvent event = {0, 10}; // X equivalent of FocusOut
		PADWriteEvent(event);
	}
#endif
	//Console.Warning("GS frame > focus lost");

	UpdateScreensaver();
}

void GSPanel::CoreThread_OnResumed()
{
	m_coreRunning = true;
	UpdateScreensaver();
}

void GSPanel::CoreThread_OnSuspended()
{
	m_coreRunning = false;
	UpdateScreensaver();
}

void GSPanel::AppStatusEvent_OnSettingsApplied()
{
	if( IsBeingDeleted() ) return;
	DoResize();
	DoShowMouse();
	Show( !EmuConfig.GS.DisableOutput );
}

void GSPanel::OnLeftDclick(wxMouseEvent& evt)
{
	if( !g_Conf->GSWindow.IsToggleFullscreenOnDoubleClick )
		return;

	//Console.WriteLn("GSPanel::OnDoubleClick: Invoking Fullscreen-Toggle accelerator.");
	DirectKeyCommand(FULLSCREEN_TOGGLE_ACCELERATOR_GSPANEL);
}

// --------------------------------------------------------------------------------------
//  GSFrame Implementation
// --------------------------------------------------------------------------------------

static const uint TitleBarUpdateMs = 333;


GSFrame::GSFrame( const wxString& title)
	: wxFrame(NULL, wxID_ANY, title, g_Conf->GSWindow.WindowPos)
	, m_timer_UpdateTitle( this )
{
	SetIcons( wxGetApp().GetIconBundle() );
	SetBackgroundColour( *wxBLACK );

	wxStaticText* label = new wxStaticText( this, wxID_ANY, _("GS Output is Disabled!") );
	m_id_OutputDisabled = label->GetId();
	label->SetFont( wxFont( 20, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD ) );
	label->SetForegroundColour( *wxWHITE );

	AppStatusEvent_OnSettingsApplied();

	GSPanel* gsPanel = new GSPanel( this );
	gsPanel->Show( !EmuConfig.GS.DisableOutput );
	m_id_gspanel = gsPanel->GetId();

	// TODO -- Implement this GS window status window!  Whee.
	// (main concern is retaining proper client window sizes when closing/re-opening the window).
	//m_statusbar = CreateStatusBar( 2 );

	Bind(wxEVT_CLOSE_WINDOW, &GSFrame::OnCloseWindow, this);
	Bind(wxEVT_MOVE, &GSFrame::OnMove, this);
	Bind(wxEVT_SIZE, &GSFrame::OnResize, this);
	Bind(wxEVT_ACTIVATE, &GSFrame::OnActivate, this);

	Bind(wxEVT_TIMER, &GSFrame::OnUpdateTitle, this, m_timer_UpdateTitle.GetId());
}

GSFrame::~GSFrame() throw()
{
}

void GSFrame::OnCloseWindow(wxCloseEvent& evt)
{
	sApp.OnGsFrameClosed( GetId() );
	Hide();		// and don't close it.
}

bool GSFrame::ShowFullScreen(bool show, bool updateConfig)
{
	/*if( show != IsFullScreen() )
		Console.WriteLn( Color_StrongMagenta, "(gsFrame) Switching to %s mode...", show ? "Fullscreen" : "Windowed" );*/

	if (updateConfig && g_Conf->GSWindow.IsFullscreen != show)
	{
		g_Conf->GSWindow.IsFullscreen = show;
		wxGetApp().PostIdleMethod( AppSaveSettings );
	}

	// IMPORTANT!  On MSW platforms you must ALWAYS show the window prior to calling
	// ShowFullscreen(), otherwise the window will be oddly unstable (lacking input and unable
	// to properly flip back into fullscreen mode after alt-enter).  I don't know if that
	// also happens on Linux.

	if( !IsShown() ) Show();
	bool retval = _parent::ShowFullScreen( show );
	
	return retval;
}



wxStaticText* GSFrame::GetLabel_OutputDisabled() const
{
	return (wxStaticText*)FindWindowById( m_id_OutputDisabled );
}

void GSFrame::CoreThread_OnResumed()
{
	m_timer_UpdateTitle.Start( TitleBarUpdateMs );
	if( !IsShown() ) Show();
}

void GSFrame::CoreThread_OnSuspended()
{
	if( !IsBeingDeleted() && g_Conf->GSWindow.CloseOnEsc ) Hide();
}

void GSFrame::CoreThread_OnStopped()
{
	//if( !IsBeingDeleted() ) Destroy();
}

void GSFrame::CorePlugins_OnShutdown()
{
	if( !IsBeingDeleted() ) Destroy();
}

// overrides base Show behavior.
bool GSFrame::Show( bool shown )
{
	if( shown )
	{
		GSPanel* gsPanel = GetViewport();

		if( !gsPanel || gsPanel->IsBeingDeleted() )
		{
			gsPanel = new GSPanel( this );
			m_id_gspanel = gsPanel->GetId();
		}

		gsPanel->Show( !EmuConfig.GS.DisableOutput );
		gsPanel->DoResize();
		gsPanel->SetFocus();

		if( wxStaticText* label = GetLabel_OutputDisabled() )
			label->Show( EmuConfig.GS.DisableOutput );

		if( !m_timer_UpdateTitle.IsRunning() )
			m_timer_UpdateTitle.Start( TitleBarUpdateMs );
	}
	else
	{
		m_timer_UpdateTitle.Stop();
	}

	return _parent::Show( shown );
}

void GSFrame::AppStatusEvent_OnSettingsApplied()
{
	if( IsBeingDeleted() ) return;

	SetWindowStyle((g_Conf->GSWindow.DisableResizeBorders ? 0 : wxRESIZE_BORDER) | wxCAPTION | wxCLIP_CHILDREN |
			wxSYSTEM_MENU | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX);
	if (!IsFullScreen() && !IsMaximized())
		SetClientSize(g_Conf->GSWindow.WindowSize);
	Refresh();

	if( g_Conf->GSWindow.CloseOnEsc )
	{
		if( IsShown() && !CorePlugins.IsOpen(PluginId_GS) )
			Show( false );
	}

	if( wxStaticText* label = GetLabel_OutputDisabled() )
		label->Show( EmuConfig.GS.DisableOutput );
}

GSPanel* GSFrame::GetViewport()
{
	return (GSPanel*)FindWindowById( m_id_gspanel );
}


void GSFrame::OnUpdateTitle( wxTimerEvent& evt )
{
#ifdef __linux__
	// Important Linux note: When the title is set in fullscreen the window is redrawn. Unfortunately
	// an intermediate white screen appears too which leads to a very annoying flickering.
	if (IsFullScreen()) return;
#endif
	AppConfig::UiTemplateOptions& templates = g_Conf->Templates;

	double fps = wxGetApp().FpsManager.GetFramerate();
	// The "not PAL" case covers both Region_NTSC and Region_NTSC_PROGRESSIVE
	float per = gsRegionMode == Region_PAL ? (fps * 100) / EmuConfig.GS.FrameratePAL.ToFloat() : (fps * 100) / EmuConfig.GS.FramerateNTSC.ToFloat();

	char gsDest[128];
	gsDest[0] = 0; // No need to set whole array to NULL.
	GSgetTitleInfo2( gsDest, sizeof(gsDest) );

	wxString limiterStr = templates.LimiterUnlimited;

	if( g_Conf->EmuOptions.GS.FrameLimitEnable )
	{
		switch( g_LimiterMode )
		{
			case Limit_Nominal:	limiterStr = templates.LimiterNormal; break;
			case Limit_Turbo:	limiterStr = templates.LimiterTurbo; break;
			case Limit_Slomo:	limiterStr = templates.LimiterSlowmo; break;
		}
	}

	FastFormatUnicode cpuUsage;
	if (m_CpuUsage.IsImplemented()) {
		m_CpuUsage.UpdateStats();

		cpuUsage.Write(L"EE: %3d%%", m_CpuUsage.GetEEcorePct());
		cpuUsage.Write(L" | GS: %3d%%", m_CpuUsage.GetGsPct());

		if (THREAD_VU1)
			cpuUsage.Write(L" | VU: %3d%%", m_CpuUsage.GetVUPct());

		pxNonReleaseCode(cpuUsage.Write(L" | UI: %3d%%", m_CpuUsage.GetGuiPct()));
	}

	const u64& smode2 = *(u64*)PS2GS_BASE(GS_SMODE2);
	wxString omodef = (smode2 & 2) ? templates.OutputFrame : templates.OutputField;
	wxString omodei = (smode2 & 1) ? templates.OutputInterlaced : templates.OutputProgressive;

	wxString title = templates.TitleTemplate;
	title.Replace(L"${slot}",		pxsFmt(L"%d", States_GetCurrentSlot()));
	title.Replace(L"${limiter}",	limiterStr);
	title.Replace(L"${speed}",		pxsFmt(L"%3d%%", lround(per)));
	title.Replace(L"${vfps}",		pxsFmt(L"%.02f", fps));
	title.Replace(L"${cpuusage}",	cpuUsage);
	title.Replace(L"${omodef}",		omodef);
	title.Replace(L"${omodei}",		omodei);
	title.Replace(L"${gsdx}",		fromUTF8(gsDest));
	if (CoreThread.IsPaused())
		title = templates.Paused + title;

	SetTitle(title);
}

void GSFrame::OnActivate( wxActivateEvent& evt )
{
	if( IsBeingDeleted() ) return;

	evt.Skip();
	if( wxWindow* gsPanel = GetViewport() ) gsPanel->SetFocus();
}

void GSFrame::OnMove( wxMoveEvent& evt )
{
	if( IsBeingDeleted() ) return;

	evt.Skip();

	g_Conf->GSWindow.IsMaximized = IsMaximized();

	// evt.GetPosition() returns the client area position, not the window frame position.
	if( !g_Conf->GSWindow.IsMaximized && !IsFullScreen() && !IsIconized() && IsVisible() )
		g_Conf->GSWindow.WindowPos = GetScreenPosition();

	// wxGTK note: X sends gratuitous amounts of OnMove messages for various crap actions
	// like selecting or deselecting a window, which muck up docking logic.  We filter them
	// out using 'lastpos' here. :)

	//static wxPoint lastpos( wxDefaultCoord, wxDefaultCoord );
	//if( lastpos == evt.GetPosition() ) return;
	//lastpos = evt.GetPosition();
}

void GSFrame::SetFocus()
{
	_parent::SetFocus();
	if( GSPanel* gsPanel = GetViewport() )
		gsPanel->SetFocusFromKbd();
}

void GSFrame::OnResize( wxSizeEvent& evt )
{
	if( IsBeingDeleted() ) return;

	if( !IsFullScreen() && !IsMaximized() && IsVisible() )
	{
		g_Conf->GSWindow.WindowSize	= GetClientSize();
	}

	if( wxStaticText* label = GetLabel_OutputDisabled() )
		label->CentreOnParent();

	if( GSPanel* gsPanel = GetViewport() )
	{
		gsPanel->DoResize();
		gsPanel->SetFocus();
	}

	//wxPoint hudpos = wxPoint(-10,-10) + (GetClientSize() - m_hud->GetSize());
	//m_hud->SetPosition( hudpos ); //+ GetScreenPosition() + GetClientAreaOrigin() );

	// if we skip, the panel is auto-sized to fit our window anyway, which we do not want!
	//evt.Skip();
}
