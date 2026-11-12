/*
 * TeacherActionIcons.cpp - NextSpace Teacher Console action icons drawn from bundled Tabler path data
 *
 * This file is part of NextSpace Classroom Management, a modified version of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>

#include "Feature.h"
#include "TeacherActionIconData.h"
#include "TeacherActionIcons.h"


namespace {

using TeacherActionIconData::Glyph;
using TeacherActionIconData::Op;

struct IconSpec
{
	Glyph off;
	Glyph on;
};

IconSpec single( Glyph glyph )
{
	return { glyph, glyph };
}

IconSpec offOn( Glyph off, Glyph on )
{
	return { off, on };
}

// V3an map by feature name, never by localized text; unlisted features keep Feature::iconUrl().
// StartApp/OpenWebsite and their dynamic entries are resolved in forFeature().
const QHash<QString, IconSpec>& featureIcons()
{
	static const QHash<QString, IconSpec> icons{
		{ QStringLiteral("MonitoringMode"), single( Glyph::DeviceDesktop ) },
		{ QStringLiteral("Demo"), single( Glyph::ScreenShare ) },
		{ QStringLiteral("ScreenLock"), offOn( Glyph::Lock, Glyph::LockOpen ) },
		{ QStringLiteral("RemoteControl"), single( Glyph::Pointer ) },
		{ QStringLiteral("TextMessage"), single( Glyph::Message ) },
		{ QStringLiteral("DistributeFiles"), single( Glyph::FileUpload ) },
		{ QStringLiteral("RemoteView"), single( Glyph::Eye ) },
		{ QStringLiteral("FileCollect"), single( Glyph::FileDownload ) },
		{ QStringLiteral("Screenshot"), single( Glyph::Camera ) },
		{ QStringLiteral("PowerOn"), single( Glyph::Power ) },
		{ QStringLiteral("Reboot"), single( Glyph::Refresh ) },
		{ QStringLiteral("PowerDown"), single( Glyph::Power ) },
		{ QStringLiteral("UserLogin"), single( Glyph::Login ) },
		{ QStringLiteral("UserLogoff"), single( Glyph::Logout ) },
		{ QStringLiteral("ShareOwnScreenFullScreen"), single( Glyph::Maximize ) },
		{ QStringLiteral("ShareUserScreenFullScreen"), single( Glyph::Maximize ) },
		{ QStringLiteral("ShareOwnScreenWindow"), single( Glyph::AppWindow ) },
		{ QStringLiteral("ShareUserScreenWindow"), single( Glyph::AppWindow ) },
	};
	return icons;
}



QPainterPath glyphPath( Glyph glyph )
{
	const auto& data = TeacherActionIconData::glyphPaths[static_cast<int>( glyph )];
	QPainterPath path;
	const float* c = data.coords;
	for( int i = 0; i < data.opCount; ++i )
	{
		switch( data.ops[i] )
		{
		case Op::MoveTo: path.moveTo( c[0], c[1] ); c += 2; break;
		case Op::LineTo: path.lineTo( c[0], c[1] ); c += 2; break;
		case Op::CubicTo: path.cubicTo( c[0], c[1], c[2], c[3], c[4], c[5] ); c += 6; break;
		}
	}
	return path;
}



// strokes the 24x24 geometry uniformly into the requested rectangle in the palette's text colour
class TeacherActionIconEngine : public QIconEngine
{
public:
	TeacherActionIconEngine( const IconSpec& spec, const QPalette& palette ) :
		m_off( glyphPath( spec.off ) ),
		m_on( glyphPath( spec.on ) ),
		m_palette( palette )
	{
	}

	void paint( QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state ) override
	{
		const auto side = qMin( rect.width(), rect.height() );
		if( side <= 0 )
		{
			return;
		}

		painter->save();
		painter->setRenderHint( QPainter::Antialiasing );
		painter->translate( rect.x() + ( rect.width() - side ) / 2.0, rect.y() + ( rect.height() - side ) / 2.0 );
		const auto scale = qreal( side ) / qreal( TeacherActionIconData::ViewBoxSize );
		painter->scale( scale, scale );
		// 2.1 view-box units: 1.75 logical px at 20 logical px; the painter's device transform adds the DPR
		painter->setPen( QPen( color( mode ), qreal( TeacherActionIconData::RenderStrokeWidth ), Qt::SolidLine,
							   Qt::RoundCap, Qt::RoundJoin ) );
		painter->setBrush( Qt::NoBrush );
		painter->drawPath( state == QIcon::On ? m_on : m_off );
		painter->restore();
	}

	QPixmap pixmap( const QSize& size, QIcon::Mode mode, QIcon::State state ) override
	{
		return scaledPixmap( size, mode, state, 1.0 );
	}

	QPixmap scaledPixmap( const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale ) override
	{
		QPixmap pixmap( ( QSizeF( size ) * scale ).toSize() );
		pixmap.setDevicePixelRatio( scale );
		pixmap.fill( Qt::transparent );
		QPainter painter( &pixmap );
		paint( &painter, QRect( QPoint( 0, 0 ), size ), mode, state );
		return pixmap;
	}

	QIconEngine* clone() const override
	{
		return new TeacherActionIconEngine( *this );
	}

	QString key() const override
	{
		return QStringLiteral("TeacherActionIconEngine");
	}

	bool isNull() override
	{
		return false;
	}

private:
	// colour follows the palette and is never the only state cue
	QColor color( QIcon::Mode mode ) const
	{
		switch( mode )
		{
		case QIcon::Disabled: return m_palette.color( QPalette::Disabled, QPalette::WindowText );
		case QIcon::Selected: return m_palette.color( QPalette::Active, QPalette::HighlightedText );
		case QIcon::Active: return m_palette.color( QPalette::Active, QPalette::WindowText );
		case QIcon::Normal: break;
		}
		return m_palette.color( QPalette::Normal, QPalette::WindowText );
	}

	QPainterPath m_off;
	QPainterPath m_on;
	QPalette m_palette;
};



QIcon icon( const IconSpec& spec, const QPalette& palette )
{
	return QIcon( new TeacherActionIconEngine( spec, palette ) );
}



using ViewControl = TeacherActionIcons::ViewControl;

// V3ao map by semantic role; no default case, so an unmapped role is a compiler warning
Glyph viewControlGlyph( ViewControl control )
{
	switch( control )
	{
	case ViewControl::ComputerSelectPanel: return Glyph::DevicesPc;
	case ViewControl::ScreenshotManagementPanel: return Glyph::Camera;
	case ViewControl::SlideshowPanel: return Glyph::Slideshow;
	case ViewControl::SpotlightPanel: return Glyph::Lamp2;
	case ViewControl::PoweredOnFilter: return Glyph::Power;
	case ViewControl::LoggedOnUsersFilter: return Glyph::Users;
	case ViewControl::AutoAdjustIconSize: return Glyph::ZoomScan;
	case ViewControl::CustomComputerPositions: return Glyph::LayoutGrid;
	case ViewControl::AlignComputers: return Glyph::AlignBoxCenterMiddle;
	case ViewControl::LoadComputerPositions: return Glyph::FolderOpen;
	case ViewControl::SaveComputerPositions: return Glyph::DeviceFloppy;
	case ViewControl::About: return Glyph::InfoCircle;
	}
	Q_UNREACHABLE();
	return Glyph::InfoCircle;
}



using SecondaryControl = TeacherActionIcons::SecondaryControl;

// V3ap/V3aq/V3ar/V3as/V3at/V3av map by semantic role; no default case, so an unmapped role is a compiler warning.
// Only the checkable start/pause and realtime buttons have distinct Off and On glyphs (play/pause and
// refresh-off/refresh).
IconSpec secondaryControlSpec( SecondaryControl control )
{
	switch( control )
	{
	case SecondaryControl::ShowScreenshot: return single( Glyph::Eye );
	case SecondaryControl::DeleteScreenshot: return single( Glyph::Trash );
	case SecondaryControl::PreviousSlide: return single( Glyph::PlayerSkipBack );
	case SecondaryControl::NextSlide: return single( Glyph::PlayerSkipForward );
	case SecondaryControl::SlideshowStartPause: return offOn( Glyph::PlayerPlay, Glyph::PlayerPause );
	case SecondaryControl::SlideshowSeparateWindow: return single( Glyph::Maximize );
	case SecondaryControl::SpotlightAdd: return single( Glyph::DeviceDesktopPlus );
	case SecondaryControl::SpotlightRemove: return single( Glyph::DeviceDesktopMinus );
	case SecondaryControl::SpotlightRealtime: return offOn( Glyph::RefreshOff, Glyph::Refresh );
	case SecondaryControl::CollectFilesOpenDestination: return single( Glyph::FolderOpen );
	}
	Q_UNREACHABLE();
	return single( Glyph::Trash );
}



using RemoteAccessControl = TeacherActionIcons::RemoteAccessControl;

// V3aw map by semantic role, separate from the secondary controls; no default case, so an unmapped role is a
// compiler warning. The two checkable toggles draw the command their label offers: View only (eye, Off) and
// Remote control (pointer, On), Fullscreen (maximize, Off) and Window (app-window, On).
IconSpec remoteAccessControlSpec( RemoteAccessControl control )
{
	switch( control )
	{
	case RemoteAccessControl::ViewOnlyToggle: return offOn( Glyph::Eye, Glyph::Pointer );
	case RemoteAccessControl::SelectScreen: return single( Glyph::DevicesPc );
	case RemoteAccessControl::SendShortcut: return single( Glyph::Keyboard );
	case RemoteAccessControl::Screenshot: return single( Glyph::Camera );
	case RemoteAccessControl::FullscreenToggle: return offOn( Glyph::Maximize, Glyph::AppWindow );
	case RemoteAccessControl::Exit: return single( Glyph::X );
	}
	Q_UNREACHABLE();
	return single( Glyph::X );
}



using FileTransferState = TeacherActionIcons::FileTransferState;

// V3ax map by transfer state; no default case, so an unmapped state is a compiler warning. Each state has one
// stable glyph, drawn for both Off and On.
Glyph fileTransferStateGlyph( FileTransferState state )
{
	switch( state )
	{
	case FileTransferState::Scheduled: return Glyph::Clock;
	case FileTransferState::Transferring: return Glyph::Transfer;
	case FileTransferState::Waiting: return Glyph::Hourglass;
	case FileTransferState::Finished: return Glyph::CircleCheck;
	}
	Q_UNREACHABLE();
	return Glyph::Clock;
}




using AdministrationPage = TeacherActionIcons::AdministrationPage;

// V3bb map by Administration page role; no default case, so an unmapped role is a compiler warning. Each page has one
// stable glyph, drawn for both Off and On; five reuse reviewed glyphs (device-desktop, devices-pc, screen-share, apps
// and transfer)
Glyph administrationPageGlyph( AdministrationPage page )
{
	switch( page )
	{
	case AdministrationPage::General: return Glyph::Settings;
	case AdministrationPage::StudentAgent: return Glyph::DeviceDesktop;
	case AdministrationPage::TeacherConsole: return Glyph::Presentation;
	case AdministrationPage::AccessControl: return Glyph::ShieldLock;
	case AdministrationPage::AuthenticationKeys: return Glyph::Key;
	case AdministrationPage::LocationsAndComputers: return Glyph::DevicesPc;
	case AdministrationPage::Broadcast: return Glyph::ScreenShare;
	case AdministrationPage::ApplicationsAndWebsites: return Glyph::Apps;
	case AdministrationPage::FileTransfer: return Glyph::Transfer;
	case AdministrationPage::Windows: return Glyph::BrandWindows;
	case AdministrationPage::LdapDirectory: return Glyph::Hierarchy2;
	}
	Q_UNREACHABLE();
	return Glyph::Settings;
}



using AdministrationCommand = TeacherActionIcons::AdministrationCommand;

// V3bc map by Administration menu command, separate from the Teacher view and secondary controls; no default case, so
// an unmapped command is a compiler warning. Each command has one stable glyph, drawn for both Off and On; all four reuse
// reviewed glyphs (device-floppy, folder-open, info-circle and trash)
Glyph administrationCommandGlyph( AdministrationCommand command )
{
	switch( command )
	{
	case AdministrationCommand::SaveSettings: return Glyph::DeviceFloppy;
	case AdministrationCommand::LoadSettings: return Glyph::FolderOpen;
	case AdministrationCommand::About: return Glyph::InfoCircle;
	case AdministrationCommand::ResetConfiguration: return Glyph::Trash;
	}
	Q_UNREACHABLE();
	return Glyph::InfoCircle;
}



using AdministrationAccessControlCommand = TeacherActionIcons::AdministrationAccessControlCommand;

// V3bd map by Administration Access Control page command, separate from the Teacher, file-transfer, page and
// menu-command roles; no default case, so an unmapped command is a compiler warning. Each command has one stable glyph,
// drawn for both Off and On; Test, RemoveRule and EditRule reuse the reviewed circle-check, trash and pencil, and the
// other five draw the appended arrow-right, arrow-left, plus, arrow-up and arrow-down
Glyph administrationAccessControlCommandGlyph( AdministrationAccessControlCommand command )
{
	switch( command )
	{
	case AdministrationAccessControlCommand::Test: return Glyph::CircleCheck;
	case AdministrationAccessControlCommand::AddAuthorizedGroup: return Glyph::ArrowRight;
	case AdministrationAccessControlCommand::RemoveAuthorizedGroup: return Glyph::ArrowLeft;
	case AdministrationAccessControlCommand::AddRule: return Glyph::Plus;
	case AdministrationAccessControlCommand::RemoveRule: return Glyph::Trash;
	case AdministrationAccessControlCommand::EditRule: return Glyph::Pencil;
	case AdministrationAccessControlCommand::MoveRuleUp: return Glyph::ArrowUp;
	case AdministrationAccessControlCommand::MoveRuleDown: return Glyph::ArrowDown;
	}
	Q_UNREACHABLE();
	return Glyph::CircleCheck;
}



using AdministrationGeneralCommand = TeacherActionIcons::AdministrationGeneralCommand;

// V3be map by Administration General page command, separate from the Teacher, file-transfer, page, menu-command and
// Access Control roles; no default case, so an unmapped command is a compiler warning. Each command has one stable glyph,
// drawn for both Off and On; all three reuse the reviewed circle-check, folder-open and trash
Glyph administrationGeneralCommandGlyph( AdministrationGeneralCommand command )
{
	switch( command )
	{
	case AdministrationGeneralCommand::TestAuthentication: return Glyph::CircleCheck;
	case AdministrationGeneralCommand::OpenLogDirectory: return Glyph::FolderOpen;
	case AdministrationGeneralCommand::ClearLogFiles: return Glyph::Trash;
	}
	Q_UNREACHABLE();
	return Glyph::CircleCheck;
}



using AdministrationTeacherConsoleCommand = TeacherActionIcons::AdministrationTeacherConsoleCommand;

// V3bf map by Administration Teacher Console page command, separate from the Teacher, file-transfer, page, menu-command,
// Access Control and General page roles; no default case, so an unmapped command is a compiler warning. Each command has
// one stable glyph, drawn for both Off and On; the three directory commands reuse the reviewed folder-open, Disable the
// reviewed arrow-right (All features to Disabled features) and Enable the reviewed arrow-left (back)
Glyph administrationTeacherConsoleCommandGlyph( AdministrationTeacherConsoleCommand command )
{
	switch( command )
	{
	case AdministrationTeacherConsoleCommand::OpenUserConfigurationDirectory: return Glyph::FolderOpen;
	case AdministrationTeacherConsoleCommand::OpenScreenshotDirectory: return Glyph::FolderOpen;
	case AdministrationTeacherConsoleCommand::OpenConfigurationTemplatesDirectory: return Glyph::FolderOpen;
	case AdministrationTeacherConsoleCommand::DisableFeature: return Glyph::ArrowRight;
	case AdministrationTeacherConsoleCommand::EnableFeature: return Glyph::ArrowLeft;
	}
	Q_UNREACHABLE();
	return Glyph::FolderOpen;
}



using AdministrationStudentAgentCommand = TeacherActionIcons::AdministrationStudentAgentCommand;

// V3bh map by Administration Student Agent page service command, separate from the Teacher, file-transfer, page,
// menu-command, Access Control, General and Teacher Console page roles; no default case, so an unmapped command is a
// compiler warning. Each command has one stable glyph, drawn for both Off and On: Start reuses the reviewed player-play
// and Stop draws the pinned player-stop
Glyph administrationStudentAgentCommandGlyph( AdministrationStudentAgentCommand command )
{
	switch( command )
	{
	case AdministrationStudentAgentCommand::StartService: return Glyph::PlayerPlay;
	case AdministrationStudentAgentCommand::StopService: return Glyph::PlayerStop;
	}
	Q_UNREACHABLE();
	return Glyph::PlayerPlay;
}



using AdministrationAuthenticationKeysCommand = TeacherActionIcons::AdministrationAuthenticationKeysCommand;

// V3bj map by Administration Authentication Keys page directory command, separate from the Teacher, file-transfer, page,
// menu-command, Access Control, General, Teacher Console and Student Agent page roles; no default case, so an unmapped
// command is a compiler warning. Each command has one stable glyph, drawn for both Off and On; both reuse the reviewed
// folder-open and stay separate roles (the public and the private key directory)
Glyph administrationAuthenticationKeysCommandGlyph( AdministrationAuthenticationKeysCommand command )
{
	switch( command )
	{
	case AdministrationAuthenticationKeysCommand::OpenPublicKeyDirectory: return Glyph::FolderOpen;
	case AdministrationAuthenticationKeysCommand::OpenPrivateKeyDirectory: return Glyph::FolderOpen;
	}
	Q_UNREACHABLE();
	return Glyph::FolderOpen;
}



using AdministrationBuiltinDirectoryCommand = TeacherActionIcons::AdministrationBuiltinDirectoryCommand;

// V3bk map by Administration built-in directory Locations & computers page command, separate from the Teacher,
// file-transfer, page, menu-command, Access Control, General, Teacher Console, Student Agent and Authentication Keys page
// roles; no default case, so an unmapped command is a compiler warning. Each command has one stable glyph, drawn for both
// Off and On; all eight reuse the reviewed plus, trash, arrow-up and arrow-down, and the location and computer roles
// stay separate although each pair draws the same glyph
Glyph administrationBuiltinDirectoryCommandGlyph( AdministrationBuiltinDirectoryCommand command )
{
	switch( command )
	{
	case AdministrationBuiltinDirectoryCommand::AddLocation: return Glyph::Plus;
	case AdministrationBuiltinDirectoryCommand::RemoveLocation: return Glyph::Trash;
	case AdministrationBuiltinDirectoryCommand::MoveLocationUp: return Glyph::ArrowUp;
	case AdministrationBuiltinDirectoryCommand::MoveLocationDown: return Glyph::ArrowDown;
	case AdministrationBuiltinDirectoryCommand::AddComputer: return Glyph::Plus;
	case AdministrationBuiltinDirectoryCommand::RemoveComputer: return Glyph::Trash;
	case AdministrationBuiltinDirectoryCommand::MoveComputerUp: return Glyph::ArrowUp;
	case AdministrationBuiltinDirectoryCommand::MoveComputerDown: return Glyph::ArrowDown;
	}
	Q_UNREACHABLE();
	return Glyph::Plus;
}



using AdministrationDesktopServicesCommand = TeacherActionIcons::AdministrationDesktopServicesCommand;

// V3bl map by Administration Desktop Services (Applications & websites) page command, separate from the Teacher,
// file-transfer, page, menu-command, Access Control, General, Teacher Console, Student Agent, Authentication Keys and
// built-in directory page roles; no default case, so an unmapped command is a compiler warning. Each command has one
// stable glyph, drawn for both Off and On; all four reuse the reviewed plus and trash, and the application and website
// roles stay separate although each pair draws the same glyph
Glyph administrationDesktopServicesCommandGlyph( AdministrationDesktopServicesCommand command )
{
	switch( command )
	{
	case AdministrationDesktopServicesCommand::AddApplication: return Glyph::Plus;
	case AdministrationDesktopServicesCommand::RemoveApplication: return Glyph::Trash;
	case AdministrationDesktopServicesCommand::AddWebsite: return Glyph::Plus;
	case AdministrationDesktopServicesCommand::RemoveWebsite: return Glyph::Trash;
	}
	Q_UNREACHABLE();
	return Glyph::Plus;
}



using AdministrationFileTransferCommand = TeacherActionIcons::AdministrationFileTransferCommand;

// V3bm map by Administration File transfer page directory command, separate from the Teacher, file-transfer state,
// page, menu-command, Access Control, General, Teacher Console, Student Agent, Authentication Keys, built-in directory
// and Desktop Services page roles; no default case, so an unmapped command is a compiler warning. Each command has one
// stable glyph, drawn for both Off and On; all four reuse the reviewed folder-open and stay separate roles although
// they draw the same glyph
Glyph administrationFileTransferCommandGlyph( AdministrationFileTransferCommand command )
{
	switch( command )
	{
	case AdministrationFileTransferCommand::BrowseDefaultSourceDirectory: return Glyph::FolderOpen;
	case AdministrationFileTransferCommand::BrowseDestinationDirectory: return Glyph::FolderOpen;
	case AdministrationFileTransferCommand::BrowseFilesToCollectSourceDirectory: return Glyph::FolderOpen;
	case AdministrationFileTransferCommand::BrowseCollectedFilesDestinationDirectory: return Glyph::FolderOpen;
	}
	Q_UNREACHABLE();
	return Glyph::FolderOpen;
}

}



QIcon TeacherActionIcons::forFeature( const Feature& feature, const QPalette& palette )
{
	const auto& name = feature.name();

	// dynamic Start Application/Open Website entries share their parent's name: the parent has no parent UID,
	// the custom entry keeps an icon URL and predefined entries have none
	if( name == QLatin1String("StartApp") || name == QLatin1String("OpenWebsite") )
	{
		if( feature.parentUid().isNull() )
		{
			const auto glyph = name == QLatin1String("StartApp") ? Glyph::Apps : Glyph::World;
			return icon( single( glyph ), palette );
		}
		if( feature.iconUrl().isEmpty() == false )
		{
			return icon( single( Glyph::Pencil ), palette );
		}
		return {};
	}

	const auto it = featureIcons().constFind( name );
	if( it == featureIcons().constEnd() )
	{
		return QIcon( feature.iconUrl() );
	}
	return icon( *it, palette );
}



QIcon TeacherActionIcons::more( const QPalette& palette )
{
	return icon( single( Glyph::Dots ), palette );
}



QIcon TeacherActionIcons::forViewControl( ViewControl control, const QPalette& palette )
{
	return icon( single( viewControlGlyph( control ) ), palette );
}



QIcon TeacherActionIcons::forSecondaryControl( SecondaryControl control, const QPalette& palette )
{
	return icon( secondaryControlSpec( control ), palette );
}



QIcon TeacherActionIcons::forRemoteAccessControl( RemoteAccessControl control, const QPalette& palette )
{
	return icon( remoteAccessControlSpec( control ), palette );
}



QIcon TeacherActionIcons::forFileTransferState( FileTransferState state, const QPalette& palette )
{
	return icon( single( fileTransferStateGlyph( state ) ), palette );
}




QIcon TeacherActionIcons::forAdministrationPage( AdministrationPage page, const QPalette& palette )
{
	return icon( single( administrationPageGlyph( page ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationCommand( AdministrationCommand command, const QPalette& palette )
{
	return icon( single( administrationCommandGlyph( command ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationAccessControlCommand( AdministrationAccessControlCommand command, const QPalette& palette )
{
	return icon( single( administrationAccessControlCommandGlyph( command ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationGeneralCommand( AdministrationGeneralCommand command, const QPalette& palette )
{
	return icon( single( administrationGeneralCommandGlyph( command ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationTeacherConsoleCommand( AdministrationTeacherConsoleCommand command,
																   const QPalette& palette )
{
	return icon( single( administrationTeacherConsoleCommandGlyph( command ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationStudentAgentCommand( AdministrationStudentAgentCommand command, const QPalette& palette )
{
	return icon( single( administrationStudentAgentCommandGlyph( command ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationAuthenticationKeysCommand( AdministrationAuthenticationKeysCommand command,
																	  const QPalette& palette )
{
	return icon( single( administrationAuthenticationKeysCommandGlyph( command ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationBuiltinDirectoryCommand( AdministrationBuiltinDirectoryCommand command,
																	const QPalette& palette )
{
	return icon( single( administrationBuiltinDirectoryCommandGlyph( command ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationDesktopServicesCommand( AdministrationDesktopServicesCommand command,
																   const QPalette& palette )
{
	return icon( single( administrationDesktopServicesCommandGlyph( command ) ), palette );
}



QIcon TeacherActionIcons::forAdministrationFileTransferCommand( AdministrationFileTransferCommand command,
																const QPalette& palette )
{
	return icon( single( administrationFileTransferCommandGlyph( command ) ), palette );
}
