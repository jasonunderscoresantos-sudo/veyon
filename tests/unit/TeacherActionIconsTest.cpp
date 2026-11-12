/*
 * TeacherActionIconsTest.cpp - NextSpace V3an/V3ao/V3ap/V3aq/V3ar/V3as/V3at/V3av/V3aw/V3ax: Teacher action-icon mapping and offscreen rendering
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

// Offscreen evidence only (run with QT_QPA_PLATFORM=offscreen): it proves the exact feature map, palette roles,
// state geometry and in-box rendering at logical 20 px; it is not native Windows rendering.

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include <cstring>
#include <functional>
#include <type_traits>

#include "Feature.h"
#include "NextSpaceActionIcons.h"
#include "TeacherActionIconData.h"
#include "TeacherActionIcons.h"

// V3bi: Toast has no export macro and its header and every build file stay frozen, so the test compiles the unchanged
// product core/src/Toast.cpp itself (AUTOMOC generates moc_Toast.cpp from core/src/Toast.h for the include below); the
// five masks it loads are the compiled core.qrc resources of the linked veyon-core
#include "Toast.cpp"
#include "moc_Toast.cpp"

namespace {

using namespace TeacherActionIconData;

constexpr int LogicalSize = 20;
constexpr int Pad = 4;
const QList<qreal> Scales{ 1.0, 1.25, 1.5, 2.0 };
const QList<QIcon::Mode> Modes{ QIcon::Normal, QIcon::Active, QIcon::Disabled, QIcon::Selected };

enum class PaletteKind { Light, Dark, HighContrast };

// synthetic palettes for the contact sheets, from docs/visual-rules.md tokens and a Windows-style high-contrast set
QPalette syntheticPalette( PaletteKind kind )
{
	struct Roles { const char* window; const char* text; const char* disabled; const char* highlight; const char* highlighted; };
	static const Roles light{ "#ffffff", "#10243a", "#8494a6", "#087f8c", "#ffffff" };
	static const Roles dark{ "#142235", "#f1f5f9", "#6f8296", "#39c6c5", "#10243a" };
	static const Roles highContrast{ "#000000", "#ffffff", "#3ff23f", "#1aebff", "#000000" };
	const auto& r = kind == PaletteKind::Light ? light : kind == PaletteKind::Dark ? dark : highContrast;

	QPalette palette;
	for( const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled } )
	{
		palette.setColor( group, QPalette::Window, QColor::fromString( QLatin1String( r.window ) ) );
		palette.setColor( group, QPalette::WindowText,
						  QColor::fromString( QLatin1String( group == QPalette::Disabled ? r.disabled : r.text ) ) );
		palette.setColor( group, QPalette::Highlight, QColor::fromString( QLatin1String( r.highlight ) ) );
		palette.setColor( group, QPalette::HighlightedText, QColor::fromString( QLatin1String( r.highlighted ) ) );
	}
	return palette;
}

QString paletteName( PaletteKind kind )
{
	return kind == PaletteKind::Light ? QStringLiteral("Light") :
			kind == PaletteKind::Dark ? QStringLiteral("Dark") : QStringLiteral("HighContrast");
}

// every role a wrong implementation could pick gets its own colour
QPalette probePalette()
{
	QPalette palette;
	for( const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled } )
	{
		palette.setColor( group, QPalette::ButtonText, QColor( 255, 0, 255 ) );
		palette.setColor( group, QPalette::Text, QColor( 0, 255, 255 ) );
		palette.setColor( group, QPalette::HighlightedText, QColor( 96, 96, 96 ) );
	}
	palette.setColor( QPalette::Active, QPalette::WindowText, QColor( 192, 0, 0 ) );
	palette.setColor( QPalette::Inactive, QPalette::WindowText, QColor( 128, 128, 0 ) );
	palette.setColor( QPalette::Disabled, QPalette::WindowText, QColor( 0, 160, 0 ) );
	palette.setColor( QPalette::Active, QPalette::HighlightedText, QColor( 0, 0, 192 ) );
	return palette;
}

Feature makeFeature( const QString& name, Feature::Uid parent = {}, const QString& iconUrl = {} )
{
	return Feature( name, Feature::Flag::Action | Feature::Flag::Master, QUuid::createUuid(), parent,
					name, {}, {}, iconUrl );
}

QPainterPath referencePath( const QString& name )
{
	for( const auto& glyph : glyphPaths )
	{
		if( QLatin1String( glyph.name ) != name )
		{
			continue;
		}
		QPainterPath path;
		const float* c = glyph.coords;
		for( int i = 0; i < glyph.opCount; ++i )
		{
			switch( glyph.ops[i] )
			{
			case Op::MoveTo: path.moveTo( c[0], c[1] ); c += 2; break;
			case Op::LineTo: path.lineTo( c[0], c[1] ); c += 2; break;
			case Op::CubicTo: path.cubicTo( c[0], c[1], c[2], c[3], c[4], c[5] ); c += 6; break;
			}
		}
		return path;
	}
	qFatal( "unknown glyph %s", qPrintable( name ) );
	return {};
}

QImage canvas( qreal dpr, const QSize& logical )
{
	QImage image( ( QSizeF( logical ) * dpr ).toSize(), QImage::Format_ARGB32_Premultiplied );
	image.setDevicePixelRatio( dpr );
	image.fill( Qt::transparent );
	return image;
}

QImage referenceImage( const QString& spec, const QColor& color, qreal dpr, int pad = 0 )
{
	auto image = canvas( dpr, QSize( LogicalSize + 2 * pad, LogicalSize + 2 * pad ) );
	QPainter painter( &image );
	painter.setRenderHint( QPainter::Antialiasing );
	painter.translate( pad, pad );
	painter.scale( qreal( LogicalSize ) / qreal( ViewBoxSize ), qreal( LogicalSize ) / qreal( ViewBoxSize ) );
	painter.setPen( QPen( color, qreal( RenderStrokeWidth ), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
	painter.setBrush( Qt::NoBrush );
	painter.drawPath( referencePath( spec ) );
	return image;
}

QImage renderIcon( const QIcon& icon, qreal dpr, QIcon::Mode mode, QIcon::State state, int pad = 0 )
{
	auto image = canvas( dpr, QSize( LogicalSize + 2 * pad, LogicalSize + 2 * pad ) );
	QPainter painter( &image );
	icon.paint( &painter, QRect( pad, pad, LogicalSize, LogicalSize ), Qt::AlignCenter, mode, state );
	return image;
}

int maxDifference( const QImage& a, const QImage& b )
{
	if( a.size() != b.size() )
	{
		return 256;
	}
	const auto x = a.convertToFormat( QImage::Format_ARGB32_Premultiplied );
	const auto y = b.convertToFormat( QImage::Format_ARGB32_Premultiplied );
	int result = 0;
	for( int row = 0; row < x.height(); ++row )
	{
		const auto* p = reinterpret_cast<const QRgb*>( x.constScanLine( row ) );
		const auto* q = reinterpret_cast<const QRgb*>( y.constScanLine( row ) );
		for( int col = 0; col < x.width(); ++col )
		{
			result = qMax( result, qMax( qAbs( qAlpha( p[col] ) - qAlpha( q[col] ) ),
										 qMax( qAbs( qRed( p[col] ) - qRed( q[col] ) ),
											   qMax( qAbs( qGreen( p[col] ) - qGreen( q[col] ) ),
													 qAbs( qBlue( p[col] ) - qBlue( q[col] ) ) ) ) ) );
		}
	}
	return result;
}

struct Ink
{
	int pixels = 0;
	int outside = 0;
	qint64 alpha = 0;
	int colourSamples = 0;
	int colourMismatches = 0;
};

// counts ink in the whole image, ink outside the device rectangle of `inner` and opaque pixels of another colour
Ink measure( const QImage& image, const QRect& inner, const QColor& expected = {} )
{
	Ink ink;
	const auto straight = image.convertToFormat( QImage::Format_ARGB32 );
	const auto dpr = image.devicePixelRatio();
	const QRect device( qRound( inner.x() * dpr ), qRound( inner.y() * dpr ),
						qRound( inner.width() * dpr ), qRound( inner.height() * dpr ) );
	for( int row = 0; row < straight.height(); ++row )
	{
		const auto* p = reinterpret_cast<const QRgb*>( straight.constScanLine( row ) );
		for( int col = 0; col < straight.width(); ++col )
		{
			const int a = qAlpha( p[col] );
			if( a == 0 )
			{
				continue;
			}
			++ink.pixels;
			ink.alpha += a;
			if( device.contains( col, row ) == false )
			{
				++ink.outside;
			}
			if( expected.isValid() && a >= 160 )
			{
				++ink.colourSamples;
				if( qAbs( qRed( p[col] ) - expected.red() ) > 3 || qAbs( qGreen( p[col] ) - expected.green() ) > 3 ||
					qAbs( qBlue( p[col] ) - expected.blue() ) > 3 )
				{
					++ink.colourMismatches;
				}
			}
		}
	}
	return ink;
}

QColor expectedColour( const QPalette& palette, QIcon::Mode mode )
{
	switch( mode )
	{
	case QIcon::Disabled: return palette.color( QPalette::Disabled, QPalette::WindowText );
	case QIcon::Selected: return palette.color( QPalette::Active, QPalette::HighlightedText );
	case QIcon::Normal:
	case QIcon::Active: break;
	}
	return palette.color( QPalette::Active, QPalette::WindowText );
}

// highest alpha of a logical 20 px render (ARGB32, no padding) inside a rectangle given in 24-unit view-box coordinates
int maxAlphaInUnits( const QImage& image, qreal dpr, const QRectF& units )
{
	const auto toDevice = [dpr]( qreal unit ) { return unit * LogicalSize / qreal( ViewBoxSize ) * dpr; };
	int result = 0;
	for( int row = qFloor( toDevice( units.top() ) ); row < qCeil( toDevice( units.bottom() ) ); ++row )
	{
		for( int col = qFloor( toDevice( units.left() ) ); col < qCeil( toDevice( units.right() ) ); ++col )
		{
			result = qMax( result, qAlpha( image.pixel( col, row ) ) );
		}
	}
	return result;
}

// summed absolute alpha difference of two equally sized renders relative to the larger total alpha
qreal alphaDifferenceRatio( const QImage& a, const QImage& b )
{
	const auto x = a.convertToFormat( QImage::Format_ARGB32 );
	const auto y = b.convertToFormat( QImage::Format_ARGB32 );
	qint64 difference = 0;
	qint64 totalA = 0;
	qint64 totalB = 0;
	for( int row = 0; row < x.height(); ++row )
	{
		for( int col = 0; col < x.width(); ++col )
		{
			const int alphaA = qAlpha( x.pixel( col, row ) );
			const int alphaB = qAlpha( y.pixel( col, row ) );
			difference += qAbs( alphaA - alphaB );
			totalA += alphaA;
			totalB += alphaB;
		}
	}
	return qreal( difference ) / qreal( qMax( qint64( 1 ), qMax( totalA, totalB ) ) );
}

using TeacherActionIcons::ViewControl;

// V3ao view-strip, grid-control and About roles: inventory row, role and expected Tabler glyph
struct ViewRow { const char* id; ViewControl control; const char* glyph; const char* role; };
const QList<ViewRow>& viewRows()
{
	static const QList<ViewRow> rows{
		{ "V3AM-035 Locations & computers", ViewControl::ComputerSelectPanel, "devices-pc", "ComputerSelectPanel" },
		{ "V3AM-036 Screenshots", ViewControl::ScreenshotManagementPanel, "camera", "ScreenshotManagementPanel" },
		{ "V3AM-037 Slideshow", ViewControl::SlideshowPanel, "slideshow", "SlideshowPanel" },
		{ "V3AM-038 Spotlight", ViewControl::SpotlightPanel, "lamp-2", "SpotlightPanel" },
		{ "V3AM-039 Powered-on filter", ViewControl::PoweredOnFilter, "power", "PoweredOnFilter" },
		{ "V3AM-040 Logged-on users filter", ViewControl::LoggedOnUsersFilter, "users", "LoggedOnUsersFilter" },
		{ "V3AM-041 Auto thumbnail size", ViewControl::AutoAdjustIconSize, "zoom-scan", "AutoAdjustIconSize" },
		{ "V3AM-042 Custom positions", ViewControl::CustomComputerPositions, "layout-grid", "CustomComputerPositions" },
		{ "V3AM-043 Align to grid", ViewControl::AlignComputers, "align-box-center-middle", "AlignComputers" },
		{ "V3AM-044 Load positions", ViewControl::LoadComputerPositions, "folder-open", "LoadComputerPositions" },
		{ "V3AM-044 Save positions", ViewControl::SaveComputerPositions, "device-floppy", "SaveComputerPositions" },
		{ "V3AM-045 About", ViewControl::About, "info-circle", "About" },
	};
	return rows;
}

using TeacherActionIcons::SecondaryControl;

// V3ap Screenshots-panel buttons: inventory row, role and expected Tabler glyph; both buttons keep their text labels
// V3aq appends the Slideshow-panel Previous/Next tool buttons, which keep their tooltips
// V3as appends the checkable Slideshow-panel separate-window toggle, which keeps its tooltip and one glyph in both states
// V3at appends the Spotlight-panel Add/Remove tool buttons, which keep their tooltips and one glyph in both states
// V3av appends the Collect Files destination browse and Open output directory buttons (one role), which share the
// Load positions folder-open in both states
struct SecondaryRow { const char* id; SecondaryControl control; const char* glyph; const char* role; };
const QList<SecondaryRow>& secondaryRows()
{
	static const QList<SecondaryRow> rows{
		{ "V3AM-046 Show", SecondaryControl::ShowScreenshot, "eye", "ShowScreenshot" },
		{ "V3AM-047 Delete", SecondaryControl::DeleteScreenshot, "trash", "DeleteScreenshot" },
		{ "V3AM-049 Previous", SecondaryControl::PreviousSlide, "player-skip-back", "PreviousSlide" },
		{ "V3AM-049 Next", SecondaryControl::NextSlide, "player-skip-forward", "NextSlide" },
		{ "V3AM-051 Separate window", SecondaryControl::SlideshowSeparateWindow, "maximize", "SlideshowSeparateWindow" },
		{ "V3AM-052 Add", SecondaryControl::SpotlightAdd, "device-desktop-plus", "SpotlightAdd" },
		{ "V3AM-052 Remove", SecondaryControl::SpotlightRemove, "device-desktop-minus", "SpotlightRemove" },
		{ "V3AM-058 Open destination", SecondaryControl::CollectFilesOpenDestination, "folder-open", "CollectFilesOpenDestination" },
	};
	return rows;
}

using TeacherActionIcons::RemoteAccessControl;

// V3aw Remote Access toolbar controls: inventory row, role, expected Off and On Tabler glyphs and role name; the
// View-only and Fullscreen toggles draw the command their current label offers (Off unchecked, On checked)
struct RemoteRow { const char* id; RemoteAccessControl control; const char* off; const char* on; const char* role; };
const QList<RemoteRow>& remoteRows()
{
	static const QList<RemoteRow> rows{
		{ "V3AM-064 View only / Remote control", RemoteAccessControl::ViewOnlyToggle, "eye", "pointer", "ViewOnlyToggle" },
		{ "V3AM-065 Select screen", RemoteAccessControl::SelectScreen, "devices-pc", "devices-pc", "SelectScreen" },
		{ "V3AM-066 Send shortcut", RemoteAccessControl::SendShortcut, "keyboard", "keyboard", "SendShortcut" },
		{ "V3AM-067 Screenshot", RemoteAccessControl::Screenshot, "camera", "camera", "Screenshot" },
		{ "V3AM-068 Fullscreen / Window", RemoteAccessControl::FullscreenToggle, "maximize", "app-window", "FullscreenToggle" },
		{ "V3AM-069 Exit", RemoteAccessControl::Exit, "x", "x", "Exit" },
	};
	return rows;
}

using TeacherActionIcons::FileTransferState;

// V3ax Send Files list (V3AM-072) and Collect Files tree (V3AM-073) transfer states: inventory rows, state, expected
// Tabler glyph and state name; every state draws one outline for both Off and On (Waiting is Collect Files only)
struct TransferRow { const char* id; FileTransferState state; const char* glyph; const char* role; };
const QList<TransferRow>& transferRows()
{
	static const QList<TransferRow> rows{
		{ "V3AM-072/073 Scheduled", FileTransferState::Scheduled, "clock", "Scheduled" },
		{ "V3AM-072/073 Transferring", FileTransferState::Transferring, "transfer", "Transferring" },
		{ "V3AM-073 Waiting", FileTransferState::Waiting, "hourglass", "Waiting" },
		{ "V3AM-072/073 Finished", FileTransferState::Finished, "circle-check", "Finished" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationPage;

// V3bb Administration navigation/page identities (V3AM-086 to 089 and the page portions of V3AM-098, 100, 102, 103,
// 105, 107 and 108): inventory row, role, expected Tabler glyph and the exact non-translated nextspaceActionIconRole
// value of the page root; every role draws one outline for both Off and On and the page titles remain the primary cues
struct AdministrationRow { const char* id; AdministrationPage page; const char* glyph; const char* role; };
const QList<AdministrationRow>& administrationRows()
{
	static const QList<AdministrationRow> rows{
		{ "V3AM-086 General", AdministrationPage::General, "settings", "General" },
		{ "V3AM-087 Student Agent", AdministrationPage::StudentAgent, "device-desktop", "StudentAgent" },
		{ "V3AM-088 Teacher Console", AdministrationPage::TeacherConsole, "presentation", "TeacherConsole" },
		{ "V3AM-089 Access control", AdministrationPage::AccessControl, "shield-lock", "AccessControl" },
		{ "V3AM-098 Authentication keys", AdministrationPage::AuthenticationKeys, "key", "AuthenticationKeys" },
		{ "V3AM-100 Locations & computers", AdministrationPage::LocationsAndComputers, "devices-pc", "LocationsAndComputers" },
		{ "V3AM-102 Broadcast", AdministrationPage::Broadcast, "screen-share", "Broadcast" },
		{ "V3AM-103 Applications & websites", AdministrationPage::ApplicationsAndWebsites, "apps", "ApplicationsAndWebsites" },
		{ "V3AM-105 File transfer", AdministrationPage::FileTransfer, "transfer", "FileTransfer" },
		{ "V3AM-107 Windows", AdministrationPage::Windows, "brand-windows", "Windows" },
		{ "V3AM-108 LDAP/AD", AdministrationPage::LdapDirectory, "hierarchy-2", "LdapDirectory" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationCommand;

// V3bc Administration (Configurator) menu commands (V3AM-090): inventory row, role, expected reviewed Tabler glyph and
// role name; every role draws one outline for both Off and On at the menus' logical 20 px, and the action labels remain
// the primary cues
struct CommandRow { const char* id; AdministrationCommand command; const char* glyph; const char* role; };
const QList<CommandRow>& commandRows()
{
	static const QList<CommandRow> rows{
		{ "V3AM-090 Save settings", AdministrationCommand::SaveSettings, "device-floppy", "SaveSettings" },
		{ "V3AM-090 Load settings", AdministrationCommand::LoadSettings, "folder-open", "LoadSettings" },
		{ "V3AM-090 About", AdministrationCommand::About, "info-circle", "About" },
		{ "V3AM-090 Reset configuration", AdministrationCommand::ResetConfiguration, "trash", "ResetConfiguration" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationAccessControlCommand;

// V3bd Administration (Configurator) Access Control page commands (V3AM-091): inventory row, role, expected Tabler glyph
// and role name; both Test buttons share the Test role, every role draws one outline for both Off and On at the page's
// logical 20 px, and the button labels and tooltips remain the primary cues
struct AccessControlCommandRow { const char* id; AdministrationAccessControlCommand command; const char* glyph; const char* role; };
const QList<AccessControlCommandRow>& accessControlCommandRows()
{
	static const QList<AccessControlCommandRow> rows{
		{ "V3AM-091 Test", AdministrationAccessControlCommand::Test, "circle-check", "Test" },
		{ "V3AM-091 Authorize selected group", AdministrationAccessControlCommand::AddAuthorizedGroup, "arrow-right", "AddAuthorizedGroup" },
		{ "V3AM-091 Remove selected authorized group", AdministrationAccessControlCommand::RemoveAuthorizedGroup, "arrow-left", "RemoveAuthorizedGroup" },
		{ "V3AM-091 Add rule", AdministrationAccessControlCommand::AddRule, "plus", "AddRule" },
		{ "V3AM-091 Remove rule", AdministrationAccessControlCommand::RemoveRule, "trash", "RemoveRule" },
		{ "V3AM-091 Edit rule", AdministrationAccessControlCommand::EditRule, "pencil", "EditRule" },
		{ "V3AM-091 Move rule up", AdministrationAccessControlCommand::MoveRuleUp, "arrow-up", "MoveRuleUp" },
		{ "V3AM-091 Move rule down", AdministrationAccessControlCommand::MoveRuleDown, "arrow-down", "MoveRuleDown" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationGeneralCommand;

// V3be Administration (Configurator) General page commands (V3AM-093): inventory row, role, expected reviewed Tabler
// glyph and role name; every role draws one outline for both Off and On at the page's logical 20 px, and the button
// labels and the log-directory button's tooltip remain the primary cues
struct GeneralCommandRow { const char* id; AdministrationGeneralCommand command; const char* glyph; const char* role; };
const QList<GeneralCommandRow>& generalCommandRows()
{
	static const QList<GeneralCommandRow> rows{
		{ "V3AM-093 Test authentication", AdministrationGeneralCommand::TestAuthentication, "circle-check", "TestAuthentication" },
		{ "V3AM-093 Open log file directory", AdministrationGeneralCommand::OpenLogDirectory, "folder-open", "OpenLogDirectory" },
		{ "V3AM-093 Clear all log files", AdministrationGeneralCommand::ClearLogFiles, "trash", "ClearLogFiles" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationTeacherConsoleCommand;

// V3bf Administration (Configurator) Teacher Console page commands (V3AM-094): inventory row, role, expected reviewed
// Tabler glyph and role name; the three directory roles share folder-open, Disable selected feature points right (All
// features to Disabled features) and Enable selected feature points left (back); every role draws one outline for both
// Off and On at the page's logical 20 px, and each icon-only button's tooltip and accessible name remain the primary cues
struct TeacherConsoleCommandRow { const char* id; AdministrationTeacherConsoleCommand command; const char* glyph; const char* role; };
const QList<TeacherConsoleCommandRow>& teacherConsoleCommandRows()
{
	static const QList<TeacherConsoleCommandRow> rows{
		{ "V3AM-094 Open user configuration directory", AdministrationTeacherConsoleCommand::OpenUserConfigurationDirectory, "folder-open", "OpenUserConfigurationDirectory" },
		{ "V3AM-094 Open screenshot directory", AdministrationTeacherConsoleCommand::OpenScreenshotDirectory, "folder-open", "OpenScreenshotDirectory" },
		{ "V3AM-094 Open configuration templates directory", AdministrationTeacherConsoleCommand::OpenConfigurationTemplatesDirectory, "folder-open", "OpenConfigurationTemplatesDirectory" },
		{ "V3AM-094 Disable selected feature", AdministrationTeacherConsoleCommand::DisableFeature, "arrow-right", "DisableFeature" },
		{ "V3AM-094 Enable selected feature", AdministrationTeacherConsoleCommand::EnableFeature, "arrow-left", "EnableFeature" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationStudentAgentCommand;

// V3bh Administration (Configurator) Student Agent page service commands (V3AM-096): inventory row, role, expected
// Tabler glyph and role name; Start service reuses the reviewed player-play of the Slideshow start/pause Off state and
// Stop service draws the pinned player-stop; both roles draw one outline for both Off and On at the page's logical
// 20 px, and the visible button labels remain the primary cues
struct StudentAgentCommandRow { const char* id; AdministrationStudentAgentCommand command; const char* glyph; const char* role; };
const QList<StudentAgentCommandRow>& studentAgentCommandRows()
{
	static const QList<StudentAgentCommandRow> rows{
		{ "V3AM-096 Start service", AdministrationStudentAgentCommand::StartService, "player-play", "StartService" },
		{ "V3AM-096 Stop service", AdministrationStudentAgentCommand::StopService, "player-stop", "StopService" },
	};
	return rows;
}

// V3bi service-control Toast icon masks (V3AM-097): inventory row, Toast icon, compiled resource, pinned Tabler glyph,
// role name, mask size in px and the size Toast draws it at in logical px (18 for the four state icons, 10 for the
// close button); each mask is a white, alpha-only raster made by tools/v3bi_toast_icons.py that Toast recolours
// (Toast::recolorImage() replaces RGB and keeps alpha), and every toast keeps its title and text as the primary cues
struct ToastRow { const char* id; Toast::Icon icon; const char* resource; const char* glyph; const char* role; int pixels; int logical; };
const QList<ToastRow>& toastRows()
{
	static const QList<ToastRow> rows{
		{ "V3AM-097 Success", Toast::Icon::Success, ":/core/toast-success.png", "circle-check", "Success", 196, 18 },
		{ "V3AM-097 Warning", Toast::Icon::Warning, ":/core/toast-warning.png", "alert-triangle", "Warning", 196, 18 },
		{ "V3AM-097 Error", Toast::Icon::Error, ":/core/toast-error.png", "circle-x", "Error", 196, 18 },
		{ "V3AM-097 Information", Toast::Icon::Information, ":/core/toast-information.png", "info-circle", "Information", 196, 18 },
		{ "V3AM-097 Close", Toast::Icon::Close, ":/core/toast-close.png", "x", "Close", 134, 10 },
	};
	return rows;
}

// whether the generated path data holds a glyph (a missing glyph fails a check instead of aborting the run)
bool hasGlyph( const char* name )
{
	for( const auto& glyph : glyphPaths )
	{
		if( qstrcmp( glyph.name, name ) == 0 )
		{
			return true;
		}
	}
	return false;
}

// the pinned glyph drawn by QPainter as a white mask of `pixels` px with the 2.1-unit render stroke and round caps/joins
QImage maskReference( const char* glyph, int pixels )
{
	QImage image( pixels, pixels, QImage::Format_ARGB32_Premultiplied );
	image.fill( Qt::transparent );
	QPainter painter( &image );
	painter.setRenderHint( QPainter::Antialiasing );
	painter.scale( qreal( pixels ) / qreal( ViewBoxSize ), qreal( pixels ) / qreal( ViewBoxSize ) );
	painter.setPen( QPen( Qt::white, qreal( RenderStrokeWidth ), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
	painter.setBrush( Qt::NoBrush );
	painter.drawPath( referencePath( QLatin1String( glyph ) ) );
	return image;
}

// the Toast::recolorImage() rule applied to a mask: every pixel takes the colour's RGB and keeps its own alpha
QImage recolouredMask( const QImage& mask, const QColor& colour )
{
	auto image = mask.convertToFormat( QImage::Format_ARGB32 );
	for( int row = 0; row < image.height(); ++row )
	{
		auto* p = reinterpret_cast<QRgb*>( image.scanLine( row ) );
		for( int col = 0; col < image.width(); ++col )
		{
			p[col] = qRgba( colour.red(), colour.green(), colour.blue(), qAlpha( p[col] ) );
		}
	}
	return image;
}

// a Toast mask as a contact-sheet icon: recoloured per mode with the palette colour the action icons use, the same
// pixmap for Off and On
QIcon toastIcon( const ToastRow& row, const QPalette& palette )
{
	const QImage mask( QLatin1String( row.resource ) );
	QIcon icon;
	for( const auto mode : Modes )
	{
		const auto pixmap = QPixmap::fromImage( recolouredMask( mask, expectedColour( palette, mode ) ) );
		icon.addPixmap( pixmap, mode, QIcon::Off );
		icon.addPixmap( pixmap, mode, QIcon::On );
	}
	return icon;
}

// empty when `shown` is `scaled` recoloured by Toast's rule: the same size and alpha everywhere, at least ten inked
// pixels, and every premultiplied channel within 1 of the colour scaled by that pixel's alpha, so no edge keeps a light
// or dark halo
QString recolourProblem( const QImage& shown, const QImage& scaled, const QColor& colour )
{
	const auto x = shown.convertToFormat( QImage::Format_ARGB32_Premultiplied );
	const auto y = scaled.convertToFormat( QImage::Format_ARGB32_Premultiplied );
	if( x.size() != y.size() )
	{
		return QStringLiteral("size %1x%2 instead of %3x%4").arg( x.width() ).arg( x.height() ).arg( y.width() ).arg( y.height() );
	}
	int inked = 0;
	for( int row = 0; row < x.height(); ++row )
	{
		const auto* p = reinterpret_cast<const QRgb*>( x.constScanLine( row ) );
		const auto* q = reinterpret_cast<const QRgb*>( y.constScanLine( row ) );
		for( int col = 0; col < x.width(); ++col )
		{
			const int alpha = qAlpha( p[col] );
			if( alpha != qAlpha( q[col] ) )
			{
				return QStringLiteral("alpha %1 instead of %2 at %3,%4").arg( alpha ).arg( qAlpha( q[col] ) ).arg( col ).arg( row );
			}
			if( alpha == 0 )
			{
				continue;
			}
			++inked;
			const auto off = [alpha]( int channel, int target ) { return qAbs( channel - target * alpha / 255.0 ) > 1.0; };
			if( off( qRed( p[col] ), colour.red() ) || off( qGreen( p[col] ), colour.green() ) || off( qBlue( p[col] ), colour.blue() ) )
			{
				return QStringLiteral("halo at %1,%2 (alpha %3)").arg( col ).arg( row ).arg( alpha );
			}
		}
	}
	return inked < 10 ? QStringLiteral("only %1 inked pixels").arg( inked ) : QString();
}

// fraction of pixels whose largest channel difference exceeds `threshold` (equally sized images)
qreal differingFraction( const QImage& a, const QImage& b, int threshold )
{
	const auto x = a.convertToFormat( QImage::Format_ARGB32_Premultiplied );
	const auto y = b.convertToFormat( QImage::Format_ARGB32_Premultiplied );
	int differing = 0;
	for( int row = 0; row < x.height(); ++row )
	{
		for( int col = 0; col < x.width(); ++col )
		{
			const auto p = x.pixel( col, row );
			const auto q = y.pixel( col, row );
			const int d = qMax( qMax( qAbs( qAlpha( p ) - qAlpha( q ) ), qAbs( qRed( p ) - qRed( q ) ) ),
								qMax( qAbs( qGreen( p ) - qGreen( q ) ), qAbs( qBlue( p ) - qBlue( q ) ) ) );
			differing += d > threshold ? 1 : 0;
		}
	}
	return qreal( differing ) / qreal( qMax( 1, x.width() * x.height() ) );
}

using NextSpaceActionIcons::AdministrationAuthenticationKeysCommand;

// V3bj Administration (Configurator) Authentication Keys page directory commands (V3AM-099): inventory row, role,
// expected reviewed Tabler glyph and role name; both roles reuse the reviewed folder-open and stay separate roles (the
// public and the private key directory); both draw one outline for both Off and On at the page's logical 20 px, and each
// icon-only button's tooltip and accessible name remain the primary cues
struct AuthenticationKeysCommandRow { const char* id; AdministrationAuthenticationKeysCommand command; const char* glyph; const char* role; };
const QList<AuthenticationKeysCommandRow>& authenticationKeysCommandRows()
{
	static const QList<AuthenticationKeysCommandRow> rows{
		{ "V3AM-099 Open public key directory", AdministrationAuthenticationKeysCommand::OpenPublicKeyDirectory, "folder-open", "OpenPublicKeyDirectory" },
		{ "V3AM-099 Open private key directory", AdministrationAuthenticationKeysCommand::OpenPrivateKeyDirectory, "folder-open", "OpenPrivateKeyDirectory" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationBuiltinDirectoryCommand;

// V3bk Administration (Configurator) built-in directory Locations & computers page commands (V3AM-101): inventory row,
// role, expected reviewed Tabler glyph and role name; the four location roles come first and the four computer roles
// follow in the same command order; location and computer roles stay separate although each pair reuses the same
// reviewed plus, trash, arrow-up or arrow-down; every role draws one outline for both Off and On at the page's logical
// 20 px, and each icon-only button's tooltip and accessible name remain the primary cues
struct BuiltinDirectoryCommandRow { const char* id; AdministrationBuiltinDirectoryCommand command; const char* glyph; const char* role; };
const QList<BuiltinDirectoryCommandRow>& builtinDirectoryCommandRows()
{
	static const QList<BuiltinDirectoryCommandRow> rows{
		{ "V3AM-101 Add new location", AdministrationBuiltinDirectoryCommand::AddLocation, "plus", "AddLocation" },
		{ "V3AM-101 Remove selected location", AdministrationBuiltinDirectoryCommand::RemoveLocation, "trash", "RemoveLocation" },
		{ "V3AM-101 Move selected location up", AdministrationBuiltinDirectoryCommand::MoveLocationUp, "arrow-up", "MoveLocationUp" },
		{ "V3AM-101 Move selected location down", AdministrationBuiltinDirectoryCommand::MoveLocationDown, "arrow-down", "MoveLocationDown" },
		{ "V3AM-101 Add new computer", AdministrationBuiltinDirectoryCommand::AddComputer, "plus", "AddComputer" },
		{ "V3AM-101 Remove selected computer", AdministrationBuiltinDirectoryCommand::RemoveComputer, "trash", "RemoveComputer" },
		{ "V3AM-101 Move selected computer up", AdministrationBuiltinDirectoryCommand::MoveComputerUp, "arrow-up", "MoveComputerUp" },
		{ "V3AM-101 Move selected computer down", AdministrationBuiltinDirectoryCommand::MoveComputerDown, "arrow-down", "MoveComputerDown" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationDesktopServicesCommand;

// V3bl Administration (Configurator) Desktop Services page commands, shown as Applications & websites (V3AM-104):
// inventory row, role, expected reviewed Tabler glyph and role name; the two application roles come first and the two
// website roles follow in the same command order; application and website roles stay separate although each pair reuses
// the same reviewed plus or trash; every role draws one outline for both Off and On at the page's logical 20 px, and
// each icon-only button's tooltip and accessible name remain the primary cues
struct DesktopServicesCommandRow { const char* id; AdministrationDesktopServicesCommand command; const char* glyph; const char* role; };
const QList<DesktopServicesCommandRow>& desktopServicesCommandRows()
{
	static const QList<DesktopServicesCommandRow> rows{
		{ "V3AM-104 Add new application", AdministrationDesktopServicesCommand::AddApplication, "plus", "AddApplication" },
		{ "V3AM-104 Remove selected application", AdministrationDesktopServicesCommand::RemoveApplication, "trash", "RemoveApplication" },
		{ "V3AM-104 Add new website", AdministrationDesktopServicesCommand::AddWebsite, "plus", "AddWebsite" },
		{ "V3AM-104 Remove selected website", AdministrationDesktopServicesCommand::RemoveWebsite, "trash", "RemoveWebsite" },
	};
	return rows;
}

using NextSpaceActionIcons::AdministrationFileTransferCommand;

// V3bm Administration (Configurator) File transfer page directory commands (V3AM-106): inventory row with the exact
// tooltip/accessible name, role, expected reviewed Tabler glyph and role name, in the page's command order (default
// source, transfer destination, files to collect source, collected files destination); the four roles stay separate
// although all reuse the reviewed folder-open; every role draws one outline for both Off and On at the page's logical
// 20 px, and each icon-only button's tooltip and accessible name remain the primary cues
struct FileTransferCommandRow { const char* id; AdministrationFileTransferCommand command; const char* glyph; const char* role; };
const QList<FileTransferCommandRow>& fileTransferCommandRows()
{
	static const QList<FileTransferCommandRow> rows{
		{ "V3AM-106 Browse for default source directory", AdministrationFileTransferCommand::BrowseDefaultSourceDirectory, "folder-open", "BrowseDefaultSourceDirectory" },
		{ "V3AM-106 Browse for file transfer destination directory", AdministrationFileTransferCommand::BrowseDestinationDirectory, "folder-open", "BrowseDestinationDirectory" },
		{ "V3AM-106 Browse for files to collect source directory", AdministrationFileTransferCommand::BrowseFilesToCollectSourceDirectory, "folder-open", "BrowseFilesToCollectSourceDirectory" },
		{ "V3AM-106 Browse for collected files destination directory", AdministrationFileTransferCommand::BrowseCollectedFilesDestinationDirectory, "folder-open", "BrowseCollectedFilesDestinationDirectory" },
	};
	return rows;
}

// the Configurator page selector's unchanged icon size (configurator/src/MainWindow.ui iconSize 48x48)
constexpr int SelectorSize = 48;

QImage renderAt( const QIcon& icon, int size, qreal dpr, QIcon::Mode mode, QIcon::State state, int pad = 0 )
{
	auto image = canvas( dpr, QSize( size + 2 * pad, size + 2 * pad ) );
	QPainter painter( &image );
	icon.paint( &painter, QRect( pad, pad, size, size ), Qt::AlignCenter, mode, state );
	return image;
}

QImage referenceAt( const QString& spec, const QColor& color, int size, qreal dpr )
{
	auto image = canvas( dpr, QSize( size, size ) );
	QPainter painter( &image );
	painter.setRenderHint( QPainter::Antialiasing );
	painter.scale( qreal( size ) / qreal( ViewBoxSize ), qreal( size ) / qreal( ViewBoxSize ) );
	painter.setPen( QPen( color, qreal( RenderStrokeWidth ), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
	painter.setBrush( Qt::NoBrush );
	painter.drawPath( referencePath( spec ) );
	return image;
}

// highest alpha of an unpadded render of the given logical size (ARGB32) inside a rectangle given in 24-unit view-box
// coordinates
int maxAlphaAt( const QImage& image, int size, qreal dpr, const QRectF& units )
{
	const auto toDevice = [size, dpr]( qreal unit ) { return unit * size / qreal( ViewBoxSize ) * dpr; };
	int result = 0;
	for( int row = qFloor( toDevice( units.top() ) ); row < qCeil( toDevice( units.bottom() ) ); ++row )
	{
		for( int col = qFloor( toDevice( units.left() ) ); col < qCeil( toDevice( units.right() ) ); ++col )
		{
			result = qMax( result, qAlpha( image.pixel( col, row ) ) );
		}
	}
	return result;
}

// one entry per distinct icon-bearing inventory treatment; Lock and Broadcast also in their On state;
// V3ao view controls use parentKind "view" and their role name
// V3ap Screenshots-panel buttons use parentKind "secondary" and their role name
// V3aq Slideshow-panel Previous/Next also use parentKind "secondary" and their role name
// V3ar Slideshow-panel start/pause uses parentKind "secondary" twice: Start in its Off and Pause in its On state
// V3as Slideshow-panel separate-window toggle uses parentKind "secondary" and its role name
// V3at Spotlight-panel Add/Remove use parentKind "secondary" and their role names; the realtime toggle appears twice:
// in its Off and in its On state
// V3av Collect Files destination actions use parentKind "secondary" and their role name, appended last
// V3aw Remote Access toolbar controls, appended after V3av, use parentKind "remote" and their role name; the
// View-only and Fullscreen toggles appear twice: in their Off and in their On state
// V3ax Send/Collect transfer states, appended after V3aw, use parentKind "transfer" and their state name
// V3bb Administration page identities, appended after V3ax, use parentKind "admin" and their role name; like every
// other column they are drawn at the sheet's logical 20 px (administrationPages() covers the selector's 48 px)
// V3bc Administration menu commands, appended after V3bb, use parentKind "command" and their role name
// V3bd Administration Access Control commands, appended after V3bc, use parentKind "access" and their role name
// V3be Administration General page commands, appended after V3bd, use parentKind "general" and their role name
// V3bf Administration Teacher Console page commands, appended after V3be, use parentKind "console" and their role name
// V3bh Administration Student Agent page service commands, appended after V3bf, use parentKind "agent" and their role name
// V3bi service-control Toast icon masks, appended after V3bh, use parentKind "toast" and their role name; they are
// drawn from the recoloured raster masks the product compiles, not by the action-icon renderer
// V3bj Administration Authentication Keys page directory commands, appended after V3bi, use parentKind "authkeys"
// and their role name
// V3bk Administration Locations & computers page commands, appended after V3bj, use parentKind "builtindirectory"
// and their role name
// V3bl Administration Applications & websites page commands, appended after V3bk, use parentKind "desktopservices"
// and their role name
// V3bm Administration File transfer page directory commands, appended after V3bl, use parentKind "filetransfer"
// and their role name
struct SheetEntry { QString label; QString feature; QString parentKind; QIcon::State state; };
const QList<SheetEntry>& sheetEntries()
{
	static const QList<SheetEntry> entries{
		{ QStringLiteral("Monitor"), QStringLiteral("MonitoringMode"), {}, QIcon::Off },
		{ QStringLiteral("Broadcast"), QStringLiteral("Demo"), {}, QIcon::Off },
		{ QStringLiteral("Stop Broadcast (On)"), QStringLiteral("Demo"), {}, QIcon::On },
		{ QStringLiteral("Lock"), QStringLiteral("ScreenLock"), {}, QIcon::Off },
		{ QStringLiteral("Unlock (Lock On)"), QStringLiteral("ScreenLock"), {}, QIcon::On },
		{ QStringLiteral("Remote Control"), QStringLiteral("RemoteControl"), {}, QIcon::Off },
		{ QStringLiteral("Message"), QStringLiteral("TextMessage"), {}, QIcon::Off },
		{ QStringLiteral("Send Files"), QStringLiteral("DistributeFiles"), {}, QIcon::Off },
		{ QStringLiteral("Remote View"), QStringLiteral("RemoteView"), {}, QIcon::Off },
		{ QStringLiteral("Collect Files"), QStringLiteral("FileCollect"), {}, QIcon::Off },
		{ QStringLiteral("Screenshot"), QStringLiteral("Screenshot"), {}, QIcon::Off },
		{ QStringLiteral("Start Application"), QStringLiteral("StartApp"), {}, QIcon::Off },
		{ QStringLiteral("Open Website"), QStringLiteral("OpenWebsite"), {}, QIcon::Off },
		{ QStringLiteral("Power On"), QStringLiteral("PowerOn"), {}, QIcon::Off },
		{ QStringLiteral("Reboot"), QStringLiteral("Reboot"), {}, QIcon::Off },
		{ QStringLiteral("Power Down"), QStringLiteral("PowerDown"), {}, QIcon::Off },
		{ QStringLiteral("Log In"), QStringLiteral("UserLogin"), {}, QIcon::Off },
		{ QStringLiteral("Log Off"), QStringLiteral("UserLogoff"), {}, QIcon::Off },
		{ QStringLiteral("Own screen fullscreen"), QStringLiteral("ShareOwnScreenFullScreen"), {}, QIcon::Off },
		{ QStringLiteral("Own screen window"), QStringLiteral("ShareOwnScreenWindow"), {}, QIcon::Off },
		{ QStringLiteral("User screen fullscreen"), QStringLiteral("ShareUserScreenFullScreen"), {}, QIcon::Off },
		{ QStringLiteral("User screen window"), QStringLiteral("ShareUserScreenWindow"), {}, QIcon::Off },
		{ QStringLiteral("More"), QStringLiteral("MoreActionsButton"), {}, QIcon::Off },
		{ QStringLiteral("Custom application/website"), QStringLiteral("StartApp"), QStringLiteral("custom"), QIcon::Off },
		{ QStringLiteral("Locations & computers"), QStringLiteral("ComputerSelectPanel"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Screenshots panel"), QStringLiteral("ScreenshotManagementPanel"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Slideshow panel"), QStringLiteral("SlideshowPanel"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Spotlight panel"), QStringLiteral("SpotlightPanel"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Powered-on filter"), QStringLiteral("PoweredOnFilter"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Logged-on users filter"), QStringLiteral("LoggedOnUsersFilter"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Auto thumbnail size"), QStringLiteral("AutoAdjustIconSize"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Custom positions"), QStringLiteral("CustomComputerPositions"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Align to grid"), QStringLiteral("AlignComputers"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Load positions"), QStringLiteral("LoadComputerPositions"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Save positions"), QStringLiteral("SaveComputerPositions"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("About"), QStringLiteral("About"), QStringLiteral("view"), QIcon::Off },
		{ QStringLiteral("Screenshots: Show"), QStringLiteral("ShowScreenshot"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Screenshots: Delete"), QStringLiteral("DeleteScreenshot"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Slideshow: Previous"), QStringLiteral("PreviousSlide"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Slideshow: Next"), QStringLiteral("NextSlide"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Slideshow: Start (Off)"), QStringLiteral("SlideshowStartPause"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Slideshow: Pause (On)"), QStringLiteral("SlideshowStartPause"), QStringLiteral("secondary"), QIcon::On },
		{ QStringLiteral("Slideshow: Separate window"), QStringLiteral("SlideshowSeparateWindow"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Spotlight: Add"), QStringLiteral("SpotlightAdd"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Spotlight: Remove"), QStringLiteral("SpotlightRemove"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Spotlight: Realtime (Off)"), QStringLiteral("SpotlightRealtime"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Spotlight: Realtime (On)"), QStringLiteral("SpotlightRealtime"), QStringLiteral("secondary"), QIcon::On },
		{ QStringLiteral("Collect Files: Open destination"), QStringLiteral("CollectFilesOpenDestination"), QStringLiteral("secondary"), QIcon::Off },
		{ QStringLiteral("Remote Access: View only (Off)"), QStringLiteral("ViewOnlyToggle"), QStringLiteral("remote"), QIcon::Off },
		{ QStringLiteral("Remote Access: Remote control (On)"), QStringLiteral("ViewOnlyToggle"), QStringLiteral("remote"), QIcon::On },
		{ QStringLiteral("Remote Access: Select screen"), QStringLiteral("SelectScreen"), QStringLiteral("remote"), QIcon::Off },
		{ QStringLiteral("Remote Access: Send shortcut"), QStringLiteral("SendShortcut"), QStringLiteral("remote"), QIcon::Off },
		{ QStringLiteral("Remote Access: Screenshot"), QStringLiteral("Screenshot"), QStringLiteral("remote"), QIcon::Off },
		{ QStringLiteral("Remote Access: Fullscreen (Off)"), QStringLiteral("FullscreenToggle"), QStringLiteral("remote"), QIcon::Off },
		{ QStringLiteral("Remote Access: Window (On)"), QStringLiteral("FullscreenToggle"), QStringLiteral("remote"), QIcon::On },
		{ QStringLiteral("Remote Access: Exit"), QStringLiteral("Exit"), QStringLiteral("remote"), QIcon::Off },
		{ QStringLiteral("Transfer: Scheduled"), QStringLiteral("Scheduled"), QStringLiteral("transfer"), QIcon::Off },
		{ QStringLiteral("Transfer: Transferring"), QStringLiteral("Transferring"), QStringLiteral("transfer"), QIcon::Off },
		{ QStringLiteral("Transfer: Waiting"), QStringLiteral("Waiting"), QStringLiteral("transfer"), QIcon::Off },
		{ QStringLiteral("Transfer: Finished"), QStringLiteral("Finished"), QStringLiteral("transfer"), QIcon::Off },
		{ QStringLiteral("Administration: General"), QStringLiteral("General"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Student Agent"), QStringLiteral("StudentAgent"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Teacher Console"), QStringLiteral("TeacherConsole"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Access control"), QStringLiteral("AccessControl"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Authentication keys"), QStringLiteral("AuthenticationKeys"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Locations & computers"), QStringLiteral("LocationsAndComputers"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Broadcast (Demo server)"), QStringLiteral("Broadcast"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Applications & websites"), QStringLiteral("ApplicationsAndWebsites"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: File transfer"), QStringLiteral("FileTransfer"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Windows"), QStringLiteral("Windows"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: LDAP/AD"), QStringLiteral("LdapDirectory"), QStringLiteral("admin"), QIcon::Off },
		{ QStringLiteral("Administration: Save settings"), QStringLiteral("SaveSettings"), QStringLiteral("command"), QIcon::Off },
		{ QStringLiteral("Administration: Load settings"), QStringLiteral("LoadSettings"), QStringLiteral("command"), QIcon::Off },
		{ QStringLiteral("Administration: About"), QStringLiteral("About"), QStringLiteral("command"), QIcon::Off },
		{ QStringLiteral("Administration: Reset configuration"), QStringLiteral("ResetConfiguration"), QStringLiteral("command"), QIcon::Off },
		{ QStringLiteral("Access control: Test"), QStringLiteral("Test"), QStringLiteral("access"), QIcon::Off },
		{ QStringLiteral("Access control: Authorize group"), QStringLiteral("AddAuthorizedGroup"), QStringLiteral("access"), QIcon::Off },
		{ QStringLiteral("Access control: Remove authorized group"), QStringLiteral("RemoveAuthorizedGroup"), QStringLiteral("access"), QIcon::Off },
		{ QStringLiteral("Access control: Add rule"), QStringLiteral("AddRule"), QStringLiteral("access"), QIcon::Off },
		{ QStringLiteral("Access control: Remove rule"), QStringLiteral("RemoveRule"), QStringLiteral("access"), QIcon::Off },
		{ QStringLiteral("Access control: Edit rule"), QStringLiteral("EditRule"), QStringLiteral("access"), QIcon::Off },
		{ QStringLiteral("Access control: Move rule up"), QStringLiteral("MoveRuleUp"), QStringLiteral("access"), QIcon::Off },
		{ QStringLiteral("Access control: Move rule down"), QStringLiteral("MoveRuleDown"), QStringLiteral("access"), QIcon::Off },
		{ QStringLiteral("General: Test authentication"), QStringLiteral("TestAuthentication"), QStringLiteral("general"), QIcon::Off },
		{ QStringLiteral("General: Open log directory"), QStringLiteral("OpenLogDirectory"), QStringLiteral("general"), QIcon::Off },
		{ QStringLiteral("General: Clear log files"), QStringLiteral("ClearLogFiles"), QStringLiteral("general"), QIcon::Off },
		{ QStringLiteral("Teacher Console: Open user configuration directory"), QStringLiteral("OpenUserConfigurationDirectory"), QStringLiteral("console"), QIcon::Off },
		{ QStringLiteral("Teacher Console: Open screenshot directory"), QStringLiteral("OpenScreenshotDirectory"), QStringLiteral("console"), QIcon::Off },
		{ QStringLiteral("Teacher Console: Open configuration templates directory"), QStringLiteral("OpenConfigurationTemplatesDirectory"), QStringLiteral("console"), QIcon::Off },
		{ QStringLiteral("Teacher Console: Disable selected feature"), QStringLiteral("DisableFeature"), QStringLiteral("console"), QIcon::Off },
		{ QStringLiteral("Teacher Console: Enable selected feature"), QStringLiteral("EnableFeature"), QStringLiteral("console"), QIcon::Off },
		{ QStringLiteral("Student Agent: Start service"), QStringLiteral("StartService"), QStringLiteral("agent"), QIcon::Off },
		{ QStringLiteral("Student Agent: Stop service"), QStringLiteral("StopService"), QStringLiteral("agent"), QIcon::Off },
		{ QStringLiteral("Toast: Success"), QStringLiteral("Success"), QStringLiteral("toast"), QIcon::Off },
		{ QStringLiteral("Toast: Warning"), QStringLiteral("Warning"), QStringLiteral("toast"), QIcon::Off },
		{ QStringLiteral("Toast: Error"), QStringLiteral("Error"), QStringLiteral("toast"), QIcon::Off },
		{ QStringLiteral("Toast: Information"), QStringLiteral("Information"), QStringLiteral("toast"), QIcon::Off },
		{ QStringLiteral("Toast: Close"), QStringLiteral("Close"), QStringLiteral("toast"), QIcon::Off },
		{ QStringLiteral("Authentication Keys: Open public key directory"), QStringLiteral("OpenPublicKeyDirectory"), QStringLiteral("authkeys"), QIcon::Off },
		{ QStringLiteral("Authentication Keys: Open private key directory"), QStringLiteral("OpenPrivateKeyDirectory"), QStringLiteral("authkeys"), QIcon::Off },
		{ QStringLiteral("Locations & computers: Add location"), QStringLiteral("AddLocation"), QStringLiteral("builtindirectory"), QIcon::Off },
		{ QStringLiteral("Locations & computers: Remove location"), QStringLiteral("RemoveLocation"), QStringLiteral("builtindirectory"), QIcon::Off },
		{ QStringLiteral("Locations & computers: Move location up"), QStringLiteral("MoveLocationUp"), QStringLiteral("builtindirectory"), QIcon::Off },
		{ QStringLiteral("Locations & computers: Move location down"), QStringLiteral("MoveLocationDown"), QStringLiteral("builtindirectory"), QIcon::Off },
		{ QStringLiteral("Locations & computers: Add computer"), QStringLiteral("AddComputer"), QStringLiteral("builtindirectory"), QIcon::Off },
		{ QStringLiteral("Locations & computers: Remove computer"), QStringLiteral("RemoveComputer"), QStringLiteral("builtindirectory"), QIcon::Off },
		{ QStringLiteral("Locations & computers: Move computer up"), QStringLiteral("MoveComputerUp"), QStringLiteral("builtindirectory"), QIcon::Off },
		{ QStringLiteral("Locations & computers: Move computer down"), QStringLiteral("MoveComputerDown"), QStringLiteral("builtindirectory"), QIcon::Off },
		{ QStringLiteral("Applications & websites: Add application"), QStringLiteral("AddApplication"), QStringLiteral("desktopservices"), QIcon::Off },
		{ QStringLiteral("Applications & websites: Remove application"), QStringLiteral("RemoveApplication"), QStringLiteral("desktopservices"), QIcon::Off },
		{ QStringLiteral("Applications & websites: Add website"), QStringLiteral("AddWebsite"), QStringLiteral("desktopservices"), QIcon::Off },
		{ QStringLiteral("Applications & websites: Remove website"), QStringLiteral("RemoveWebsite"), QStringLiteral("desktopservices"), QIcon::Off },
		{ QStringLiteral("File transfer: Browse default source directory"), QStringLiteral("BrowseDefaultSourceDirectory"), QStringLiteral("filetransfer"), QIcon::Off },
		{ QStringLiteral("File transfer: Browse destination directory"), QStringLiteral("BrowseDestinationDirectory"), QStringLiteral("filetransfer"), QIcon::Off },
		{ QStringLiteral("File transfer: Browse files to collect source directory"), QStringLiteral("BrowseFilesToCollectSourceDirectory"), QStringLiteral("filetransfer"), QIcon::Off },
		{ QStringLiteral("File transfer: Browse collected files destination directory"), QStringLiteral("BrowseCollectedFilesDestinationDirectory"), QStringLiteral("filetransfer"), QIcon::Off },
	};
	return entries;
}

QIcon iconFor( const QString& name, const QString& parentKind, const QPalette& palette )
{
	if( parentKind == QStringLiteral("authkeys") )
	{
		for( const auto& row : authenticationKeysCommandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration Authentication Keys command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("builtindirectory") )
	{
		for( const auto& row : builtinDirectoryCommandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration Locations & computers command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("desktopservices") )
	{
		for( const auto& row : desktopServicesCommandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationDesktopServicesCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration Applications & websites command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("filetransfer") )
	{
		for( const auto& row : fileTransferCommandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationFileTransferCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration File transfer command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("toast") )
	{
		for( const auto& row : toastRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return toastIcon( row, palette );
			}
		}
		qFatal( "unknown Toast icon %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("agent") )
	{
		for( const auto& row : studentAgentCommandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationStudentAgentCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration Student Agent command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("console") )
	{
		for( const auto& row : teacherConsoleCommandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration Teacher Console command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("general") )
	{
		for( const auto& row : generalCommandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationGeneralCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration General command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("access") )
	{
		for( const auto& row : accessControlCommandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationAccessControlCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration Access Control command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("command") )
	{
		for( const auto& row : commandRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationCommand( row.command, palette );
			}
		}
		qFatal( "unknown Administration command %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("admin") )
	{
		for( const auto& row : administrationRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return NextSpaceActionIcons::forAdministrationPage( row.page, palette );
			}
		}
		qFatal( "unknown Administration page %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("transfer") )
	{
		for( const auto& row : transferRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return TeacherActionIcons::forFileTransferState( row.state, palette );
			}
		}
		qFatal( "unknown file-transfer state %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("remote") )
	{
		for( const auto& row : remoteRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return TeacherActionIcons::forRemoteAccessControl( row.control, palette );
			}
		}
		qFatal( "unknown Remote Access control %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("secondary") )
	{
		// V3ar/V3at: the two stateful secondary roles; they are not in secondaryRows() because their Off and On glyphs differ
		if( name == QStringLiteral("SlideshowStartPause") )
		{
			return TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, palette );
		}
		if( name == QStringLiteral("SpotlightRealtime") )
		{
			return TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRealtime, palette );
		}
		for( const auto& row : secondaryRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return TeacherActionIcons::forSecondaryControl( row.control, palette );
			}
		}
		qFatal( "unknown secondary control %s", qPrintable( name ) );
	}
	if( parentKind == QStringLiteral("view") )
	{
		for( const auto& row : viewRows() )
		{
			if( QLatin1String( row.role ) == name )
			{
				return TeacherActionIcons::forViewControl( row.control, palette );
			}
		}
		qFatal( "unknown view control %s", qPrintable( name ) );
	}
	if( name == QStringLiteral("MoreActionsButton") )
	{
		return TeacherActionIcons::more( palette );
	}
	if( parentKind == QStringLiteral("custom") )
	{
		return TeacherActionIcons::forFeature( makeFeature( name, QUuid::createUuid(), QStringLiteral(":/core/document-edit.png") ), palette );
	}
	if( parentKind == QStringLiteral("predefined") || parentKind == QStringLiteral("child") )
	{
		return TeacherActionIcons::forFeature( makeFeature( name, QUuid::createUuid() ), palette );
	}
	return TeacherActionIcons::forFeature( makeFeature( name ), palette );
}

QImage contactSheet( qreal dpr )
{
	const auto& entries = sheetEntries();
	const int cell = 32;
	const int labelWidth = 104;
	const int titleHeight = 40;
	const int bandHeight = 18 + 4 * cell + 8;
	const int legendRows = ( entries.size() + 3 ) / 4;
	const QSize logical( labelWidth + entries.size() * cell + 16, titleHeight + 3 * bandHeight + 16 + legendRows * 16 + 8 );

	auto image = canvas( dpr, logical );
	image.fill( QColor( 0xe5, 0xe7, 0xeb ) );
	QPainter painter( &image );
	auto font = painter.font();
	font.setPixelSize( 11 );
	painter.setFont( font );
	painter.setPen( Qt::black );
	painter.drawText( QRect( 8, 4, logical.width() - 16, 16 ), Qt::AlignLeft | Qt::AlignVCenter,
					  QStringLiteral("V3an/V3ao/V3ap/V3aq/V3ar/V3as/V3at/V3av/V3aw/V3ax/V3bb/V3bc/V3bd/V3be/V3bf/V3bh/V3bi/V3bj/V3bk/V3bl/V3bm Teacher and Administration action icons, Tabler Icons outline v3.49.0; device scale %1; logical 20 px")
						  .arg( dpr, 0, 'f', 2 ) );
	painter.drawText( QRect( 8, 20, logical.width() - 16, 16 ), Qt::AlignLeft | Qt::AlignVCenter,
					  QStringLiteral("Offscreen Qt rendering on synthetic palettes; Checked = On state on a synthetic checked panel. "
								 "NOT native Windows, system high-contrast or live evidence.") );

	int y = titleHeight;
	for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
	{
		const auto palette = syntheticPalette( kind );
		painter.fillRect( QRect( 0, y, logical.width(), bandHeight ), palette.color( QPalette::Window ) );
		painter.setPen( palette.color( QPalette::WindowText ) );
		painter.drawText( QRect( 8, y + 2, labelWidth + 200, 14 ), Qt::AlignLeft | Qt::AlignVCenter,
						  QStringLiteral("%1 palette").arg( paletteName( kind ) ) );
		for( int c = 0; c < entries.size(); ++c )
		{
			painter.drawText( QRect( labelWidth + c * cell, y + 2, cell, 14 ), Qt::AlignCenter, QString::number( c + 1 ) );
		}
		// the Checked row paints the On state on a synthetic checked-button panel (not the native style)
		const QList<QPair<QIcon::Mode, QString>> rows{ { QIcon::Normal, QStringLiteral("Normal") },
													   { QIcon::Disabled, QStringLiteral("Disabled") },
													   { QIcon::Selected, QStringLiteral("Selected") },
													   { QIcon::Normal, QStringLiteral("Checked") } };
		for( int r = 0; r < rows.size(); ++r )
		{
			const int rowY = y + 18 + r * cell;
			painter.setPen( palette.color( QPalette::WindowText ) );
			painter.drawText( QRect( 8, rowY, labelWidth - 8, cell ), Qt::AlignLeft | Qt::AlignVCenter, rows[r].second );
			for( int c = 0; c < entries.size(); ++c )
			{
				const QRect cellRect( labelWidth + c * cell, rowY, cell, cell );
				const bool checked = rows[r].second == QStringLiteral("Checked");
				if( rows[r].first == QIcon::Selected )
				{
					painter.fillRect( cellRect.adjusted( 2, 2, -2, -2 ), palette.color( QPalette::Highlight ) );
				}
				if( checked )
				{
					auto panel = palette.color( QPalette::Highlight );
					panel.setAlpha( 56 );
					painter.fillRect( cellRect.adjusted( 2, 2, -2, -2 ), panel );
					painter.setPen( palette.color( QPalette::Highlight ) );
					painter.drawRect( cellRect.adjusted( 2, 2, -3, -3 ) );
				}
				const auto icon = iconFor( entries[c].feature, entries[c].parentKind, palette );
				icon.paint( &painter, QRect( cellRect.x() + 6, cellRect.y() + 6, LogicalSize, LogicalSize ),
							Qt::AlignCenter, rows[r].first, checked ? QIcon::On : entries[c].state );
			}
		}
		y += bandHeight;
	}
	painter.setPen( Qt::black );
	y += 8;
	for( int c = 0; c < entries.size(); ++c )
	{
		const int col = c / legendRows;
		const int row = c % legendRows;
		painter.drawText( QRect( 8 + col * ( logical.width() / 4 ), y + row * 16, logical.width() / 4, 16 ),
						  Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("%1  %2").arg( c + 1 ).arg( entries[c].label ) );
	}
	painter.end();
	return image;
}

// V3ba: the neutral facade names the existing API itself, not a copy
static_assert( std::is_same_v<NextSpaceActionIcons::ViewControl, TeacherActionIcons::ViewControl> );
static_assert( std::is_same_v<NextSpaceActionIcons::SecondaryControl, TeacherActionIcons::SecondaryControl> );
static_assert( std::is_same_v<NextSpaceActionIcons::RemoteAccessControl, TeacherActionIcons::RemoteAccessControl> );
static_assert( std::is_same_v<NextSpaceActionIcons::FileTransferState, TeacherActionIcons::FileTransferState> );
// (compared through a constexpr helper: a direct self-comparison trips -Werror=tautological-compare)
template<typename Function>
constexpr bool sameFunction( Function a, Function b )
{
	return a == b;
}
static_assert( sameFunction( &NextSpaceActionIcons::forFeature, &TeacherActionIcons::forFeature ) );
static_assert( sameFunction( &NextSpaceActionIcons::more, &TeacherActionIcons::more ) );
static_assert( sameFunction( &NextSpaceActionIcons::forViewControl, &TeacherActionIcons::forViewControl ) );
static_assert( sameFunction( &NextSpaceActionIcons::forSecondaryControl, &TeacherActionIcons::forSecondaryControl ) );
static_assert( sameFunction( &NextSpaceActionIcons::forRemoteAccessControl, &TeacherActionIcons::forRemoteAccessControl ) );
static_assert( sameFunction( &NextSpaceActionIcons::forFileTransferState, &TeacherActionIcons::forFileTransferState ) );
// V3bb: the Administration page roles are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationPage, TeacherActionIcons::AdministrationPage> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationPage, &TeacherActionIcons::forAdministrationPage ) );
// V3bc: the Administration menu commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationCommand, TeacherActionIcons::AdministrationCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationCommand, &TeacherActionIcons::forAdministrationCommand ) );
// V3bd: the Administration Access Control commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationAccessControlCommand, TeacherActionIcons::AdministrationAccessControlCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationAccessControlCommand, &TeacherActionIcons::forAdministrationAccessControlCommand ) );
// V3be: the Administration General page commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationGeneralCommand, TeacherActionIcons::AdministrationGeneralCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationGeneralCommand, &TeacherActionIcons::forAdministrationGeneralCommand ) );
// V3bf: the Administration Teacher Console page commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationTeacherConsoleCommand, TeacherActionIcons::AdministrationTeacherConsoleCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationTeacherConsoleCommand, &TeacherActionIcons::forAdministrationTeacherConsoleCommand ) );
// V3bh: the Administration Student Agent page service commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationStudentAgentCommand, TeacherActionIcons::AdministrationStudentAgentCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationStudentAgentCommand, &TeacherActionIcons::forAdministrationStudentAgentCommand ) );
// V3bj: the Administration Authentication Keys page directory commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationAuthenticationKeysCommand, TeacherActionIcons::AdministrationAuthenticationKeysCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand, &TeacherActionIcons::forAdministrationAuthenticationKeysCommand ) );
// V3bk: the Administration Locations & computers page commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationBuiltinDirectoryCommand, TeacherActionIcons::AdministrationBuiltinDirectoryCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand, &TeacherActionIcons::forAdministrationBuiltinDirectoryCommand ) );
// V3bl: the Administration Applications & websites page commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationDesktopServicesCommand, TeacherActionIcons::AdministrationDesktopServicesCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationDesktopServicesCommand, &TeacherActionIcons::forAdministrationDesktopServicesCommand ) );
// V3bm: the Administration File transfer page directory commands are part of the same shared API
static_assert( std::is_same_v<NextSpaceActionIcons::AdministrationFileTransferCommand, TeacherActionIcons::AdministrationFileTransferCommand> );
static_assert( sameFunction( &NextSpaceActionIcons::forAdministrationFileTransferCommand, &TeacherActionIcons::forAdministrationFileTransferCommand ) );
// V3bi: the Toast API keeps its preset and icon order (V3AM-097 replaces only the five raster masks)
static_assert( int( Toast::Preset::Success ) == 0 && int( Toast::Preset::Warning ) == 1 && int( Toast::Preset::Error ) == 2 && int( Toast::Preset::Information ) == 3 );
static_assert( int( Toast::Icon::Success ) == 0 && int( Toast::Icon::Warning ) == 1 && int( Toast::Icon::Error ) == 2 && int( Toast::Icon::Information ) == 3 && int( Toast::Icon::Close ) == 4 );

// V3ba representative roles of every API family: Feature (one stateless, one Off/On), More, ViewControl,
// SecondaryControl, RemoteAccessControl (one stateless and one Off/On each) and FileTransferState; each row renders the
// same role through the neutral NextSpaceActionIcons name and the legacy TeacherActionIcons name
using IconFactory = std::function<QIcon( const QPalette& )>;
struct FacadeRow { const char* id; IconFactory neutral; IconFactory legacy; };
const QList<FacadeRow>& facadeRows()
{
	static const auto message = makeFeature( QStringLiteral("TextMessage") );
	static const auto lock = makeFeature( QStringLiteral("ScreenLock") );
	static const QList<FacadeRow> rows{
		{ "Feature TextMessage",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forFeature( message, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forFeature( message, p ); } },
		{ "Feature ScreenLock",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forFeature( lock, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forFeature( lock, p ); } },
		{ "More",
		  []( const QPalette& p ) { return NextSpaceActionIcons::more( p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::more( p ); } },
		{ "ViewControl PoweredOnFilter",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forViewControl( NextSpaceActionIcons::ViewControl::PoweredOnFilter, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forViewControl( TeacherActionIcons::ViewControl::PoweredOnFilter, p ); } },
		{ "ViewControl About",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forViewControl( NextSpaceActionIcons::ViewControl::About, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forViewControl( TeacherActionIcons::ViewControl::About, p ); } },
		{ "SecondaryControl DeleteScreenshot",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forSecondaryControl( NextSpaceActionIcons::SecondaryControl::DeleteScreenshot, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forSecondaryControl( TeacherActionIcons::SecondaryControl::DeleteScreenshot, p ); } },
		{ "SecondaryControl SlideshowStartPause",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forSecondaryControl( NextSpaceActionIcons::SecondaryControl::SlideshowStartPause, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forSecondaryControl( TeacherActionIcons::SecondaryControl::SlideshowStartPause, p ); } },
		{ "RemoteAccessControl ViewOnlyToggle",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forRemoteAccessControl( NextSpaceActionIcons::RemoteAccessControl::ViewOnlyToggle, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forRemoteAccessControl( TeacherActionIcons::RemoteAccessControl::ViewOnlyToggle, p ); } },
		{ "RemoteAccessControl Exit",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forRemoteAccessControl( NextSpaceActionIcons::RemoteAccessControl::Exit, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forRemoteAccessControl( TeacherActionIcons::RemoteAccessControl::Exit, p ); } },
		{ "FileTransferState Transferring",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forFileTransferState( NextSpaceActionIcons::FileTransferState::Transferring, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forFileTransferState( TeacherActionIcons::FileTransferState::Transferring, p ); } },
		{ "FileTransferState Finished",
		  []( const QPalette& p ) { return NextSpaceActionIcons::forFileTransferState( NextSpaceActionIcons::FileTransferState::Finished, p ); },
		  []( const QPalette& p ) { return TeacherActionIcons::forFileTransferState( TeacherActionIcons::FileTransferState::Finished, p ); } },
	};
	return rows;
}

} // namespace



class TeacherActionIconsTest : public QObject
{
	Q_OBJECT
private Q_SLOTS:
	void initTestCase()
	{
		// offscreen evidence only
		QCOMPARE( QGuiApplication::platformName(), QStringLiteral("offscreen") );
	}

	void generatedData()
	{
		const QStringList expected{ QStringLiteral("app-window"), QStringLiteral("apps"), QStringLiteral("camera"),
									QStringLiteral("device-desktop"), QStringLiteral("dots"), QStringLiteral("eye"),
									QStringLiteral("file-download"), QStringLiteral("file-upload"), QStringLiteral("lock"),
									QStringLiteral("lock-open"), QStringLiteral("login"), QStringLiteral("logout"),
									QStringLiteral("maximize"), QStringLiteral("message"), QStringLiteral("pencil"),
									QStringLiteral("pointer"), QStringLiteral("power"), QStringLiteral("refresh"),
									QStringLiteral("screen-share"), QStringLiteral("world"),
									// V3ao, appended after the twenty reviewed V3an glyphs
									QStringLiteral("devices-pc"), QStringLiteral("slideshow"), QStringLiteral("lamp-2"),
									QStringLiteral("users"), QStringLiteral("zoom-scan"), QStringLiteral("layout-grid"),
									QStringLiteral("align-box-center-middle"), QStringLiteral("folder-open"),
									QStringLiteral("device-floppy"), QStringLiteral("info-circle"),
									// V3ap, appended after the thirty reviewed glyphs
									QStringLiteral("trash"),
									// V3aq, appended after the thirty-one reviewed glyphs
									QStringLiteral("player-skip-back"), QStringLiteral("player-skip-forward"),
									// V3ar, appended after the thirty-three reviewed glyphs
									QStringLiteral("player-play"), QStringLiteral("player-pause"),
									// V3at, appended after the thirty-five reviewed glyphs
									QStringLiteral("device-desktop-plus"), QStringLiteral("device-desktop-minus"), QStringLiteral("refresh-off"),
									// V3aw, appended after the thirty-eight reviewed glyphs
									QStringLiteral("keyboard"), QStringLiteral("x"),
									// V3ax, appended after the forty reviewed glyphs
									QStringLiteral("clock"), QStringLiteral("transfer"), QStringLiteral("hourglass"), QStringLiteral("circle-check"),
									// V3bb, appended after the forty-four reviewed glyphs
									QStringLiteral("settings"), QStringLiteral("presentation"), QStringLiteral("shield-lock"),
									QStringLiteral("key"), QStringLiteral("brand-windows"), QStringLiteral("hierarchy-2"),
									// V3bd, appended after the fifty reviewed glyphs
									QStringLiteral("arrow-right"), QStringLiteral("arrow-left"), QStringLiteral("plus"),
									QStringLiteral("arrow-up"), QStringLiteral("arrow-down"),
									// V3bh, appended after the fifty-five reviewed glyphs
									QStringLiteral("player-stop"),
									// V3bi, appended after the fifty-six reviewed glyphs
									QStringLiteral("alert-triangle"), QStringLiteral("circle-x") };
		QCOMPARE( int( std::size( glyphPaths ) ), int( expected.size() ) );
		QCOMPARE( int( Glyph::Count ), int( expected.size() ) );
		for( int i = 0; i < expected.size(); ++i )
		{
			QCOMPARE( QString::fromLatin1( glyphPaths[i].name ), expected[i] );
			int coords = 0;
			for( int op = 0; op < glyphPaths[i].opCount; ++op )
			{
				coords += glyphPaths[i].ops[op] == Op::CubicTo ? 6 : 2;
			}
			QCOMPARE( coords, glyphPaths[i].coordCount );
			QVERIFY( glyphPaths[i].ops[0] == Op::MoveTo );
			const auto bounds = referencePath( expected[i] ).boundingRect();
			QVERIFY2( QRectF( 0, 0, 24, 24 ).contains( bounds ), qPrintable( expected[i] ) );
		}
		QCOMPARE( ViewBoxSize, 24.0f );
		QCOMPARE( SourceStrokeWidth, 2.0f );
		QCOMPARE( RenderStrokeWidth, 2.1f );
		QVERIFY( qAbs( qreal( RenderStrokeWidth ) * LogicalSize / qreal( ViewBoxSize ) - 1.75 ) < 1e-6 );
	}

	// all 27 V3am GC2-S1 inventory rows
	void inventoryRows_data()
	{
		QTest::addColumn<QString>("feature");
		QTest::addColumn<QString>("parentKind");
		QTest::addColumn<QString>("iconUrl");
		QTest::addColumn<QString>("expectedOff");
		QTest::addColumn<QString>("expectedOn");

		const auto row = []( const char* id, const char* feature, const char* parentKind, const char* iconUrl,
							 const char* off, const char* on ) {
			QTest::newRow( id ) << QString::fromLatin1( feature ) << QString::fromLatin1( parentKind )
								<< QString::fromLatin1( iconUrl ) << QString::fromLatin1( off ) << QString::fromLatin1( on );
		};
		row( "V3AM-001 Monitor", "MonitoringMode", "", ":/core/presentation-none.png", "device-desktop", "device-desktop" );
		row( "V3AM-002 Broadcast", "Demo", "", ":/demo/demo.png", "screen-share", "screen-share" );
		row( "V3AM-003 Lock/Unlock", "ScreenLock", "", ":/screenlock/system-lock-screen.png", "lock", "lock-open" );
		row( "V3AM-004 Remote Control", "RemoteControl", "", ":/remoteaccess/krdc.png", "pointer", "pointer" );
		row( "V3AM-005 Message", "TextMessage", "", ":/textmessage/dialog-information.png", "message", "message" );
		row( "V3AM-006 Send Files", "DistributeFiles", "", ":/filetransfer/distribute-files.png", "file-upload", "file-upload" );
		row( "V3AM-007 Remote View", "RemoteView", "", ":/remoteaccess/kmag.png", "eye", "eye" );
		row( "V3AM-008 Collect Files", "FileCollect", "", ":/filetransfer/collect-files.png", "file-download", "file-download" );
		row( "V3AM-009 Screenshot", "Screenshot", "", ":/screenshot/camera-photo.png", "camera", "camera" );
		row( "V3AM-010 Start Application", "StartApp", "", ":/desktopservices/preferences-desktop-launch-feedback.png", "apps", "apps" );
		row( "V3AM-011 Open Website", "OpenWebsite", "", ":/desktopservices/internet-web-browser.png", "world", "world" );
		row( "V3AM-012 Power On", "PowerOn", "", ":/powercontrol/preferences-system-power-management.png", "power", "power" );
		row( "V3AM-013 Reboot", "Reboot", "", ":/powercontrol/system-reboot.png", "refresh", "refresh" );
		row( "V3AM-014 Power Down", "PowerDown", "", ":/powercontrol/system-shutdown.png", "power", "power" );
		row( "V3AM-015 Log In", "UserLogin", "", ":/usersessioncontrol/login-user.png", "login", "login" );
		row( "V3AM-016 Log Off", "UserLogoff", "", ":/usersessioncontrol/logout-user.png", "logout", "logout" );
		row( "V3AM-017 Power down now", "PowerDownNow", "child", "", "", "" );
		row( "V3AM-018 Install updates and power down", "InstallUpdatesAndPowerDown", "child", "", "", "" );
		row( "V3AM-019 Power down after user confirmation", "PowerDownConfirmed", "child", "", "", "" );
		row( "V3AM-020 Power down after timeout", "PowerDownDelayed", "child", "", "", "" );
		row( "V3AM-021 Own screen fullscreen", "ShareOwnScreenFullScreen", "child", ":/demo/presentation-fullscreen.png", "maximize", "maximize" );
		row( "V3AM-022 Own screen window", "ShareOwnScreenWindow", "child", ":/demo/presentation-window.png", "app-window", "app-window" );
		row( "V3AM-023 User screen fullscreen", "ShareUserScreenFullScreen", "child", ":/demo/presentation-fullscreen.png", "maximize", "maximize" );
		row( "V3AM-024 User screen window", "ShareUserScreenWindow", "child", ":/demo/presentation-window.png", "app-window", "app-window" );
		row( "V3AM-025 More button", "MoreActionsButton", "", "", "dots", "dots" );
		row( "V3AM-026 Custom application", "StartApp", "child", ":/core/document-edit.png", "pencil", "pencil" );
		row( "V3AM-026 Custom website", "OpenWebsite", "child", ":/core/document-edit.png", "pencil", "pencil" );
		row( "V3AM-027 Predefined application", "StartApp", "child", "", "", "" );
		row( "V3AM-027 Predefined website", "OpenWebsite", "child", "", "", "" );
	}

	void inventoryRows()
	{
		QFETCH( QString, feature );
		QFETCH( QString, parentKind );
		QFETCH( QString, iconUrl );
		QFETCH( QString, expectedOff );
		QFETCH( QString, expectedOn );

		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto icon = feature == QStringLiteral("MoreActionsButton") ?
							  TeacherActionIcons::more( palette ) :
							  TeacherActionIcons::forFeature( makeFeature( feature, parentKind.isEmpty() ? Feature::Uid() : QUuid::createUuid(), iconUrl ),
															  palette );
		if( expectedOff.isEmpty() )
		{
			// text-only rows: an empty QIcon, exactly as their empty iconUrl gives today
			QVERIFY( icon.isNull() );
			return;
		}
		QVERIFY( icon.isNull() == false );
		const auto colour = expectedColour( palette, QIcon::Normal );
		for( const auto dpr : Scales )
		{
			QCOMPARE( maxDifference( renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ), referenceImage( expectedOff, colour, dpr ) ), 0 );
			QCOMPARE( maxDifference( renderIcon( icon, dpr, QIcon::Normal, QIcon::On ), referenceImage( expectedOn, colour, dpr ) ), 0 );
		}
	}

	// V3ao: the eleven V3am GC2-S2a rows (twelve roles; V3AM-044 is Load and Save)
	void viewControls_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < viewRows().size(); ++i )
		{
			QTest::newRow( viewRows()[i].id ) << i;
		}
	}

	void viewControls()
	{
		QFETCH( int, index );
		const auto& row = viewRows()[index];
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto icon = TeacherActionIcons::forViewControl( row.control, palette );
		QVERIFY( icon.isNull() == false );
		const auto colour = expectedColour( palette, QIcon::Normal );
		for( const auto dpr : Scales )
		{
			QCOMPARE( maxDifference( renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ), referenceImage( QLatin1String( row.glyph ), colour, dpr ) ), 0 );
			QCOMPARE( maxDifference( renderIcon( icon, dpr, QIcon::Normal, QIcon::On ), referenceImage( QLatin1String( row.glyph ), colour, dpr ) ), 0 );
			QVERIFY( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr ).isNull() == false );
		}
	}

	void viewControlsAreDistinct()
	{
		// twelve roles, ten new glyphs plus the reused camera and power; only those two share V3an geometry
		const auto palette = syntheticPalette( PaletteKind::Light );
		QSet<QString> glyphs;
		for( const auto& row : viewRows() )
		{
			glyphs.insert( QLatin1String( row.glyph ) );
		}
		QCOMPARE( glyphs.size(), 12 );
		QCOMPARE( maxDifference( renderIcon( TeacherActionIcons::forViewControl( ViewControl::ScreenshotManagementPanel, palette ), 1.0, QIcon::Normal, QIcon::Off ),
								 renderIcon( TeacherActionIcons::forFeature( makeFeature( QStringLiteral("Screenshot") ), palette ), 1.0, QIcon::Normal, QIcon::Off ) ), 0 );
		QCOMPARE( maxDifference( renderIcon( TeacherActionIcons::forViewControl( ViewControl::PoweredOnFilter, palette ), 1.0, QIcon::Normal, QIcon::Off ),
								 renderIcon( TeacherActionIcons::forFeature( makeFeature( QStringLiteral("PowerOn") ), palette ), 1.0, QIcon::Normal, QIcon::Off ) ), 0 );
		for( int i = 0; i < viewRows().size(); ++i )
		{
			for( int j = i + 1; j < viewRows().size(); ++j )
			{
				const auto a = renderIcon( TeacherActionIcons::forViewControl( viewRows()[i].control, palette ), 1.0, QIcon::Normal, QIcon::Off );
				const auto b = renderIcon( TeacherActionIcons::forViewControl( viewRows()[j].control, palette ), 1.0, QIcon::Normal, QIcon::Off );
				QVERIFY2( maxDifference( a, b ) > 128, viewRows()[i].id );
			}
		}
	}

	// checkable toggles keep one glyph; the existing checked-button styling remains the state cue
	void viewControlsStableAcrossStates()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto& row : viewRows() )
			{
				const auto icon = TeacherActionIcons::forViewControl( row.control, palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						QVERIFY2( maxDifference( renderIcon( icon, dpr, mode, QIcon::Off ), renderIcon( icon, dpr, mode, QIcon::On ) ) == 0, row.id );
					}
				}
			}
		}
	}

	void viewControlPaletteRoles()
	{
		// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{ { QIcon::Normal, QColor( 192, 0, 0 ) },
														  { QIcon::Active, QColor( 192, 0, 0 ) },
														  { QIcon::Disabled, QColor( 0, 160, 0 ) },
														  { QIcon::Selected, QColor( 0, 0, 192 ) } };
		for( const auto& row : viewRows() )
		{
			const auto icon = TeacherActionIcons::forViewControl( row.control, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					const auto ink = measure( renderIcon( icon, 2.0, mode, state ), QRect( 0, 0, 20, 20 ), colour );
					QVERIFY2( ink.colourSamples > 10, row.id );
					QVERIFY2( ink.colourMismatches == 0, row.id );
				}
			}
		}
		// each synthetic palette is followed; nothing is hard-coded
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto& row : viewRows() )
			{
				const auto icon = TeacherActionIcons::forViewControl( row.control, palette );
				for( const auto mode : Modes )
				{
					const auto ink = measure( renderIcon( icon, 1.5, mode, QIcon::Off ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
					QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
				}
			}
		}
	}

	// V3ap: V3AM-046 Show and V3AM-047 Delete in the Teacher Screenshots panel
	void secondaryControls_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < secondaryRows().size(); ++i )
		{
			QTest::newRow( secondaryRows()[i].id ) << i;
		}
	}

	void secondaryControls()
	{
		QFETCH( int, index );
		const auto& row = secondaryRows()[index];
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto icon = TeacherActionIcons::forSecondaryControl( row.control, palette );
		QVERIFY( icon.isNull() == false );
		const auto colour = expectedColour( palette, QIcon::Normal );
		for( const auto dpr : Scales )
		{
			QCOMPARE( maxDifference( renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ), referenceImage( QLatin1String( row.glyph ), colour, dpr ) ), 0 );
			QCOMPARE( maxDifference( renderIcon( icon, dpr, QIcon::Normal, QIcon::On ), referenceImage( QLatin1String( row.glyph ), colour, dpr ) ), 0 );
			QVERIFY( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr ).isNull() == false );
		}
	}

	// Show reuses the reviewed Remote View eye exactly, in every palette, mode and scale
	void showReusesRemoteViewEye()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto show = TeacherActionIcons::forSecondaryControl( SecondaryControl::ShowScreenshot, palette );
			const auto remoteView = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("RemoteView") ), palette );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					QCOMPARE( maxDifference( renderIcon( show, dpr, mode, QIcon::Off ), renderIcon( remoteView, dpr, mode, QIcon::Off ) ), 0 );
				}
			}
		}
	}

	// Delete is a new glyph distinct from Show and from every other icon; Show differs from all but Remote View
	void secondaryControlsAreDistinct()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto show = renderIcon( TeacherActionIcons::forSecondaryControl( SecondaryControl::ShowScreenshot, palette ), 1.0, QIcon::Normal, QIcon::Off );
		const auto del = renderIcon( TeacherActionIcons::forSecondaryControl( SecondaryControl::DeleteScreenshot, palette ), 1.0, QIcon::Normal, QIcon::Off );
		QVERIFY( maxDifference( show, del ) > 128 );
		for( const auto& entry : sheetEntries() )
		{
			if( entry.parentKind == QStringLiteral("secondary") )
			{
				continue;
			}
			// V3bc: the Administration Reset configuration command reuses the Delete trash by design; see
			// administrationCommandsReuseReviewedGlyphs()
			if( entry.parentKind == QStringLiteral("command") && entry.feature == QStringLiteral("ResetConfiguration") )
			{
				continue;
			}
			// V3bd: the Access Control Remove rule command reuses the Delete trash by design; see
			// accessControlCommandsReuseReviewedGlyphs()
			if( entry.parentKind == QStringLiteral("access") && entry.feature == QStringLiteral("RemoveRule") )
			{
				continue;
			}
			// V3be: the General page Clear log files command reuses the Delete trash by design; see
			// generalCommandsReuseReviewedGlyphs()
			if( entry.parentKind == QStringLiteral("general") && entry.feature == QStringLiteral("ClearLogFiles") )
			{
				continue;
			}
			// V3bk: the Locations & computers Remove location and Remove computer commands reuse the Delete trash by
			// design; see builtinDirectoryCommandsReuseReviewedGlyphs()
			if( entry.parentKind == QStringLiteral("builtindirectory") &&
				( entry.feature == QStringLiteral("RemoveLocation") || entry.feature == QStringLiteral("RemoveComputer") ) )
			{
				continue;
			}
			// V3bl: the Applications & websites Remove application and Remove website commands reuse the Delete trash by
			// design; see desktopServicesCommandsReuseReviewedGlyphs()
			if( entry.parentKind == QStringLiteral("desktopservices") &&
				( entry.feature == QStringLiteral("RemoveApplication") || entry.feature == QStringLiteral("RemoveWebsite") ) )
			{
				continue;
			}
			const auto other = renderIcon( iconFor( entry.feature, entry.parentKind, palette ), 1.0, QIcon::Normal, entry.state );
			QVERIFY2( maxDifference( del, other ) > 128, qPrintable( entry.label ) );
			// V3aw: the Remote Access View-only toggle reuses the same eye in its Off state
			if( entry.parentKind == QStringLiteral("remote") && entry.feature == QStringLiteral("ViewOnlyToggle") && entry.state == QIcon::Off )
			{
				QVERIFY2( maxDifference( show, other ) == 0, qPrintable( entry.label ) );
				continue;
			}
			if( entry.feature != QStringLiteral("RemoteView") )
			{
				QVERIFY2( maxDifference( show, other ) > 128, qPrintable( entry.label ) );
			}
		}
	}

	// neither button is checkable; both roles still draw one glyph for both states
	void secondaryControlsStableAcrossStates()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto& row : secondaryRows() )
			{
				const auto icon = TeacherActionIcons::forSecondaryControl( row.control, palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						QVERIFY2( maxDifference( renderIcon( icon, dpr, mode, QIcon::Off ), renderIcon( icon, dpr, mode, QIcon::On ) ) == 0, row.id );
					}
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; no destructive red
	void secondaryControlPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{ { QIcon::Normal, QColor( 192, 0, 0 ) },
														  { QIcon::Active, QColor( 192, 0, 0 ) },
														  { QIcon::Disabled, QColor( 0, 160, 0 ) },
														  { QIcon::Selected, QColor( 0, 0, 192 ) } };
		for( const auto& row : secondaryRows() )
		{
			const auto icon = TeacherActionIcons::forSecondaryControl( row.control, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					const auto ink = measure( renderIcon( icon, 2.0, mode, state ), QRect( 0, 0, 20, 20 ), colour );
					QVERIFY2( ink.colourSamples > 10, row.id );
					QVERIFY2( ink.colourMismatches == 0, row.id );
				}
			}
		}
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto& row : secondaryRows() )
			{
				const auto icon = TeacherActionIcons::forSecondaryControl( row.control, palette );
				for( const auto mode : Modes )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderIcon( icon, dpr, mode, QIcon::Off ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// the bin is an open outline, not a filled shape: its centre, between the two inner strokes, stays clear
	void deleteIsOpenOutline()
	{
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::DeleteScreenshot, syntheticPalette( PaletteKind::Light ) );
		for( const auto dpr : Scales )
		{
			const auto image = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const int centre = qFloor( LogicalSize / 2.0 * dpr );
			QVERIFY2( qAlpha( image.pixel( centre, centre ) ) < 128, qPrintable( QString::number( dpr ) ) );
			const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
			QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( QString::number( dpr ) ) );
		}
	}

	// V3aq: V3AM-049 Previous and Next differ from each other and from every other icon on the sheet at every scale
	void navigationControlsAreDistinct()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto previousIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::PreviousSlide, palette );
		const auto nextIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::NextSlide, palette );
		for( const auto dpr : Scales )
		{
			const auto previous = renderIcon( previousIcon, dpr, QIcon::Normal, QIcon::Off );
			const auto next = renderIcon( nextIcon, dpr, QIcon::Normal, QIcon::Off );
			QVERIFY( maxDifference( previous, next ) > 128 );
			for( const auto& entry : sheetEntries() )
			{
				if( entry.feature == QStringLiteral("PreviousSlide") || entry.feature == QStringLiteral("NextSlide") )
				{
					continue;
				}
				const auto other = renderIcon( iconFor( entry.feature, entry.parentKind, palette ), dpr, QIcon::Normal, entry.state );
				QVERIFY2( maxDifference( previous, other ) > 128, qPrintable( entry.label ) );
				QVERIFY2( maxDifference( next, other ) > 128, qPrintable( entry.label ) );
			}
		}
	}

	// Previous points left and Next points right: each is the other's mirror image, and on the centre row
	// (y = 12 units) the triangle apex is inked at x = 8 (Previous) or x = 16 (Next) while the other point is clear.
	// Mirroring reverses the path orientation, so the stroker's round joins/caps differ slightly at anti-aliased
	// edges; the mirror test therefore compares summed coverage rather than single pixels.
	void navigationDirection()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto previousIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::PreviousSlide, palette );
		const auto nextIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::NextSlide, palette );
		for( const auto dpr : Scales )
		{
			const auto previous = renderIcon( previousIcon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto next = renderIcon( nextIcon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto where = QString::number( dpr );
			const auto mirrored = alphaDifferenceRatio( previous.flipped( Qt::Horizontal ), next );
			const auto unmirrored = alphaDifferenceRatio( previous, next );
			qInfo( "dpr %.2f: mirrored Previous vs Next max pixel difference %d, coverage difference %.4f; unmirrored %.4f",
				   dpr, maxDifference( previous.flipped( Qt::Horizontal ), next ), mirrored, unmirrored );
			QVERIFY2( mirrored < 0.05, qPrintable( where ) );
			QVERIFY2( unmirrored > 0.5, qPrintable( where ) );
			const QRectF leftApex( 7.9, 11.9, 0.2, 0.2 );
			const QRectF rightApex( 15.9, 11.9, 0.2, 0.2 );
			QVERIFY2( maxAlphaInUnits( previous, dpr, leftApex ) >= 128, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( previous, dpr, rightApex ) == 0, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( next, dpr, rightApex ) >= 128, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( next, dpr, leftApex ) == 0, qPrintable( where ) );
		}
	}

	// both triangles stay open outlines, not filled or fused shapes: the region around each triangle's centroid stays
	// clear, the bar stays separate from the apex and coverage stays below half
	void navigationIsOpenOutline()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		for( const auto dpr : Scales )
		{
			const auto where = QString::number( dpr );
			const auto previous = renderIcon( TeacherActionIcons::forSecondaryControl( SecondaryControl::PreviousSlide, palette ),
											  dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto next = renderIcon( TeacherActionIcons::forSecondaryControl( SecondaryControl::NextSlide, palette ),
										  dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			QVERIFY2( maxAlphaInUnits( previous, dpr, QRectF( 15, 11, 2, 2 ) ) == 0, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( next, dpr, QRectF( 7, 11, 2, 2 ) ) == 0, qPrintable( where ) );
			// between the bar (x = 4 or 20) and the apex (x = 8 or 16) on the centre row
			QVERIFY2( maxAlphaInUnits( previous, dpr, QRectF( 5.9, 11.9, 0.2, 0.2 ) ) < 128, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( next, dpr, QRectF( 17.9, 11.9, 0.2, 0.2 ) ) < 128, qPrintable( where ) );
			for( const auto& image : { previous, next } )
			{
				const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
			}
		}
	}

	// V3ar: V3AM-050, the checkable Slideshow start/pause button. Off (not running) draws exactly player-play and On
	// (running, the button's initial checked state) draws exactly player-pause, in every palette, mode and scale
	void startPauseStates()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto colour = expectedColour( palette, mode );
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::Off ), referenceImage( QStringLiteral("player-play"), colour, dpr ) ), 0 );
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::On ), referenceImage( QStringLiteral("player-pause"), colour, dpr ) ), 0 );
				}
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					QVERIFY( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, QIcon::Normal, state ).isNull() == false );
				}
			}
		}
	}

	// the two states are immediately distinguishable and never inverted
	void startPauseStatesAreDistinct()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, palette );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto colour = expectedColour( palette, mode );
					const auto off = renderIcon( icon, dpr, mode, QIcon::Off );
					const auto on = renderIcon( icon, dpr, mode, QIcon::On );
					QVERIFY2( maxDifference( off, on ) > 128, qPrintable( where ) );
					QVERIFY2( alphaDifferenceRatio( off, on ) > 0.5, qPrintable( where ) );
					QVERIFY2( maxDifference( off, referenceImage( QStringLiteral("player-pause"), colour, dpr ) ) > 128, qPrintable( where ) );
					QVERIFY2( maxDifference( on, referenceImage( QStringLiteral("player-play"), colour, dpr ) ) > 128, qPrintable( where ) );
				}
			}
		}
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, syntheticPalette( PaletteKind::Light ) );
		for( const auto dpr : Scales )
		{
			qInfo( "dpr %.2f: play (Off) vs pause (On) coverage difference %.4f", dpr,
				   alphaDifferenceRatio( renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ), renderIcon( icon, dpr, QIcon::Normal, QIcon::On ) ) );
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states
	void startPausePaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{ { QIcon::Normal, QColor( 192, 0, 0 ) },
														  { QIcon::Active, QColor( 192, 0, 0 ) },
														  { QIcon::Disabled, QColor( 0, 160, 0 ) },
														  { QIcon::Selected, QColor( 0, 0, 192 ) } };
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, probe );
		for( const auto& [mode, colour] : expected )
		{
			for( const auto state : { QIcon::Off, QIcon::On } )
			{
				for( const auto dpr : Scales )
				{
					const auto ink = measure( renderIcon( icon, dpr, mode, state ), QRect( 0, 0, 20, 20 ), colour );
					QVERIFY( ink.colourSamples > 10 );
					QCOMPARE( ink.colourMismatches, 0 );
				}
			}
		}
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto themed = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, palette );
			for( const auto mode : Modes )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					const auto ink = measure( renderIcon( themed, 1.5, mode, state ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
					QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) ) );
				}
			}
		}
	}

	// at logical 20 px play stays an open triangle pointing right and pause stays two open, separate bars: the
	// triangle's centre and each bar's inner column stay clear, the column between the bars stays under half coverage
	// along its whole height, and neither state is filled
	void startPauseIsOpenOutline()
	{
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, syntheticPalette( PaletteKind::Light ) );
		for( const auto dpr : Scales )
		{
			const auto where = QString::number( dpr );
			const auto play = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto pause = renderIcon( icon, dpr, QIcon::Normal, QIcon::On ).convertToFormat( QImage::Format_ARGB32 );
			// play: triangle (7,4) (7,20) (20,12); the region around its centroid (11.3, 12) is clear
			QVERIFY2( maxAlphaInUnits( play, dpr, QRectF( 10, 11, 2, 2 ) ) == 0, qPrintable( where ) );
			// it points right: on the centre row the base (x = 7) and the apex (x = 20) are inked, left of the base is clear
			QVERIFY2( maxAlphaInUnits( play, dpr, QRectF( 6.9, 11.9, 0.2, 0.2 ) ) >= 128, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( play, dpr, QRectF( 19.9, 11.9, 0.2, 0.2 ) ) >= 128, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( play, dpr, QRectF( 3.9, 11.9, 0.2, 0.2 ) ) == 0, qPrintable( where ) );
			// pause: rounded bars x 6..10 and 14..18, y 5..19; each bar's inner column (x = 8 / 16) is clear
			QVERIFY2( maxAlphaInUnits( pause, dpr, QRectF( 7.9, 8, 0.2, 8 ) ) == 0, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( pause, dpr, QRectF( 15.9, 8, 0.2, 8 ) ) == 0, qPrintable( where ) );
			// the bars stay separate: the centre column (x = 12) is under half coverage from top to bottom
			const auto gap = maxAlphaInUnits( pause, dpr, QRectF( 11.9, 3, 0.2, 18 ) );
			qInfo( "dpr %.2f: pause centre-gap max alpha %d", dpr, gap );
			QVERIFY2( gap < 128, qPrintable( where ) );
			for( const auto& image : { play, pause } )
			{
				const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
			}
		}
	}

	// play and pause each differ from every other icon on the sheet at every scale
	void startPauseIsDistinct()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, palette );
		for( const auto dpr : Scales )
		{
			const auto play = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off );
			const auto pause = renderIcon( icon, dpr, QIcon::Normal, QIcon::On );
			for( const auto& entry : sheetEntries() )
			{
				if( entry.feature == QStringLiteral("SlideshowStartPause") )
				{
					continue;
				}
				const auto other = renderIcon( iconFor( entry.feature, entry.parentKind, palette ), dpr, QIcon::Normal, entry.state );
				// V3bh: the Student Agent Start service command reuses the play glyph by design (see
				// studentAgentStartReusesSlideshowPlay()); it still differs from pause
				if( entry.parentKind == QStringLiteral("agent") && entry.feature == QStringLiteral("StartService") )
				{
					QVERIFY2( maxDifference( play, other ) == 0, qPrintable( entry.label ) );
					QVERIFY2( maxDifference( pause, other ) > 128, qPrintable( entry.label ) );
					continue;
				}
				QVERIFY2( maxDifference( play, other ) > 128, qPrintable( entry.label ) );
				QVERIFY2( maxDifference( pause, other ) > 128, qPrintable( entry.label ) );
			}
		}
	}

	// V3as: V3AM-051, the checkable Slideshow separate-window toggle, draws exactly the reviewed maximize, identical to
	// the fullscreen share mode, in both states and in every palette, mode and scale
	void separateWindowReusesFullscreenMaximize()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto separateWindow = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowSeparateWindow, palette );
			const auto fullscreen = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("ShareOwnScreenFullScreen"), QUuid::createUuid() ), palette );
			QVERIFY( separateWindow.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto colour = expectedColour( palette, mode );
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto image = renderIcon( separateWindow, dpr, mode, state );
						QCOMPARE( maxDifference( image, referenceImage( QStringLiteral("maximize"), colour, dpr ) ), 0 );
						QCOMPARE( maxDifference( image, renderIcon( fullscreen, dpr, mode, state ) ), 0 );
						QVERIFY( separateWindow.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).isNull() == false );
					}
				}
			}
		}
	}

	// the toggle keeps one glyph; its existing checked/pressed button styling remains the only state cue
	void separateWindowStableAcrossStates()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto separateWindow = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowSeparateWindow, syntheticPalette( kind ) );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					QCOMPARE( maxDifference( renderIcon( separateWindow, dpr, mode, QIcon::Off ), renderIcon( separateWindow, dpr, mode, QIcon::On ) ), 0 );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states
	void separateWindowPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{ { QIcon::Normal, QColor( 192, 0, 0 ) },
														  { QIcon::Active, QColor( 192, 0, 0 ) },
														  { QIcon::Disabled, QColor( 0, 160, 0 ) },
														  { QIcon::Selected, QColor( 0, 0, 192 ) } };
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowSeparateWindow, probe );
		for( const auto& [mode, colour] : expected )
		{
			for( const auto state : { QIcon::Off, QIcon::On } )
			{
				for( const auto dpr : Scales )
				{
					const auto ink = measure( renderIcon( icon, dpr, mode, state ), QRect( 0, 0, 20, 20 ), colour );
					QVERIFY( ink.colourSamples > 10 );
					QCOMPARE( ink.colourMismatches, 0 );
				}
			}
		}
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto themed = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowSeparateWindow, palette );
			for( const auto mode : Modes )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					const auto ink = measure( renderIcon( themed, 1.5, mode, state ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
					QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) ) );
				}
			}
		}
	}

	// at logical 20 px maximize stays four open, separate corner brackets: every bracket arm is inked, the middle of
	// each edge stays clear (the brackets do not fuse into a frame), the centre stays clear and coverage is below half
	void separateWindowIsOpenOutline()
	{
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowSeparateWindow, syntheticPalette( PaletteKind::Light ) );
		for( const auto dpr : Scales )
		{
			const auto where = QString::number( dpr );
			const auto image = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			// bracket arms on x = 4 / 20 (y 6..8 and 16..18) and on y = 4 / 20 (x 6..8 and 16..18)
			for( const auto& arm : { QPointF( 4, 7 ), QPointF( 7, 4 ), QPointF( 17, 4 ), QPointF( 20, 7 ),
									 QPointF( 20, 17 ), QPointF( 17, 20 ), QPointF( 7, 20 ), QPointF( 4, 17 ) } )
			{
				QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( arm.x() - 0.1, arm.y() - 0.1, 0.2, 0.2 ) ) >= 128,
						  qPrintable( where + QStringLiteral(" arm %1,%2").arg( arm.x() ).arg( arm.y() ) ) );
			}
			// the arms end with round caps at 8 + 1.05 and 16 - 1.05 units; the middle of every edge is clear
			for( const auto& gap : { QRectF( 11, 2.5, 2, 3 ), QRectF( 11, 18.5, 2, 3 ), QRectF( 2.5, 11, 3, 2 ), QRectF( 18.5, 11, 3, 2 ) } )
			{
				QVERIFY2( maxAlphaInUnits( image, dpr, gap ) == 0, qPrintable( where ) );
			}
			QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( 7, 7, 10, 10 ) ) == 0, qPrintable( where ) );
			const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
			QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
			qInfo( "dpr %.2f: maximize ink %.1f %% of the 20 px box", dpr,
				   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
		}
	}

	// the separate-window icon differs from every other sheet icon except the two fullscreen share modes, whose
	// reviewed maximize it reuses exactly
	void separateWindowIsDistinct()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowSeparateWindow, palette );
		for( const auto dpr : Scales )
		{
			const auto separateWindow = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off );
			for( const auto& entry : sheetEntries() )
			{
				if( entry.feature == QStringLiteral("SlideshowSeparateWindow") )
				{
					continue;
				}
				const auto other = renderIcon( iconFor( entry.feature, entry.parentKind, palette ), dpr, QIcon::Normal, entry.state );
				// V3aw: the Remote Access Fullscreen toggle reuses the same maximize in its Off state
				if( entry.parentKind == QStringLiteral("remote") && entry.feature == QStringLiteral("FullscreenToggle") && entry.state == QIcon::Off )
				{
					QVERIFY2( maxDifference( separateWindow, other ) == 0, qPrintable( entry.label ) );
					continue;
				}
				if( entry.feature == QStringLiteral("ShareOwnScreenFullScreen") || entry.feature == QStringLiteral("ShareUserScreenFullScreen") )
				{
					QVERIFY2( maxDifference( separateWindow, other ) == 0, qPrintable( entry.label ) );
				}
				else
				{
					QVERIFY2( maxDifference( separateWindow, other ) > 128, qPrintable( entry.label ) );
				}
			}
		}
	}

	// V3at: V3AM-052, the Spotlight-panel Add and Remove tool buttons. Add draws exactly device-desktop-plus and Remove
	// exactly device-desktop-minus, in both states and in every palette, mode and scale; they name the computer and
	// the operation instead of the old up/down arrows. Their only geometric differences are the plus bar (x = 19,
	// y 16..22) and the monitor's right edge (x = 21), which Add shortens to y = 12.5 and Remove keeps to y = 15
	void spotlightAddRemoveSemantics()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto addIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightAdd, palette );
			const auto removeIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRemove, palette );
			QVERIFY( addIcon.isNull() == false );
			QVERIFY( removeIcon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto colour = expectedColour( palette, mode );
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						QCOMPARE( maxDifference( renderIcon( addIcon, dpr, mode, state ), referenceImage( QStringLiteral("device-desktop-plus"), colour, dpr ) ), 0 );
						QCOMPARE( maxDifference( renderIcon( removeIcon, dpr, mode, state ), referenceImage( QStringLiteral("device-desktop-minus"), colour, dpr ) ), 0 );
						QVERIFY( addIcon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).isNull() == false );
						QVERIFY( removeIcon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).isNull() == false );
					}
				}
			}
		}
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto addIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightAdd, palette );
		const auto removeIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRemove, palette );
		for( const auto dpr : Scales )
		{
			const auto where = QString::number( dpr );
			const auto add = renderIcon( addIcon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto remove = renderIcon( removeIcon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			// the plus bar above and below the shared horizontal bar: inked for Add, clear for Remove (not inverted)
			for( const auto& plusBar : { QRectF( 18.9, 16.4, 0.2, 0.2 ), QRectF( 18.9, 21.4, 0.2, 0.2 ) } )
			{
				QVERIFY2( maxAlphaInUnits( add, dpr, plusBar ) >= 128, qPrintable( where ) );
				QVERIFY2( maxAlphaInUnits( remove, dpr, plusBar ) == 0, qPrintable( where ) );
			}
			// both keep the horizontal bar (x 16..22, y = 19)
			QVERIFY2( maxAlphaInUnits( add, dpr, QRectF( 16.9, 18.9, 0.2, 0.2 ) ) >= 128, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( remove, dpr, QRectF( 16.9, 18.9, 0.2, 0.2 ) ) >= 128, qPrintable( where ) );
			// Remove's monitor edge continues to y = 15 where Add's stops short of its plus
			QVERIFY2( maxAlphaInUnits( remove, dpr, QRectF( 20.9, 14.6, 0.2, 0.2 ) ) >= 128, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( add, dpr, QRectF( 20.9, 14.6, 0.2, 0.2 ) ) == 0, qPrintable( where ) );
			QVERIFY2( alphaDifferenceRatio( add, remove ) > 0.05, qPrintable( where ) );
			qInfo( "dpr %.2f: Add vs Remove max pixel difference %d, coverage difference %.4f", dpr,
				   maxDifference( add, remove ), alphaDifferenceRatio( add, remove ) );
		}
	}

	// Add and Remove differ from each other and from every other sheet icon at every scale, including Monitor's
	// device-desktop, and keep one glyph in both states
	void spotlightAddRemoveAreDistinct()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto addIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightAdd, palette );
		const auto removeIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRemove, palette );
		for( const auto dpr : Scales )
		{
			const auto add = renderIcon( addIcon, dpr, QIcon::Normal, QIcon::Off );
			const auto remove = renderIcon( removeIcon, dpr, QIcon::Normal, QIcon::Off );
			QVERIFY( maxDifference( add, remove ) > 128 );
			for( const auto mode : Modes )
			{
				QCOMPARE( maxDifference( renderIcon( addIcon, dpr, mode, QIcon::Off ), renderIcon( addIcon, dpr, mode, QIcon::On ) ), 0 );
				QCOMPARE( maxDifference( renderIcon( removeIcon, dpr, mode, QIcon::Off ), renderIcon( removeIcon, dpr, mode, QIcon::On ) ), 0 );
			}
			for( const auto& entry : sheetEntries() )
			{
				if( entry.feature == QStringLiteral("SpotlightAdd") || entry.feature == QStringLiteral("SpotlightRemove") )
				{
					continue;
				}
				const auto other = renderIcon( iconFor( entry.feature, entry.parentKind, palette ), dpr, QIcon::Normal, entry.state );
				QVERIFY2( maxDifference( add, other ) > 128, qPrintable( entry.label ) );
				QVERIFY2( maxDifference( remove, other ) > 128, qPrintable( entry.label ) );
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states
	void spotlightAddRemovePaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{ { QIcon::Normal, QColor( 192, 0, 0 ) },
														  { QIcon::Active, QColor( 192, 0, 0 ) },
														  { QIcon::Disabled, QColor( 0, 160, 0 ) },
														  { QIcon::Selected, QColor( 0, 0, 192 ) } };
		for( const auto control : { SecondaryControl::SpotlightAdd, SecondaryControl::SpotlightRemove } )
		{
			const auto icon = TeacherActionIcons::forSecondaryControl( control, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderIcon( icon, dpr, mode, state ), QRect( 0, 0, 20, 20 ), colour );
						QVERIFY( ink.colourSamples > 10 );
						QCOMPARE( ink.colourMismatches, 0 );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = TeacherActionIcons::forSecondaryControl( control, palette );
				for( const auto mode : Modes )
				{
					const auto ink = measure( renderIcon( themed, 1.5, mode, QIcon::Off ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
					QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) ) );
				}
			}
		}
	}

	// at logical 20 px the monitor stays an open, unfilled outline and the plus/minus stays separate from it: the
	// screen interior is clear, the gaps between the monitor's bottom/right ends and the sign stay under half
	// coverage, and coverage is below half
	void spotlightAddRemoveIsOpenOutline()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto addIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightAdd, palette );
		const auto removeIcon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRemove, palette );
		for( const auto dpr : Scales )
		{
			const auto where = QString::number( dpr );
			const auto add = renderIcon( addIcon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto remove = renderIcon( removeIcon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			for( const auto& image : { add, remove } )
			{
				// screen interior (frame x 3..21, y 4..16)
				QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( 8, 7, 8, 5 ) ) == 0, qPrintable( where ) );
				// between the monitor's bottom edge end (13.5, 16) and the sign's left end (16, 19)
				QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( 14.65, 17.4, 0.2, 0.2 ) ) < 128, qPrintable( where ) );
				// between the monitor's right edge end and the sign's horizontal bar
				QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( 20.9, 16.9, 0.2, 0.2 ) ) < 128, qPrintable( where ) );
				const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
			}
			// between Add's shortened right edge (21, 12.5) and the top of its plus bar (19, 16)
			QVERIFY2( maxAlphaInUnits( add, dpr, QRectF( 19.9, 14.15, 0.2, 0.2 ) ) < 128, qPrintable( where ) );
		}
	}

	// V3at: V3AM-053, the checkable Spotlight realtime toggle (initially checked). Off (thumbnails not updated in
	// realtime) draws exactly the crossed-out refresh-off and On (realtime, the button's initial checked state) draws
	// exactly the reviewed refresh, identical to Reboot, in every palette, mode and scale
	void realtimeStates()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRealtime, palette );
			const auto reboot = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("Reboot") ), palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto colour = expectedColour( palette, mode );
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::Off ), referenceImage( QStringLiteral("refresh-off"), colour, dpr ) ), 0 );
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::On ), referenceImage( QStringLiteral("refresh"), colour, dpr ) ), 0 );
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::On ), renderIcon( reboot, dpr, mode, QIcon::Off ) ), 0 );
				}
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					QVERIFY( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, QIcon::Normal, state ).isNull() == false );
				}
			}
		}
	}

	// the two states are distinguishable without colour and never inverted: Off carries the corner-to-corner slash
	// through the centre, which On leaves clear
	void realtimeStatesAreDistinct()
	{
		const QRectF centre( 11.9, 11.9, 0.2, 0.2 );
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRealtime, palette );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto colour = expectedColour( palette, mode );
					const auto off = renderIcon( icon, dpr, mode, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
					const auto on = renderIcon( icon, dpr, mode, QIcon::On ).convertToFormat( QImage::Format_ARGB32 );
					QVERIFY2( maxDifference( off, on ) > 128, qPrintable( where ) );
					QVERIFY2( alphaDifferenceRatio( off, on ) > 0.25, qPrintable( where ) );
					QVERIFY2( maxDifference( off, referenceImage( QStringLiteral("refresh"), colour, dpr ) ) > 128, qPrintable( where ) );
					QVERIFY2( maxDifference( on, referenceImage( QStringLiteral("refresh-off"), colour, dpr ) ) > 128, qPrintable( where ) );
					QVERIFY2( maxAlphaInUnits( off, dpr, centre ) >= 128, qPrintable( where ) );
					QVERIFY2( maxAlphaInUnits( on, dpr, centre ) == 0, qPrintable( where ) );
				}
			}
		}
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRealtime, syntheticPalette( PaletteKind::Light ) );
		for( const auto dpr : Scales )
		{
			qInfo( "dpr %.2f: refresh-off (Off) vs refresh (On) coverage difference %.4f", dpr,
				   alphaDifferenceRatio( renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ), renderIcon( icon, dpr, QIcon::Normal, QIcon::On ) ) );
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states
	void realtimePaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{ { QIcon::Normal, QColor( 192, 0, 0 ) },
														  { QIcon::Active, QColor( 192, 0, 0 ) },
														  { QIcon::Disabled, QColor( 0, 160, 0 ) },
														  { QIcon::Selected, QColor( 0, 0, 192 ) } };
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRealtime, probe );
		for( const auto& [mode, colour] : expected )
		{
			for( const auto state : { QIcon::Off, QIcon::On } )
			{
				for( const auto dpr : Scales )
				{
					const auto ink = measure( renderIcon( icon, dpr, mode, state ), QRect( 0, 0, 20, 20 ), colour );
					QVERIFY( ink.colourSamples > 10 );
					QCOMPARE( ink.colourMismatches, 0 );
				}
			}
		}
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto themed = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRealtime, palette );
			for( const auto mode : Modes )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					const auto ink = measure( renderIcon( themed, 1.5, mode, state ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
					QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) ) );
				}
			}
		}
	}

	// at logical 20 px both states stay open outlines: the regions between the arcs and the slash stay clear, On's
	// centre stays clear, the slash is inked along its length in Off, and neither state is filled
	void realtimeIsOpenOutline()
	{
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRealtime, syntheticPalette( PaletteKind::Light ) );
		for( const auto dpr : Scales )
		{
			const auto where = QString::number( dpr );
			const auto off = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto on = renderIcon( icon, dpr, QIcon::Normal, QIcon::On ).convertToFormat( QImage::Format_ARGB32 );
			for( const auto& [state, image] : { qMakePair( "Off", off ), qMakePair( "On", on ) } )
			{
				QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( 14, 7.5, 2, 1 ) ) == 0, qPrintable( where ) );
				QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( 7.5, 14, 1, 2 ) ) == 0, qPrintable( where ) );
				const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				qInfo( "dpr %.2f: realtime %s ink %.1f %% of the 20 px box", dpr, state,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
			QVERIFY2( maxAlphaInUnits( on, dpr, QRectF( 10, 10, 4, 4 ) ) == 0, qPrintable( where ) );
			for( const auto& slash : { QRectF( 5.9, 5.9, 0.2, 0.2 ), QRectF( 11.9, 11.9, 0.2, 0.2 ), QRectF( 17.9, 17.9, 0.2, 0.2 ) } )
			{
				QVERIFY2( maxAlphaInUnits( off, dpr, slash ) >= 128, qPrintable( where ) );
			}
		}
	}

	// realtime Off differs from every other sheet icon; realtime On differs from every other sheet icon except Reboot,
	// whose reviewed refresh it reuses exactly
	void realtimeIsDistinct()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::SpotlightRealtime, palette );
		for( const auto dpr : Scales )
		{
			const auto off = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off );
			const auto on = renderIcon( icon, dpr, QIcon::Normal, QIcon::On );
			for( const auto& entry : sheetEntries() )
			{
				if( entry.feature == QStringLiteral("SpotlightRealtime") )
				{
					continue;
				}
				const auto other = renderIcon( iconFor( entry.feature, entry.parentKind, palette ), dpr, QIcon::Normal, entry.state );
				QVERIFY2( maxDifference( off, other ) > 128, qPrintable( entry.label ) );
				if( entry.feature == QStringLiteral("Reboot") )
				{
					QVERIFY2( maxDifference( on, other ) == 0, qPrintable( entry.label ) );
				}
				else
				{
					QVERIFY2( maxDifference( on, other ) > 128, qPrintable( entry.label ) );
				}
			}
		}
	}

	// the four V3at sheet columns are appended after the 43 reviewed columns, so every reviewed cell keeps its place
	void spotlightColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[42].feature, QStringLiteral("SlideshowSeparateWindow") );
		const QList<QPair<QString, QIcon::State>> appended{ { QStringLiteral("SpotlightAdd"), QIcon::Off },
															 { QStringLiteral("SpotlightRemove"), QIcon::Off },
															 { QStringLiteral("SpotlightRealtime"), QIcon::Off },
															 { QStringLiteral("SpotlightRealtime"), QIcon::On } };
		for( int i = 0; i < appended.size(); ++i )
		{
			QCOMPARE( entries[43 + i].feature, appended[i].first );
			QCOMPARE( entries[43 + i].parentKind, QStringLiteral("secondary") );
			QCOMPARE( entries[43 + i].state, appended[i].second );
		}
		for( int i = 0; i < 43; ++i )
		{
			QVERIFY2( entries[i].feature.startsWith( QStringLiteral("Spotlight") ) == false || entries[i].parentKind == QStringLiteral("view"),
					  qPrintable( entries[i].label ) );
		}
	}

	// V3av: V3AM-058, the Collect Files destination browse tool button and finished-state Open output directory
	// button. The role reuses the reviewed Load positions folder-open exactly, in both states and in every palette,
	// mode and scale, through both the paint and the pixmap paths
	void collectFilesOpenDestinationReusesLoadPositions()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::CollectFilesOpenDestination, palette );
			const auto loadPositions = TeacherActionIcons::forViewControl( ViewControl::LoadComputerPositions, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto colour = expectedColour( palette, mode );
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto image = renderIcon( icon, dpr, mode, state );
						QCOMPARE( maxDifference( image, renderIcon( loadPositions, dpr, mode, state ) ), 0 );
						QCOMPARE( maxDifference( image, referenceImage( QStringLiteral("folder-open"), colour, dpr ) ), 0 );
						QCOMPARE( maxDifference( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(),
												 loadPositions.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage() ), 0 );
					}
				}
			}
		}
	}

	// at logical 20 px the shared folder stays an open, unfilled outline inside its box: the front flap interior
	// (x 11..15, y 14.5..16 units) and the back panel interior above the flap (x 5.5..7, y 7.5..9) are clear,
	// nothing is clipped and coverage is below half
	void collectFilesOpenDestinationIsOpenOutline()
	{
		const auto icon = TeacherActionIcons::forSecondaryControl( SecondaryControl::CollectFilesOpenDestination, syntheticPalette( PaletteKind::Light ) );
		for( const auto dpr : Scales )
		{
			const auto where = QString::number( dpr );
			const auto image = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( 11, 14.5, 4, 1.5 ) ) == 0, qPrintable( where ) );
			QVERIFY2( maxAlphaInUnits( image, dpr, QRectF( 5.5, 7.5, 1.5, 1.5 ) ) == 0, qPrintable( where ) );
			const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
			QVERIFY2( ink.pixels > 0, qPrintable( where ) );
			QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
			const auto padded = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off, Pad );
			QVERIFY2( measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ) ).outside == 0, qPrintable( where ) );
		}
	}

	// the V3av column is appended after the 47 reviewed columns, so every reviewed cell keeps its place; its sheet cells
	// equal the Load positions cells (column 34) in every palette band, row and scale
	void collectFilesColumnAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[47].feature, QStringLiteral("CollectFilesOpenDestination") );
		QCOMPARE( entries[47].parentKind, QStringLiteral("secondary") );
		QCOMPARE( entries[47].state, QIcon::Off );
		QCOMPARE( entries[33].feature, QStringLiteral("LoadComputerPositions") );
		QCOMPARE( entries[46].feature, QStringLiteral("SpotlightRealtime") );
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					QVERIFY2( maxDifference( cellAt( 47 ), cellAt( 33 ) ) == 0,
							  qPrintable( QStringLiteral("dpr %1 band %2 row %3").arg( dpr ).arg( band ).arg( row ) ) );
				}
			}
		}
	}

	// V3aw: V3AM-064 to V3AM-069, the six Remote Access toolbar controls. Every role draws exactly its generated
	// reference glyph in its Off and On states, in every palette, mode and scale
	void remoteAccessControls_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < remoteRows().size(); ++i )
		{
			QTest::newRow( remoteRows()[i].id ) << i;
		}
	}

	void remoteAccessControls()
	{
		QFETCH( int, index );
		const auto& row = remoteRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = TeacherActionIcons::forRemoteAccessControl( row.control, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto colour = expectedColour( palette, mode );
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::Off ), referenceImage( QLatin1String( row.off ), colour, dpr ) ), 0 );
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::On ), referenceImage( QLatin1String( row.on ), colour, dpr ) ), 0 );
				}
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					QVERIFY( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, QIcon::Normal, state ).isNull() == false );
				}
			}
		}
	}

	// the five reused glyphs equal their reviewed consumers byte for byte in every palette, mode, state and scale,
	// through both the paint and the pixmap paths: View only (Off) the Remote View eye and Remote control (On) the
	// Remote Control pointer, Select screen the Locations & computers devices-pc, Screenshot the Screenshot camera,
	// Fullscreen (Off) the fullscreen share-mode maximize and Window (On) the window share-mode app-window
	void remoteAccessReusesReviewedGlyphs()
	{
		struct Reuse { RemoteAccessControl control; QIcon off; QIcon on; };
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto remoteView = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("RemoteView") ), palette );
			const auto remoteControl = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("RemoteControl") ), palette );
			const auto computers = TeacherActionIcons::forViewControl( ViewControl::ComputerSelectPanel, palette );
			const auto screenshot = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("Screenshot") ), palette );
			const auto fullscreen = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("ShareOwnScreenFullScreen"), QUuid::createUuid() ), palette );
			const auto window = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("ShareOwnScreenWindow"), QUuid::createUuid() ), palette );
			const QList<Reuse> reuses{
				{ RemoteAccessControl::ViewOnlyToggle, remoteView, remoteControl },
				{ RemoteAccessControl::SelectScreen, computers, computers },
				{ RemoteAccessControl::Screenshot, screenshot, screenshot },
				{ RemoteAccessControl::FullscreenToggle, fullscreen, window },
			};
			for( const auto& reuse : reuses )
			{
				const auto icon = TeacherActionIcons::forRemoteAccessControl( reuse.control, palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto& source = state == QIcon::Off ? reuse.off : reuse.on;
							QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, state ), renderIcon( source, dpr, mode, state ) ), 0 );
							const auto pixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage();
							const auto reviewed = source.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage();
							QCOMPARE( maxDifference( pixmap, reviewed ), 0 );
						}
					}
				}
			}
		}
	}

	// both checkable toggles are distinguishable without colour and never inverted: View only (Off) is not the
	// pointer and Remote control (On) not the eye; Fullscreen (Off) is not the app-window and Window (On) not the
	// maximize
	void remoteAccessToggleStatesAreDistinct()
	{
		struct Toggle { RemoteAccessControl control; const char* off; const char* on; };
		const QList<Toggle> toggles{
			{ RemoteAccessControl::ViewOnlyToggle, "eye", "pointer" },
			{ RemoteAccessControl::FullscreenToggle, "maximize", "app-window" },
		};
		for( const auto& toggle : toggles )
		{
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto icon = TeacherActionIcons::forRemoteAccessControl( toggle.control, palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						const auto where = QStringLiteral("%1 %2 dpr %3 mode %4").arg( QString::fromLatin1( toggle.off ) )
											   .arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
						const auto colour = expectedColour( palette, mode );
						const auto off = renderIcon( icon, dpr, mode, QIcon::Off );
						const auto on = renderIcon( icon, dpr, mode, QIcon::On );
						QVERIFY2( maxDifference( off, on ) > 128, qPrintable( where ) );
						QVERIFY2( alphaDifferenceRatio( off, on ) > 0.25, qPrintable( where ) );
						QVERIFY2( maxDifference( off, referenceImage( QLatin1String( toggle.on ), colour, dpr ) ) > 128, qPrintable( where ) );
						QVERIFY2( maxDifference( on, referenceImage( QLatin1String( toggle.off ), colour, dpr ) ) > 128, qPrintable( where ) );
					}
				}
			}
			const auto icon = TeacherActionIcons::forRemoteAccessControl( toggle.control, syntheticPalette( PaletteKind::Light ) );
			for( const auto dpr : Scales )
			{
				qInfo( "dpr %.2f: %s (Off) vs %s (On) coverage difference %.4f", dpr, toggle.off, toggle.on,
					   alphaDifferenceRatio( renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ), renderIcon( icon, dpr, QIcon::Normal, QIcon::On ) ) );
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states
	void remoteAccessPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : remoteRows() )
		{
			const auto icon = TeacherActionIcons::forRemoteAccessControl( row.control, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderIcon( icon, dpr, mode, state ), QRect( 0, 0, 20, 20 ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = TeacherActionIcons::forRemoteAccessControl( row.control, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderIcon( themed, 1.5, mode, state ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at logical 20 px both new glyphs stay open outlines inside their box: the keyboard frame (x 2..22, y 6..18), its
	// six keys and its space bar are inked while the gaps between neighbouring keys and between the key rows stay
	// under half coverage; the x's two arms are inked along their length and cross at the centre while the regions
	// between the arms stay clear; nothing is clipped and coverage is below half
	void remoteAccessNewGlyphsAreOpenOutline()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto keyboardIcon = TeacherActionIcons::forRemoteAccessControl( RemoteAccessControl::SendShortcut, palette );
		const auto exitIcon = TeacherActionIcons::forRemoteAccessControl( RemoteAccessControl::Exit, palette );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( const auto dpr : Scales )
		{
			const auto where = QString::number( dpr );
			const auto keyboard = renderIcon( keyboardIcon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto exitImage = renderIcon( exitIcon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			for( const auto& ink : { QPointF( 12, 6 ), QPointF( 12, 18 ), QPointF( 2, 12 ), QPointF( 22, 12 ), QPointF( 6, 10 ),
									 QPointF( 10, 10 ), QPointF( 14, 10 ), QPointF( 18, 10 ), QPointF( 6, 14 ), QPointF( 18, 14 ),
									 QPointF( 12, 14 ) } )
			{
				QVERIFY2( maxAlphaInUnits( keyboard, dpr, at( ink ) ) >= 128,
						  qPrintable( where + QStringLiteral(" keyboard ink %1,%2").arg( ink.x() ).arg( ink.y() ) ) );
			}
			for( const auto& gap : { QPointF( 8, 10 ), QPointF( 12, 10 ), QPointF( 16, 10 ), QPointF( 8, 12 ), QPointF( 16, 12 ),
									 QPointF( 8, 14 ), QPointF( 16, 14 ) } )
			{
				QVERIFY2( maxAlphaInUnits( keyboard, dpr, at( gap ) ) < 128,
						  qPrintable( where + QStringLiteral(" keyboard gap %1,%2").arg( gap.x() ).arg( gap.y() ) ) );
			}
			for( const auto& ink : { QPointF( 12, 12 ), QPointF( 7, 7 ), QPointF( 17, 7 ), QPointF( 7, 17 ), QPointF( 17, 17 ) } )
			{
				QVERIFY2( maxAlphaInUnits( exitImage, dpr, at( ink ) ) >= 128,
						  qPrintable( where + QStringLiteral(" x ink %1,%2").arg( ink.x() ).arg( ink.y() ) ) );
			}
			for( const auto& clear : { QPointF( 12, 7 ), QPointF( 12, 17 ), QPointF( 7, 12 ), QPointF( 17, 12 ) } )
			{
				QVERIFY2( maxAlphaInUnits( exitImage, dpr, at( clear ) ) == 0,
						  qPrintable( where + QStringLiteral(" x clear %1,%2").arg( clear.x() ).arg( clear.y() ) ) );
			}
			for( const auto& image : { keyboard, exitImage } )
			{
				const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
			}
			for( const auto& icon : { keyboardIcon, exitIcon } )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					const auto padded = renderIcon( icon, dpr, QIcon::Normal, state, Pad );
					QVERIFY2( measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ) ).outside == 0, qPrintable( where ) );
				}
			}
			qInfo( "dpr %.2f: keyboard ink %.1f %%, x ink %.1f %% of the 20 px box", dpr,
				   100.0 * measure( keyboard, QRect( 0, 0, 20, 20 ) ).alpha / ( 255.0 * keyboard.width() * keyboard.height() ),
				   100.0 * measure( exitImage, QRect( 0, 0, 20, 20 ) ).alpha / ( 255.0 * exitImage.width() * exitImage.height() ) );
		}
	}

	// keyboard and x differ from each other and from every other sheet icon at every scale; the reused roles are
	// pinned to their reviewed glyphs by remoteAccessReusesReviewedGlyphs()
	void remoteAccessNewGlyphsAreDistinct()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto keyboardIcon = TeacherActionIcons::forRemoteAccessControl( RemoteAccessControl::SendShortcut, palette );
		const auto exitIcon = TeacherActionIcons::forRemoteAccessControl( RemoteAccessControl::Exit, palette );
		for( const auto dpr : Scales )
		{
			const auto keyboard = renderIcon( keyboardIcon, dpr, QIcon::Normal, QIcon::Off );
			const auto exitImage = renderIcon( exitIcon, dpr, QIcon::Normal, QIcon::Off );
			QVERIFY( maxDifference( keyboard, exitImage ) > 128 );
			for( const auto& entry : sheetEntries() )
			{
				if( entry.parentKind == QStringLiteral("remote") &&
					( entry.feature == QStringLiteral("SendShortcut") || entry.feature == QStringLiteral("Exit") ) )
				{
					continue;
				}
				// (V3bi: the Toast close mask is the same Tabler x as Exit by design; toastColumnsAppended() bounds the
				// raster against this vector column, and it must still differ from the keyboard)
				if( entry.parentKind == QStringLiteral("toast") && entry.feature == QStringLiteral("Close") )
				{
					QVERIFY2( maxDifference( keyboard, renderIcon( iconFor( entry.feature, entry.parentKind, palette ), dpr, QIcon::Normal, entry.state ) ) > 128,
							  qPrintable( entry.label ) );
					continue;
				}
				const auto other = renderIcon( iconFor( entry.feature, entry.parentKind, palette ), dpr, QIcon::Normal, entry.state );
				QVERIFY2( maxDifference( keyboard, other ) > 128, qPrintable( entry.label ) );
				QVERIFY2( maxDifference( exitImage, other ) > 128, qPrintable( entry.label ) );
			}
		}
	}

	// the eight V3aw sheet columns are appended after the 48 reviewed columns, so every reviewed cell keeps its place;
	// each reused column's cells equal the reviewed column whose glyph it reuses in every palette band, row and scale
	// (the Checked row paints the On state, so there the View-only and Fullscreen Off columns match their On source)
	void remoteAccessColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[47].feature, QStringLiteral("CollectFilesOpenDestination") );
		QCOMPARE( entries[5].feature, QStringLiteral("RemoteControl") );
		QCOMPARE( entries[8].feature, QStringLiteral("RemoteView") );
		QCOMPARE( entries[10].feature, QStringLiteral("Screenshot") );
		QCOMPARE( entries[18].feature, QStringLiteral("ShareOwnScreenFullScreen") );
		QCOMPARE( entries[19].feature, QStringLiteral("ShareOwnScreenWindow") );
		QCOMPARE( entries[24].feature, QStringLiteral("ComputerSelectPanel") );
		// 0-based reviewed source columns for the Normal/Disabled/Selected rows and for the Checked row; -1 marks the
		// two new glyphs, which remoteAccessNewGlyphsAreDistinct() compares instead
		struct Column { const char* feature; QIcon::State state; int source; int checkedSource; };
		const QList<Column> appended{
			{ "ViewOnlyToggle", QIcon::Off, 8, 5 },
			{ "ViewOnlyToggle", QIcon::On, 5, 5 },
			{ "SelectScreen", QIcon::Off, 24, 24 },
			{ "SendShortcut", QIcon::Off, -1, -1 },
			{ "Screenshot", QIcon::Off, 10, 10 },
			{ "FullscreenToggle", QIcon::Off, 18, 19 },
			{ "FullscreenToggle", QIcon::On, 19, 19 },
			{ "Exit", QIcon::Off, -1, -1 },
		};
		for( int i = 0; i < appended.size(); ++i )
		{
			QCOMPARE( entries[48 + i].feature, QString::fromLatin1( appended[i].feature ) );
			QCOMPARE( entries[48 + i].parentKind, QStringLiteral("remote") );
			QCOMPARE( entries[48 + i].state, appended[i].state );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < appended.size(); ++i )
					{
						const auto source = row == 3 ? appended[i].checkedSource : appended[i].source;
						if( source < 0 )
						{
							continue;
						}
						QVERIFY2( maxDifference( cellAt( 48 + i ), cellAt( source ) ) == 0,
								  qPrintable( QStringLiteral("column %1 dpr %2 band %3 row %4").arg( 49 + i ).arg( dpr ).arg( band ).arg( row ) ) );
					}
				}
			}
		}
	}

	void fileTransferStates_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < transferRows().size(); ++i )
		{
			QTest::newRow( transferRows()[i].id ) << i;
		}
	}

	// each state draws its exact generated glyph in the supplied palette's colours and identically in Off and On, in
	// every palette, mode and scale, through both the paint and the pixmap paths
	void fileTransferStates()
	{
		QFETCH( int, index );
		const auto& row = transferRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = TeacherActionIcons::forFileTransferState( row.state, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto off = renderIcon( icon, dpr, mode, QIcon::Off );
					QCOMPARE( maxDifference( off, referenceImage( QLatin1String( row.glyph ), expectedColour( palette, mode ), dpr ) ), 0 );
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::On ), off ), 0 );
					const auto offPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off ).toImage();
					const auto onPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::On ).toImage();
					QVERIFY( offPixmap.isNull() == false );
					QCOMPARE( maxDifference( offPixmap, onPixmap ), 0 );
				}
			}
		}
	}

	// the four states are shape-distinct without colour in every palette, mode and scale, so a swapped or collapsed
	// mapping fails; clock and circle-check share their 9-unit circle, hence the 0.15 coverage floor (the hands and the
	// check are probed separately by fileTransferStatesAreOpenOutline()); each state also differs from every other
	// sheet icon
	void fileTransferStatesAreDistinct()
	{
		const auto& rows = transferRows();
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( int a = 0; a < rows.size(); ++a )
					{
						for( int b = a + 1; b < rows.size(); ++b )
						{
							const auto where = QStringLiteral("%1 vs %2 %3 dpr %4 mode %5").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ),
																					 paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
							const auto first = renderIcon( TeacherActionIcons::forFileTransferState( rows[a].state, palette ), dpr, mode, QIcon::Off );
							const auto second = renderIcon( TeacherActionIcons::forFileTransferState( rows[b].state, palette ), dpr, mode, QIcon::Off );
							QVERIFY2( maxDifference( first, second ) > 128, qPrintable( where ) );
							QVERIFY2( alphaDifferenceRatio( first, second ) > 0.15, qPrintable( where ) );
						}
					}
				}
			}
		}
		const auto palette = syntheticPalette( PaletteKind::Light );
		for( const auto dpr : Scales )
		{
			for( const auto& row : rows )
			{
				const auto image = renderIcon( TeacherActionIcons::forFileTransferState( row.state, palette ), dpr, QIcon::Normal, QIcon::Off );
				for( const auto& entry : sheetEntries() )
				{
					// (V3bb: the Administration File transfer page reuses the Transferring glyph by design; see
					// administrationPagesReuseReviewedGlyphs())
					if( entry.parentKind == QStringLiteral("transfer") || ( entry.parentKind == QStringLiteral("admin") && entry.feature == QStringLiteral("FileTransfer") ) )
					{
						continue;
					}
					// (V3bd: the Access Control Test command reuses the Finished glyph by design; see
					// accessControlCommandsReuseReviewedGlyphs())
					if( entry.parentKind == QStringLiteral("access") && entry.feature == QStringLiteral("Test") &&
						row.state == FileTransferState::Finished )
					{
						continue;
					}
					// (V3be: the General page Test authentication command reuses the Finished glyph by design; see
					// generalCommandsReuseReviewedGlyphs())
					if( entry.parentKind == QStringLiteral("general") && entry.feature == QStringLiteral("TestAuthentication") &&
						row.state == FileTransferState::Finished )
					{
						continue;
					}
					// (V3bi: the Toast Success mask is the same Tabler circle-check as Finished by design;
					// toastColumnsAppended() bounds the raster against this vector column)
					if( entry.parentKind == QStringLiteral("toast") && entry.feature == QStringLiteral("Success") &&
						row.state == FileTransferState::Finished )
					{
						continue;
					}
					const auto other = renderIcon( iconFor( entry.feature, entry.parentKind, palette ), dpr, QIcon::Normal, entry.state );
					QVERIFY2( maxDifference( image, other ) > 128, qPrintable( QLatin1String( row.glyph ) + QStringLiteral(" vs ") + entry.label ) );
				}
			}
			for( int a = 0; a < rows.size(); ++a )
			{
				for( int b = a + 1; b < rows.size(); ++b )
				{
					qInfo( "dpr %.2f: %s vs %s coverage difference %.4f", dpr, rows[a].glyph, rows[b].glyph,
						   alphaDifferenceRatio( renderIcon( TeacherActionIcons::forFileTransferState( rows[a].state, palette ), dpr, QIcon::Normal, QIcon::Off ),
												 renderIcon( TeacherActionIcons::forFileTransferState( rows[b].state, palette ), dpr, QIcon::Normal, QIcon::Off ) ) );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states
	void fileTransferStatePaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : transferRows() )
		{
			const auto icon = TeacherActionIcons::forFileTransferState( row.state, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderIcon( icon, dpr, mode, state ), QRect( 0, 0, 20, 20 ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = TeacherActionIcons::forFileTransferState( row.state, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderIcon( themed, 1.5, mode, state ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at logical 20 px every state stays an open outline inside its box: the probed strokes are inked, the probed
	// gaps between strokes stay under half coverage and the probed clear areas are untouched; nothing is clipped in
	// either state and coverage is below half. Points are 24-unit view-box coordinates of the pinned sources.
	void fileTransferStatesAreOpenOutline()
	{
		struct Probe { FileTransferState state; QList<QPointF> ink; QList<QPointF> gaps; QList<QPointF> clear; };
		const QList<Probe> probes{
			// clock: the 9-unit circle and both hands are inked; the face beside the hands stays open
			{ FileTransferState::Scheduled,
			  { QPointF( 12, 3 ), QPointF( 3, 12 ), QPointF( 21, 12 ), QPointF( 12, 21 ), QPointF( 12, 9 ), QPointF( 13.5, 13.5 ) },
			  { QPointF( 11, 14 ) },
			  { QPointF( 8, 8 ), QPointF( 16, 8 ), QPointF( 8, 15 ) } },
			// transfer: both shafts and both arrowheads are inked; the gap between the shafts stays open and the corners
			// opposite the arrowheads stay clear
			{ FileTransferState::Transferring,
			  { QPointF( 12, 10 ), QPointF( 12, 14 ), QPointF( 6.75, 7 ), QPointF( 17.25, 17 ) },
			  { QPointF( 12, 12 ) },
			  { QPointF( 17, 6 ), QPointF( 7, 18 ) } },
			// hourglass: both caps, both bulb lines and the waist are inked; the inside of each bulb stays open and the
			// area beside the waist stays clear
			{ FileTransferState::Waiting,
			  { QPointF( 12, 3 ), QPointF( 12, 21 ), QPointF( 12, 7 ), QPointF( 12, 17 ), QPointF( 12, 12 ) },
			  { QPointF( 12, 9.5 ), QPointF( 12, 14.5 ) },
			  { QPointF( 3, 12 ), QPointF( 21, 12 ) } },
			// circle-check: the 9-unit circle and the check are inked; where the clock has its hour hand the face stays open
			{ FileTransferState::Finished,
			  { QPointF( 12, 3 ), QPointF( 3, 12 ), QPointF( 21, 12 ), QPointF( 12, 21 ), QPointF( 9, 12 ), QPointF( 11, 14 ), QPointF( 15, 10 ) },
			  { QPointF( 12, 9 ) },
			  { QPointF( 12, 7 ), QPointF( 8, 16 ), QPointF( 16, 15 ) } },
		};
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( const auto& probe : probes )
		{
			const auto icon = TeacherActionIcons::forFileTransferState( probe.state, palette );
			const auto glyph = QLatin1String( transferRows()[int( probe.state )].glyph );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 dpr %2").arg( glyph ).arg( dpr );
				const auto image = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : probe.ink )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) >= 128,
							  qPrintable( where + QStringLiteral(" ink %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				for( const auto& point : probe.gaps )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) < 128,
							  qPrintable( where + QStringLiteral(" gap %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				for( const auto& point : probe.clear )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, 20, 20 ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					const auto padded = renderIcon( icon, dpr, QIcon::Normal, state, Pad );
					QVERIFY2( measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ) ).outside == 0, qPrintable( where ) );
				}
				qInfo( "dpr %.2f: %s ink %.1f %% of the 20 px box", dpr, qPrintable( glyph ),
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// the four V3ax sheet columns are appended after the 56 reviewed columns, so every reviewed cell keeps its place;
	// each new column differs from every other column in every palette band, row and scale. Sheet cells are opaque, so
	// two different glyphs differ by the full ink/background contrast of their row somewhere; the weakest on the sheet
	// is about 96 (Dark Disabled) and identical glyphs differ by 0, hence the 64 floor
	void fileTransferColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[55].feature, QStringLiteral("Exit") );
		QCOMPARE( entries[55].parentKind, QStringLiteral("remote") );
		for( int i = 0; i < transferRows().size(); ++i )
		{
			QCOMPARE( entries[56 + i].feature, QString::fromLatin1( transferRows()[i].role ) );
			QCOMPARE( entries[56 + i].parentKind, QStringLiteral("transfer") );
			QCOMPARE( entries[56 + i].state, QIcon::Off );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int column = 56; column < 60; ++column )
					{
						const auto cell = cellAt( column );
						for( int other = 0; other < 60; ++other )
						{
							if( other != column )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64,
										  qPrintable( QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( column + 1 ).arg( other + 1 )
													  .arg( dpr ).arg( band ).arg( row ) ) );
							}
						}
					}
				}
			}
		}
	}

	void unmappedFeatureKeepsIconUrl()
	{
		QTemporaryDir dir;
		QVERIFY( dir.isValid() );
		const auto file = dir.filePath( QStringLiteral("fallback.png") );
		QImage marker( 32, 32, QImage::Format_ARGB32 );
		marker.fill( QColor( 255, 0, 255 ) );
		QVERIFY( marker.save( file ) );

		const auto palette = syntheticPalette( PaletteKind::Light );
		// a real Teacher feature outside the V3an map and an arbitrary one keep QIcon( feature.iconUrl() )
		for( const auto& name : { QStringLiteral("InputDevicesLock"), QStringLiteral("SomeFutureFeature") } )
		{
			const auto icon = TeacherActionIcons::forFeature( makeFeature( name, {}, file ), palette );
			QVERIFY( icon.isNull() == false );
			const auto image = icon.pixmap( QSize( LogicalSize, LogicalSize ), 1.0 ).toImage().convertToFormat( QImage::Format_ARGB32 );
			QCOMPARE( image.pixelColor( 10, 10 ), QColor( 255, 0, 255 ) );
			QVERIFY( TeacherActionIcons::forFeature( makeFeature( name ), palette ).isNull() );
		}
		// the mapped name wins over its upstream iconUrl
		const auto mapped = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("Reboot"), {}, file ), palette );
		QVERIFY( measure( renderIcon( mapped, 1.0, QIcon::Normal, QIcon::Off ), QRect( 0, 0, 20, 20 ), QColor( 255, 0, 255 ) ).colourMismatches > 0 );
	}

	// Remote Control is the bare pinned pointer at normal 24x24 geometry and the family stroke, distinct from
	// Monitor (device-desktop) and Remote View (eye)
	void remoteControlIsBarePointer()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto colour = expectedColour( palette, QIcon::Normal );
		const auto remoteControl = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("RemoteControl") ), palette );
		const auto monitor = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("MonitoringMode") ), palette );
		const auto remoteView = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("RemoteView") ), palette );
		for( const auto dpr : Scales )
		{
			const auto image = renderIcon( remoteControl, dpr, QIcon::Normal, QIcon::Off );
			QCOMPARE( maxDifference( image, referenceImage( QStringLiteral("pointer"), colour, dpr ) ), 0 );
			QVERIFY( maxDifference( image, renderIcon( monitor, dpr, QIcon::Normal, QIcon::Off ) ) > 128 );
			QVERIFY( maxDifference( image, renderIcon( remoteView, dpr, QIcon::Normal, QIcon::Off ) ) > 128 );
		}
		// full size: the pointer spans its whole upstream 4..20 unit box
		const auto bounds = referencePath( QStringLiteral("pointer") ).boundingRect();
		QVERIFY( bounds.width() > 15.9 && bounds.height() > 15.9 );
	}

	void lockOnDiffersFromOff()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto lock = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("ScreenLock") ), palette );
		for( const auto dpr : Scales )
		{
			for( const auto mode : Modes )
			{
				QVERIFY( maxDifference( renderIcon( lock, dpr, mode, QIcon::Off ), renderIcon( lock, dpr, mode, QIcon::On ) ) > 128 );
				QCOMPARE( maxDifference( renderIcon( lock, dpr, mode, QIcon::On ),
										 referenceImage( QStringLiteral("lock-open"), expectedColour( palette, mode ), dpr ) ), 0 );
			}
		}
	}

	void broadcastStableAcrossStates()
	{
		const auto palette = syntheticPalette( PaletteKind::Dark );
		for( const auto& name : { QStringLiteral("Demo"), QStringLiteral("MonitoringMode"), QStringLiteral("RemoteControl") } )
		{
			const auto icon = TeacherActionIcons::forFeature( makeFeature( name ), palette );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					QCOMPARE( maxDifference( renderIcon( icon, dpr, mode, QIcon::Off ), renderIcon( icon, dpr, mode, QIcon::On ) ), 0 );
				}
			}
		}
	}

	void paletteRoles()
	{
		// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{ { QIcon::Normal, QColor( 192, 0, 0 ) },
														  { QIcon::Active, QColor( 192, 0, 0 ) },
														  { QIcon::Disabled, QColor( 0, 160, 0 ) },
														  { QIcon::Selected, QColor( 0, 0, 192 ) } };
		for( const auto& icon : { TeacherActionIcons::forFeature( makeFeature( QStringLiteral("TextMessage") ), probe ),
								  TeacherActionIcons::more( probe ) } )
		{
			for( const auto& [mode, colour] : expected )
			{
				const auto ink = measure( renderIcon( icon, 2.0, mode, QIcon::Off ), QRect( 0, 0, 20, 20 ), colour );
				QVERIFY( ink.colourSamples > 20 );
				QCOMPARE( ink.colourMismatches, 0 );
			}
		}
		// the same icon follows each synthetic palette; nothing is hard-coded
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("ScreenLock") ), palette );
			for( const auto mode : Modes )
			{
				const auto ink = measure( renderIcon( icon, 1.5, mode, QIcon::On ), QRect( 0, 0, 20, 20 ), expectedColour( palette, mode ) );
				QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) ) );
			}
		}
	}

	// no clipping at logical 20 px for every icon, mode and state at 100/125/150/200 %
	void noClipping()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		for( const auto& entry : sheetEntries() )
		{
			const auto icon = iconFor( entry.feature, entry.parentKind, palette );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderIcon( icon, dpr, mode, state, Pad );
						const auto ink = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						const auto where = QStringLiteral("%1 dpr %2 mode %3 state %4").arg( entry.label ).arg( dpr ).arg( int( mode ) ).arg( int( state ) );
						QVERIFY2( ink.pixels > 0, qPrintable( where ) );
						QVERIFY2( ink.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( ink.colourMismatches == 0, qPrintable( where ) );

						// the pixmap path renders the same image at the requested device pixel ratio
						const auto pixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state );
						QCOMPARE( pixmap.devicePixelRatio(), dpr );
						QCOMPARE( pixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
						const auto device = qRound( Pad * dpr );
						auto cropped = padded.copy( device, device, pixmap.width(), pixmap.height() );
						cropped.setDevicePixelRatio( dpr );
						QVERIFY2( maxDifference( pixmap.toImage(), cropped ) <= 2, qPrintable( where ) );
					}
				}
			}
		}
	}

	void strokeIsOnePointSevenFiveLogicalPixels()
	{
		// power's vertical bar (x = 12 units) crossed at y = 8 units; the arc is far away at this height
		const auto icon = TeacherActionIcons::forFeature( makeFeature( QStringLiteral("PowerOn") ), syntheticPalette( PaletteKind::Light ) );
		for( const auto dpr : Scales )
		{
			const auto image = renderIcon( icon, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const int row = qFloor( 8.0 * LogicalSize / 24.0 * dpr );
			const int centre = qRound( 12.0 * LogicalSize / 24.0 * dpr );
			qreal coverage = 0;
			for( int col = centre - qCeil( 3 * dpr ); col <= centre + qCeil( 3 * dpr ); ++col )
			{
				coverage += qAlpha( image.pixel( col, row ) ) / 255.0;
			}
			QVERIFY2( qAbs( coverage - 1.75 * dpr ) < 0.15 * dpr, qPrintable( QString::number( coverage ) ) );
		}
	}

	void uniformScalingInNonSquareRect()
	{
		const auto icon = TeacherActionIcons::more( syntheticPalette( PaletteKind::Light ) );
		auto image = canvas( 1.0, QSize( 40, 20 ) );
		QPainter painter( &image );
		icon.paint( &painter, QRect( 0, 0, 40, 20 ), Qt::AlignCenter, QIcon::Normal, QIcon::Off );
		painter.end();
		const auto ink = measure( image, QRect( 10, 0, 20, 20 ) );
		QVERIFY( ink.pixels > 0 );
		QCOMPARE( ink.outside, 0 );
	}

	void neutralFacadeMatchesLegacyApi_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < facadeRows().size(); ++i )
		{
			QTest::newRow( facadeRows()[i].id ) << i;
		}
	}

	// V3ba: each representative role renders pixel-identically (maximum per-channel difference 0) through
	// NextSpaceActionIcons and TeacherActionIcons in every synthetic palette, mode, scale and state, through both the
	// paint and the pixmap paths; every rendered image carries ink, so two empty icons cannot pass
	void neutralFacadeMatchesLegacyApi()
	{
		QFETCH( int, index );
		const auto& row = facadeRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto neutral = row.neutral( palette );
			const auto legacy = row.legacy( palette );
			QVERIFY( neutral.isNull() == false );
			QVERIFY( legacy.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto where = QStringLiteral("%1 dpr %2 mode %3 state %4")
											   .arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) ).arg( int( state ) );
						const auto image = renderIcon( neutral, dpr, mode, state );
						QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
						QVERIFY2( maxDifference( image, renderIcon( legacy, dpr, mode, state ) ) == 0, qPrintable( where ) );
						const auto neutralPixmap = neutral.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage();
						QVERIFY2( neutralPixmap.isNull() == false, qPrintable( where ) );
						QVERIFY2( maxDifference( neutralPixmap,
												 legacy.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage() ) == 0,
								  qPrintable( where ) );
					}
				}
			}
		}
	}

	void administrationPages_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < administrationRows().size(); ++i )
		{
			QTest::newRow( administrationRows()[i].id ) << i;
		}
	}

	// V3bb: at the page selector's 48 logical px each Administration role draws exactly its generated glyph in the
	// supplied palette's colours and identically in Off and On, in every palette, mode and scale, through both the paint
	// and the pixmap paths
	void administrationPages()
	{
		QFETCH( int, index );
		const auto& row = administrationRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationPage( row.page, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( icon, SelectorSize, dpr, mode, QIcon::Off );
					QVERIFY2( maxDifference( off, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), SelectorSize, dpr ) ) == 0,
							  qPrintable( where ) );
					QVERIFY2( maxDifference( renderAt( icon, SelectorSize, dpr, mode, QIcon::On ), off ) == 0, qPrintable( where ) );
					const auto offPixmap = icon.pixmap( QSize( SelectorSize, SelectorSize ), dpr, mode, QIcon::Off );
					const auto onPixmap = icon.pixmap( QSize( SelectorSize, SelectorSize ), dpr, mode, QIcon::On );
					QCOMPARE( offPixmap.devicePixelRatio(), dpr );
					QCOMPARE( offPixmap.size(), ( QSizeF( SelectorSize, SelectorSize ) * dpr ).toSize() );
					QVERIFY2( maxDifference( offPixmap.toImage(), onPixmap.toImage() ) == 0, qPrintable( where ) );
					QVERIFY2( maxDifference( offPixmap.toImage(), off ) <= 2, qPrintable( where ) );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 48 logical px; and the same role follows each synthetic palette
	void administrationPagePaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : administrationRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationPage( row.page, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, SelectorSize, dpr, mode, state ), QRect( 0, 0, SelectorSize, SelectorSize ), colour );
						QVERIFY2( ink.colourSamples > 20, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationPage( row.page, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, SelectorSize, 1.5, mode, state ), QRect( 0, 0, SelectorSize, SelectorSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 20 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 48 logical px every role stays an open outline inside its box: nothing is drawn outside the requested rectangle
	// in any mode or state, the probed interior points of the pinned sources (24-unit view-box coordinates) stay
	// untouched and coverage stays below 40 % of the box
	void administrationPagesAreOpenOutline()
	{
		// one list per administrationRows() entry, in the same order
		const QList<QList<QPointF>> clear{
			{ QPointF( 12, 12 ) },                                                     // settings: inside the centre circle
			{ QPointF( 12, 10 ) },                                                     // device-desktop: the screen
			{ QPointF( 6.5, 7 ), QPointF( 17, 13 ) },                                  // presentation: the board beside the chart
			{ QPointF( 8.5, 9 ), QPointF( 15.5, 9 ) },                                 // shield-lock: inside the shield beside the lock
			{ QPointF( 5, 6 ), QPointF( 19, 19 ) },                                    // key: corners away from head and shaft
			{ QPointF( 17, 12.5 ), QPointF( 6, 8 ) },                                  // devices-pc: monitor and tower interiors
			{ QPointF( 7, 10 ) },                                                      // screen-share: the screen
			{ QPointF( 7, 7 ), QPointF( 12, 12 ) },                                    // apps: a tile and the gap between tiles
			{ QPointF( 12, 12 ) },                                                     // transfer: between the two shafts
			{ QPointF( 8, 8.5 ), QPointF( 16, 8 ), QPointF( 8, 15.5 ), QPointF( 16, 16 ) }, // brand-windows: the four panes
			{ QPointF( 12, 5 ), QPointF( 5, 19 ), QPointF( 19, 19 ), QPointF( 5, 10 ) },   // hierarchy-2: the three boxes and outside
		};
		QCOMPARE( clear.size(), administrationRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < administrationRows().size(); ++index )
		{
			const auto& row = administrationRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationPage( row.page, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 dpr %2").arg( QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, SelectorSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaAt( image, SelectorSize, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, SelectorSize, SelectorSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() * 2 / 5, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, SelectorSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, SelectorSize, SelectorSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s ink %.1f %% of the 48 px box", dpr, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// the eleven roles draw eleven different glyphs: every pair is shape-distinct without colour in every palette, mode
	// and scale at 48 logical px, so a swapped or collapsed mapping fails. Each pair's weakest coverage difference over
	// all of them is logged before it is checked. The reviewed device-desktop and screen-share share their monitor frame
	// and stand (weakest measured difference about 0.147); every other pair differs by more than 0.5, and identical
	// glyphs give 0, hence the 0.10 floor
	void administrationPagesAreDistinct()
	{
		const auto& rows = administrationRows();
		QSet<QString> glyphs;
		QSet<QString> roles;
		for( const auto& row : rows )
		{
			glyphs.insert( QLatin1String( row.glyph ) );
			roles.insert( QLatin1String( row.role ) );
		}
		QCOMPARE( glyphs.size(), 11 );
		QCOMPARE( roles.size(), 11 );
		constexpr qreal CoverageFloor = 0.10;
		QMap<QString, qreal> weakest;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( int a = 0; a < rows.size(); ++a )
					{
						const auto first = renderAt( NextSpaceActionIcons::forAdministrationPage( rows[a].page, palette ), SelectorSize, dpr, mode, QIcon::Off );
						for( int b = a + 1; b < rows.size(); ++b )
						{
							const auto where = QStringLiteral("%1 vs %2 %3 dpr %4 mode %5").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ),
																					 paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
							const auto second = renderAt( NextSpaceActionIcons::forAdministrationPage( rows[b].page, palette ), SelectorSize, dpr, mode, QIcon::Off );
							QVERIFY2( maxDifference( first, second ) > 128, qPrintable( where ) );
							const auto pair = QStringLiteral("%1 vs %2").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ) );
							const auto ratio = alphaDifferenceRatio( first, second );
							weakest[pair] = weakest.contains( pair ) ? qMin( weakest[pair], ratio ) : ratio;
						}
					}
				}
			}
		}
		QCOMPARE( weakest.size(), 55 );
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			qInfo( "48 px: %s weakest coverage difference %.4f", qPrintable( it.key() ), it.value() );
		}
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			QVERIFY2( it.value() > CoverageFloor, qPrintable( it.key() ) );
		}
	}

	// the five reused glyphs are the reviewed generated data itself: each page renders pixel-identically to its reviewed
	// role (Monitor, the Locations & computers panel toggle, Broadcast, the Start Application parent and the Transferring
	// state) in every palette, mode, scale and state, through both the paint and the pixmap paths
	void administrationPagesReuseReviewedGlyphs()
	{
		const auto startApp = makeFeature( QStringLiteral("StartApp") );
		struct Reuse { AdministrationPage page; std::function<QIcon( const QPalette& )> reviewed; };
		const QList<Reuse> reuses{
			{ AdministrationPage::StudentAgent, []( const QPalette& p ) { return TeacherActionIcons::forFeature( makeFeature( QStringLiteral("MonitoringMode") ), p ); } },
			{ AdministrationPage::LocationsAndComputers, []( const QPalette& p ) { return TeacherActionIcons::forViewControl( ViewControl::ComputerSelectPanel, p ); } },
			{ AdministrationPage::Broadcast, []( const QPalette& p ) { return TeacherActionIcons::forFeature( makeFeature( QStringLiteral("Demo") ), p ); } },
			{ AdministrationPage::ApplicationsAndWebsites, [&startApp]( const QPalette& p ) { return TeacherActionIcons::forFeature( startApp, p ); } },
			{ AdministrationPage::FileTransfer, []( const QPalette& p ) { return TeacherActionIcons::forFileTransferState( FileTransferState::Transferring, p ); } },
		};
		for( const auto& reuse : reuses )
		{
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto page = NextSpaceActionIcons::forAdministrationPage( reuse.page, palette );
				const auto reviewed = reuse.reviewed( palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( int( reuse.page ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto image = renderAt( page, SelectorSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, SelectorSize, SelectorSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, renderAt( reviewed, SelectorSize, dpr, mode, state ) ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( page.pixmap( QSize( SelectorSize, SelectorSize ), dpr, mode, state ).toImage(),
													 reviewed.pixmap( QSize( SelectorSize, SelectorSize ), dpr, mode, state ).toImage() ) == 0,
									  qPrintable( where ) );
						}
					}
				}
			}
		}
	}

	// the eleven V3bb sheet columns are appended after the 60 reviewed columns, so every reviewed cell keeps its place;
	// each new column equals exactly the reviewed columns that draw the same glyph and differs from every other column
	// (including the other new ones) in every palette band, row and scale, with the 64 floor of
	// fileTransferColumnsAppended()
	void administrationColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[59].feature, QStringLiteral("Finished") );
		QCOMPARE( entries[59].parentKind, QStringLiteral("transfer") );
		QCOMPARE( entries[0].feature, QStringLiteral("MonitoringMode") );
		QCOMPARE( entries[1].feature, QStringLiteral("Demo") );
		QCOMPARE( entries[2].feature, QStringLiteral("Demo") );
		QCOMPARE( entries[11].feature, QStringLiteral("StartApp") );
		QCOMPARE( entries[24].feature, QStringLiteral("ComputerSelectPanel") );
		QCOMPARE( entries[50].feature, QStringLiteral("SelectScreen") );
		QCOMPARE( entries[57].feature, QStringLiteral("Transferring") );
		// 0-based reviewed columns drawing the same glyph in every row; empty for the six new glyphs
		const QList<QList<int>> sameGlyph{ {}, { 0 }, {}, {}, {}, { 24, 50 }, { 1, 2 }, { 11 }, { 57 }, {}, {} };
		QCOMPARE( sameGlyph.size(), administrationRows().size() );
		for( int i = 0; i < administrationRows().size(); ++i )
		{
			QCOMPARE( entries[60 + i].feature, QString::fromLatin1( administrationRows()[i].role ) );
			QCOMPARE( entries[60 + i].parentKind, QStringLiteral("admin") );
			QCOMPARE( entries[60 + i].state, QIcon::Off );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < administrationRows().size(); ++i )
					{
						const auto cell = cellAt( 60 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 60 + i )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 61 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
	}

	void administrationCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < commandRows().size(); ++i )
		{
			QTest::newRow( commandRows()[i].id ) << i;
		}
	}

	// V3bc: at the menus' logical 20 px each Administration command draws exactly its generated glyph in the supplied
	// palette's colours and identically in Off and On, in every palette, mode and scale, through both the paint and the
	// pixmap paths
	void administrationCommands()
	{
		QFETCH( int, index );
		const auto& row = commandRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( icon, LogicalSize, dpr, mode, QIcon::Off );
					QVERIFY2( maxDifference( off, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
							  qPrintable( where ) );
					QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, QIcon::On ), off ) == 0, qPrintable( where ) );
					const auto offPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off );
					const auto onPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::On );
					QCOMPARE( offPixmap.devicePixelRatio(), dpr );
					QCOMPARE( offPixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
					QVERIFY2( maxDifference( offPixmap.toImage(), onPixmap.toImage() ) == 0, qPrintable( where ) );
					QVERIFY2( maxDifference( offPixmap.toImage(), off ) <= 2, qPrintable( where ) );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 20 logical px; and the same role follows each synthetic palette
	void administrationCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : commandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px every command stays an open outline inside its box: nothing is drawn outside the requested
	// rectangle in any mode or state, the probed interiors of the reviewed sources (24-unit view-box rectangles; the
	// folder's are those of collectFilesOpenDestinationIsOpenOutline()) stay untouched and coverage stays below half of
	// the box, as in the reviewed 20-px open-outline tests
	void administrationCommandsAreOpenOutline()
	{
		// one list per commandRows() entry, in the same order
		const QList<QList<QRectF>> clear{
			{ QRectF( 6.9, 10.9, 0.2, 0.2 ), QRectF( 16.9, 10.9, 0.2, 0.2 ), QRectF( 16.9, 16.9, 0.2, 0.2 ) }, // device-floppy: beside the shutter and the hub
			{ QRectF( 11, 14.5, 4, 1.5 ), QRectF( 5.5, 7.5, 1.5, 1.5 ) },                                       // folder-open: front flap and back panel
			{ QRectF( 6.9, 11.9, 0.2, 0.2 ), QRectF( 16.9, 11.9, 0.2, 0.2 ), QRectF( 11.9, 5.9, 0.2, 0.2 ) },  // info-circle: inside the ring beside the i and above the dot
			{ QRectF( 7.4, 9.9, 0.2, 0.2 ), QRectF( 16.4, 9.9, 0.2, 0.2 ), QRectF( 11.9, 8.9, 0.2, 0.2 ) },    // trash: inside the bin beside and above the bars
		};
		QCOMPARE( clear.size(), commandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		for( int index = 0; index < commandRows().size(); ++index )
		{
			const auto& row = commandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 dpr %2").arg( QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& units : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, units ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( units.x() ).arg( units.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s ink %.1f %% of the 20 px box", dpr, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// the four commands draw four different glyphs: every pair is shape-distinct without colour in every palette, mode
	// and scale at 20 logical px, so a swapped or collapsed mapping fails. Each pair's weakest coverage difference over all
	// of them is logged before it is checked against the 0.10 floor of administrationPagesAreDistinct() (identical glyphs
	// give 0)
	void administrationCommandsAreDistinct()
	{
		const auto& rows = commandRows();
		QSet<QString> glyphs;
		QSet<QString> roles;
		for( const auto& row : rows )
		{
			glyphs.insert( QLatin1String( row.glyph ) );
			roles.insert( QLatin1String( row.role ) );
		}
		QCOMPARE( glyphs.size(), 4 );
		QCOMPARE( roles.size(), 4 );
		constexpr qreal CoverageFloor = 0.10;
		QMap<QString, qreal> weakest;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( int a = 0; a < rows.size(); ++a )
					{
						const auto first = renderAt( NextSpaceActionIcons::forAdministrationCommand( rows[a].command, palette ), LogicalSize, dpr, mode, QIcon::Off );
						for( int b = a + 1; b < rows.size(); ++b )
						{
							const auto where = QStringLiteral("%1 vs %2 %3 dpr %4 mode %5").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ),
																					 paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
							const auto second = renderAt( NextSpaceActionIcons::forAdministrationCommand( rows[b].command, palette ), LogicalSize, dpr, mode, QIcon::Off );
							QVERIFY2( maxDifference( first, second ) > 128, qPrintable( where ) );
							const auto pair = QStringLiteral("%1 vs %2").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ) );
							const auto ratio = alphaDifferenceRatio( first, second );
							weakest[pair] = weakest.contains( pair ) ? qMin( weakest[pair], ratio ) : ratio;
						}
					}
				}
			}
		}
		QCOMPARE( weakest.size(), 6 );
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			qInfo( "20 px: %s weakest coverage difference %.4f", qPrintable( it.key() ), it.value() );
		}
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			QVERIFY2( it.value() > CoverageFloor, qPrintable( it.key() ) );
		}
	}

	// the four commands reuse the reviewed generated data itself: each renders pixel-identically to its reviewed Teacher
	// role (Save and Load computer positions, the About header button and the Screenshots-panel Delete button) in every
	// palette, mode, scale and state, through both the paint and the pixmap paths. Only this test names the Teacher roles;
	// the Configurator uses AdministrationCommand alone
	void administrationCommandsReuseReviewedGlyphs()
	{
		struct Reuse { AdministrationCommand command; std::function<QIcon( const QPalette& )> reviewed; };
		const QList<Reuse> reuses{
			{ AdministrationCommand::SaveSettings, []( const QPalette& p ) { return TeacherActionIcons::forViewControl( ViewControl::SaveComputerPositions, p ); } },
			{ AdministrationCommand::LoadSettings, []( const QPalette& p ) { return TeacherActionIcons::forViewControl( ViewControl::LoadComputerPositions, p ); } },
			{ AdministrationCommand::About, []( const QPalette& p ) { return TeacherActionIcons::forViewControl( ViewControl::About, p ); } },
			{ AdministrationCommand::ResetConfiguration, []( const QPalette& p ) { return TeacherActionIcons::forSecondaryControl( SecondaryControl::DeleteScreenshot, p ); } },
		};
		QCOMPARE( reuses.size(), commandRows().size() );
		for( const auto& reuse : reuses )
		{
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto command = NextSpaceActionIcons::forAdministrationCommand( reuse.command, palette );
				const auto reviewed = reuse.reviewed( palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( int( reuse.command ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto image = renderAt( command, LogicalSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, renderAt( reviewed, LogicalSize, dpr, mode, state ) ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( command.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(),
													 reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage() ) == 0,
									  qPrintable( where ) );
						}
					}
				}
			}
		}
	}

	// the four V3bc sheet columns are appended after the 71 reviewed columns, so every reviewed cell keeps its place;
	// each new column equals exactly the reviewed columns that draw the same glyph and differs from every other column
	// (including the other new ones) in every palette band, row and scale, with the 64 floor of
	// fileTransferColumnsAppended()
	void administrationCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[70].feature, QStringLiteral("LdapDirectory") );
		QCOMPARE( entries[70].parentKind, QStringLiteral("admin") );
		QCOMPARE( entries[33].feature, QStringLiteral("LoadComputerPositions") );
		QCOMPARE( entries[34].feature, QStringLiteral("SaveComputerPositions") );
		QCOMPARE( entries[35].feature, QStringLiteral("About") );
		QCOMPARE( entries[35].parentKind, QStringLiteral("view") );
		QCOMPARE( entries[37].feature, QStringLiteral("DeleteScreenshot") );
		QCOMPARE( entries[47].feature, QStringLiteral("CollectFilesOpenDestination") );
		// 0-based reviewed columns drawing the same glyph in every row
		const QList<QList<int>> sameGlyph{ { 34 }, { 33, 47 }, { 35 }, { 37 } };
		QCOMPARE( sameGlyph.size(), commandRows().size() );
		for( int i = 0; i < commandRows().size(); ++i )
		{
			QCOMPARE( entries[71 + i].feature, QString::fromLatin1( commandRows()[i].role ) );
			QCOMPARE( entries[71 + i].parentKind, QStringLiteral("command") );
			QCOMPARE( entries[71 + i].state, QIcon::Off );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < commandRows().size(); ++i )
					{
						const auto cell = cellAt( 71 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 71 + i )
							{
								continue;
							}
							// V3bd: the Access Control columns are compared with every earlier column by
							// accessControlCommandColumnsAppended()
							if( other >= 75 )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 72 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
	}

	void accessControlCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < accessControlCommandRows().size(); ++i )
		{
			QTest::newRow( accessControlCommandRows()[i].id ) << i;
		}
	}

	// V3bd: at the page's logical 20 px each Access Control command draws exactly its generated glyph in the supplied
	// palette's colours and identically in Off and On, in every palette, mode and scale, through both the paint and the
	// pixmap paths
	void accessControlCommands()
	{
		QFETCH( int, index );
		const auto& row = accessControlCommandRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationAccessControlCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( icon, LogicalSize, dpr, mode, QIcon::Off );
					QVERIFY2( maxDifference( off, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
							  qPrintable( where ) );
					QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, QIcon::On ), off ) == 0, qPrintable( where ) );
					const auto offPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off );
					const auto onPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::On );
					QCOMPARE( offPixmap.devicePixelRatio(), dpr );
					QCOMPARE( offPixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
					QVERIFY2( maxDifference( offPixmap.toImage(), onPixmap.toImage() ) == 0, qPrintable( where ) );
					QVERIFY2( maxDifference( offPixmap.toImage(), off ) <= 2, qPrintable( where ) );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 20 logical px; and the same role follows each synthetic palette
	void accessControlCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : accessControlCommandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationAccessControlCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationAccessControlCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px every command stays an open outline inside its box: nothing is drawn outside the requested
	// rectangle in any mode or state, the probed clear areas of each source (24-unit view-box points, pre-checked
	// numerically to stay clear of the 2.1-unit stroke at every scale: build/v3bd-logs/probe-clearance.py) stay untouched
	// and coverage stays below half of the box, as in the reviewed 20-px open-outline tests
	void accessControlCommandsAreOpenOutline()
	{
		// one list per accessControlCommandRows() entry, in the same order
		const QList<QList<QPointF>> clear{
			{ QPointF( 12, 7 ), QPointF( 8, 16 ), QPointF( 16, 15 ) },        // circle-check: the face of fileTransferStatesAreOpenOutline()
			{ QPointF( 9, 8.5 ), QPointF( 9, 15.5 ), QPointF( 18, 7 ) },       // arrow-right: beside the shaft and beyond the upper head
			{ QPointF( 15, 8.5 ), QPointF( 15, 15.5 ), QPointF( 6, 7 ) },      // arrow-left: beside the shaft and beyond the upper head
			{ QPointF( 8, 8 ), QPointF( 16, 8 ), QPointF( 8, 16 ), QPointF( 16, 16 ) }, // plus: the four quadrants
			{ QPointF( 7.5, 10 ), QPointF( 16.5, 10 ), QPointF( 12, 9 ) },     // trash: inside the bin beside and above the bars
			{ QPointF( 6, 8 ), QPointF( 17, 17 ), QPointF( 9, 15 ) },         // pencil: both sides of the shaft and inside it
			{ QPointF( 8.5, 15 ), QPointF( 15.5, 15 ), QPointF( 7, 6 ) },      // arrow-up: beside the shaft and beyond the left head
			{ QPointF( 8.5, 9 ), QPointF( 15.5, 9 ), QPointF( 7, 18 ) },       // arrow-down: beside the shaft and beyond the left head
		};
		QCOMPARE( clear.size(), accessControlCommandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < accessControlCommandRows().size(); ++index )
		{
			const auto& row = accessControlCommandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationAccessControlCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 dpr %2").arg( QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s ink %.1f %% of the 20 px box", dpr, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// the eight roles draw eight different glyphs: every pair is shape-distinct without colour in every palette, mode and
	// scale at 20 logical px, so a swapped or collapsed mapping fails. Each pair's weakest coverage difference over all of
	// them is logged before it is checked against the 0.10 floor of administrationPagesAreDistinct() (identical glyphs
	// give 0)
	void accessControlCommandsAreDistinct()
	{
		const auto& rows = accessControlCommandRows();
		QSet<QString> glyphs;
		QSet<QString> roles;
		for( const auto& row : rows )
		{
			glyphs.insert( QLatin1String( row.glyph ) );
			roles.insert( QLatin1String( row.role ) );
		}
		QCOMPARE( glyphs.size(), 8 );
		QCOMPARE( roles.size(), 8 );
		constexpr qreal CoverageFloor = 0.10;
		QMap<QString, qreal> weakest;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( int a = 0; a < rows.size(); ++a )
					{
						const auto first = renderAt( NextSpaceActionIcons::forAdministrationAccessControlCommand( rows[a].command, palette ), LogicalSize, dpr, mode, QIcon::Off );
						for( int b = a + 1; b < rows.size(); ++b )
						{
							const auto where = QStringLiteral("%1 vs %2 %3 dpr %4 mode %5").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ),
																			 paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
							const auto second = renderAt( NextSpaceActionIcons::forAdministrationAccessControlCommand( rows[b].command, palette ), LogicalSize, dpr, mode, QIcon::Off );
							QVERIFY2( maxDifference( first, second ) > 128, qPrintable( where ) );
							const auto pair = QStringLiteral("%1 vs %2").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ) );
							const auto ratio = alphaDifferenceRatio( first, second );
							weakest[pair] = weakest.contains( pair ) ? qMin( weakest[pair], ratio ) : ratio;
						}
					}
				}
			}
		}
		QCOMPARE( weakest.size(), 28 );
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			qInfo( "20 px: %s weakest coverage difference %.4f", qPrintable( it.key() ), it.value() );
		}
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			QVERIFY2( it.value() > CoverageFloor, qPrintable( it.key() ) );
		}
	}

	// the three reused roles draw the reviewed generated data itself: Test renders pixel-identically to the Send/Collect
	// Finished state, Remove rule to the Screenshots-panel Delete button and Edit rule to the custom application/website
	// entry, in every palette, mode, scale and state, through both the paint and the pixmap paths. Only this test names
	// those reviewed roles; the Configurator uses AdministrationAccessControlCommand alone
	void accessControlCommandsReuseReviewedGlyphs()
	{
		struct Reuse { AdministrationAccessControlCommand command; std::function<QIcon( const QPalette& )> reviewed; };
		const QList<Reuse> reuses{
			{ AdministrationAccessControlCommand::Test, []( const QPalette& p ) { return TeacherActionIcons::forFileTransferState( FileTransferState::Finished, p ); } },
			{ AdministrationAccessControlCommand::RemoveRule, []( const QPalette& p ) { return TeacherActionIcons::forSecondaryControl( SecondaryControl::DeleteScreenshot, p ); } },
			{ AdministrationAccessControlCommand::EditRule, []( const QPalette& p ) {
				return TeacherActionIcons::forFeature( makeFeature( QStringLiteral("StartApp"), QUuid::createUuid(), QStringLiteral(":/core/document-edit.png") ), p ); } },
		};
		QCOMPARE( reuses.size(), 3 );
		for( const auto& reuse : reuses )
		{
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto command = NextSpaceActionIcons::forAdministrationAccessControlCommand( reuse.command, palette );
				const auto reviewed = reuse.reviewed( palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( int( reuse.command ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto image = renderAt( command, LogicalSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, renderAt( reviewed, LogicalSize, dpr, mode, state ) ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( command.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(),
													 reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage() ) == 0,
									  qPrintable( where ) );
						}
					}
				}
			}
		}
	}

	// the eight V3bd sheet columns are appended after the 75 reviewed columns, so every reviewed cell keeps its place;
	// each new column equals exactly the reviewed columns that draw the same glyph and differs from every other column
	// (including the other new ones) in every palette band, row and scale, with the 64 floor of
	// fileTransferColumnsAppended()
	void accessControlCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[74].feature, QStringLiteral("ResetConfiguration") );
		QCOMPARE( entries[74].parentKind, QStringLiteral("command") );
		QCOMPARE( entries[23].feature, QStringLiteral("StartApp") );
		QCOMPARE( entries[23].parentKind, QStringLiteral("custom") );
		QCOMPARE( entries[37].feature, QStringLiteral("DeleteScreenshot") );
		QCOMPARE( entries[59].feature, QStringLiteral("Finished") );
		QCOMPARE( entries[59].parentKind, QStringLiteral("transfer") );
		// 0-based reviewed columns drawing the same glyph in every row; empty for the five new glyphs
		const QList<QList<int>> sameGlyph{ { 59 }, {}, {}, {}, { 37, 74 }, { 23 }, {}, {} };
		QCOMPARE( sameGlyph.size(), accessControlCommandRows().size() );
		for( int i = 0; i < accessControlCommandRows().size(); ++i )
		{
			QCOMPARE( entries[75 + i].feature, QString::fromLatin1( accessControlCommandRows()[i].role ) );
			QCOMPARE( entries[75 + i].parentKind, QStringLiteral("access") );
			QCOMPARE( entries[75 + i].state, QIcon::Off );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < accessControlCommandRows().size(); ++i )
					{
						const auto cell = cellAt( 75 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 75 + i )
							{
								continue;
							}
							// V3be: the General page columns are compared with every earlier column by
							// generalCommandColumnsAppended()
							if( other >= 83 )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 76 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
	}

	void generalCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < generalCommandRows().size(); ++i )
		{
			QTest::newRow( generalCommandRows()[i].id ) << i;
		}
	}

	// V3be: at the page's logical 20 px each General page command draws exactly its generated glyph in the supplied
	// palette's colours and identically in Off and On, in every palette, mode and scale, through both the paint and the
	// pixmap paths
	void generalCommands()
	{
		QFETCH( int, index );
		const auto& row = generalCommandRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationGeneralCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( icon, LogicalSize, dpr, mode, QIcon::Off );
					QVERIFY2( maxDifference( off, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
							  qPrintable( where ) );
					QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, QIcon::On ), off ) == 0, qPrintable( where ) );
					const auto offPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off );
					const auto onPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::On );
					QCOMPARE( offPixmap.devicePixelRatio(), dpr );
					QCOMPARE( offPixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
					QVERIFY2( maxDifference( offPixmap.toImage(), onPixmap.toImage() ) == 0, qPrintable( where ) );
					QVERIFY2( maxDifference( offPixmap.toImage(), off ) <= 2, qPrintable( where ) );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 20 logical px; and the same role follows each synthetic palette
	void generalCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : generalCommandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationGeneralCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationGeneralCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px every command stays an open outline inside its box: nothing is drawn outside the requested
	// rectangle in any mode or state, the probed clear areas of each source (24-unit view-box points, pre-checked
	// numerically to stay clear of the 2.1-unit stroke at every scale: build/v3be-logs/probe-clearance.py) stay untouched
	// and coverage stays below half of the box, as in the reviewed 20-px open-outline tests
	void generalCommandsAreOpenOutline()
	{
		// one list per generalCommandRows() entry, in the same order
		const QList<QList<QPointF>> clear{
			{ QPointF( 12, 7 ), QPointF( 8, 16 ), QPointF( 16, 15 ) },        // circle-check: the face of fileTransferStatesAreOpenOutline()
			{ QPointF( 13, 15.25 ), QPointF( 6.25, 8.25 ), QPointF( 16, 9.5 ) }, // folder-open: front flap, back panel and right of the tab
			{ QPointF( 7.5, 10 ), QPointF( 16.5, 10 ), QPointF( 12, 9 ) },     // trash: inside the bin beside and above the bars
		};
		QCOMPARE( clear.size(), generalCommandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < generalCommandRows().size(); ++index )
		{
			const auto& row = generalCommandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationGeneralCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 dpr %2").arg( QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s ink %.1f %% of the 20 px box", dpr, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// the three roles draw three different glyphs: every pair is shape-distinct without colour in every palette, mode and
	// scale at 20 logical px, so a swapped or collapsed mapping fails. Each pair's weakest coverage difference over all of
	// them is logged before it is checked against the 0.10 floor of administrationPagesAreDistinct() (identical glyphs
	// give 0)
	void generalCommandsAreDistinct()
	{
		const auto& rows = generalCommandRows();
		QSet<QString> glyphs;
		QSet<QString> roles;
		for( const auto& row : rows )
		{
			glyphs.insert( QLatin1String( row.glyph ) );
			roles.insert( QLatin1String( row.role ) );
		}
		QCOMPARE( glyphs.size(), 3 );
		QCOMPARE( roles.size(), 3 );
		constexpr qreal CoverageFloor = 0.10;
		QMap<QString, qreal> weakest;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( int a = 0; a < rows.size(); ++a )
					{
						const auto first = renderAt( NextSpaceActionIcons::forAdministrationGeneralCommand( rows[a].command, palette ), LogicalSize, dpr, mode, QIcon::Off );
						for( int b = a + 1; b < rows.size(); ++b )
						{
							const auto where = QStringLiteral("%1 vs %2 %3 dpr %4 mode %5").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ),
																			 paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
							const auto second = renderAt( NextSpaceActionIcons::forAdministrationGeneralCommand( rows[b].command, palette ), LogicalSize, dpr, mode, QIcon::Off );
							QVERIFY2( maxDifference( first, second ) > 128, qPrintable( where ) );
							const auto pair = QStringLiteral("%1 vs %2").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ) );
							const auto ratio = alphaDifferenceRatio( first, second );
							weakest[pair] = weakest.contains( pair ) ? qMin( weakest[pair], ratio ) : ratio;
						}
					}
				}
			}
		}
		QCOMPARE( weakest.size(), 3 );
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			qInfo( "20 px: %s weakest coverage difference %.4f", qPrintable( it.key() ), it.value() );
		}
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			QVERIFY2( it.value() > CoverageFloor, qPrintable( it.key() ) );
		}
	}

	// all three roles draw the reviewed generated data itself: Test authentication renders pixel-identically to the
	// Send/Collect Finished state, Open log directory to the Load positions action and Clear log files to the
	// Screenshots-panel Delete button, in every palette, mode, scale and state, through both the paint and the pixmap
	// paths. Only this test names those reviewed roles; the Configurator uses AdministrationGeneralCommand alone
	void generalCommandsReuseReviewedGlyphs()
	{
		struct Reuse { AdministrationGeneralCommand command; std::function<QIcon( const QPalette& )> reviewed; };
		const QList<Reuse> reuses{
			{ AdministrationGeneralCommand::TestAuthentication, []( const QPalette& p ) { return TeacherActionIcons::forFileTransferState( FileTransferState::Finished, p ); } },
			{ AdministrationGeneralCommand::OpenLogDirectory, []( const QPalette& p ) { return TeacherActionIcons::forViewControl( ViewControl::LoadComputerPositions, p ); } },
			{ AdministrationGeneralCommand::ClearLogFiles, []( const QPalette& p ) { return TeacherActionIcons::forSecondaryControl( SecondaryControl::DeleteScreenshot, p ); } },
		};
		QCOMPARE( reuses.size(), 3 );
		for( const auto& reuse : reuses )
		{
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto command = NextSpaceActionIcons::forAdministrationGeneralCommand( reuse.command, palette );
				const auto reviewed = reuse.reviewed( palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( int( reuse.command ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto image = renderAt( command, LogicalSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, renderAt( reviewed, LogicalSize, dpr, mode, state ) ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( command.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(),
													 reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage() ) == 0,
									  qPrintable( where ) );
						}
					}
				}
			}
		}
	}

	// the three V3be sheet columns are appended after the 83 reviewed columns, so every reviewed cell keeps its place;
	// each new column equals exactly the reviewed columns that draw the same glyph and differs from every other column
	// (including the other new ones) in every palette band, row and scale, with the 64 floor of
	// fileTransferColumnsAppended()
	void generalCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[82].feature, QStringLiteral("MoveRuleDown") );
		QCOMPARE( entries[82].parentKind, QStringLiteral("access") );
		QCOMPARE( entries[59].feature, QStringLiteral("Finished") );
		QCOMPARE( entries[59].parentKind, QStringLiteral("transfer") );
		QCOMPARE( entries[75].feature, QStringLiteral("Test") );
		QCOMPARE( entries[75].parentKind, QStringLiteral("access") );
		QCOMPARE( entries[33].feature, QStringLiteral("LoadComputerPositions") );
		QCOMPARE( entries[47].feature, QStringLiteral("CollectFilesOpenDestination") );
		QCOMPARE( entries[72].feature, QStringLiteral("LoadSettings") );
		QCOMPARE( entries[37].feature, QStringLiteral("DeleteScreenshot") );
		QCOMPARE( entries[74].feature, QStringLiteral("ResetConfiguration") );
		QCOMPARE( entries[79].feature, QStringLiteral("RemoveRule") );
		// 0-based reviewed columns drawing the same glyph in every row
		const QList<QList<int>> sameGlyph{ { 59, 75 }, { 33, 47, 72 }, { 37, 74, 79 } };
		QCOMPARE( sameGlyph.size(), generalCommandRows().size() );
		for( int i = 0; i < generalCommandRows().size(); ++i )
		{
			QCOMPARE( entries[83 + i].feature, QString::fromLatin1( generalCommandRows()[i].role ) );
			QCOMPARE( entries[83 + i].parentKind, QStringLiteral("general") );
			QCOMPARE( entries[83 + i].state, QIcon::Off );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < generalCommandRows().size(); ++i )
					{
						const auto cell = cellAt( 83 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 83 + i )
							{
								continue;
							}
							// V3bf: the Teacher Console page columns are compared with every earlier column by
							// teacherConsoleCommandColumnsAppended()
							if( other >= 86 )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 84 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
	}

	void teacherConsoleCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < teacherConsoleCommandRows().size(); ++i )
		{
			QTest::newRow( teacherConsoleCommandRows()[i].id ) << i;
		}
	}

	// V3bf: at the page's logical 20 px each Teacher Console page command draws exactly its generated glyph in the
	// supplied palette's colours and identically in Off and On, in every palette, mode and scale, through both the paint
	// and the pixmap paths
	void teacherConsoleCommands()
	{
		QFETCH( int, index );
		const auto& row = teacherConsoleCommandRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( icon, LogicalSize, dpr, mode, QIcon::Off );
					QVERIFY2( maxDifference( off, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
							  qPrintable( where ) );
					QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, QIcon::On ), off ) == 0, qPrintable( where ) );
					const auto offPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off );
					const auto onPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::On );
					QCOMPARE( offPixmap.devicePixelRatio(), dpr );
					QCOMPARE( offPixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
					QVERIFY2( maxDifference( offPixmap.toImage(), onPixmap.toImage() ) == 0, qPrintable( where ) );
					QVERIFY2( maxDifference( offPixmap.toImage(), off ) <= 2, qPrintable( where ) );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 20 logical px; and the same role follows each synthetic palette
	void teacherConsoleCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : teacherConsoleCommandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px every command stays an open outline inside its box: nothing is drawn outside the requested
	// rectangle in any mode or state, the probed clear areas of each source (24-unit view-box points, pre-checked
	// numerically to stay clear of the 2.1-unit stroke at every scale: build/v3bf-logs/probe-clearance.py) stay untouched
	// and coverage stays below half of the box, as in the reviewed 20-px open-outline tests
	void teacherConsoleCommandsAreOpenOutline()
	{
		// one list per teacherConsoleCommandRows() entry, in the same order
		const QList<QList<QPointF>> clear{
			{ QPointF( 13, 15.25 ), QPointF( 6.25, 8.25 ), QPointF( 16, 9.5 ) }, // folder-open: front flap, back panel and right of the tab
			{ QPointF( 13, 15.25 ), QPointF( 6.25, 8.25 ), QPointF( 16, 9.5 ) }, // folder-open
			{ QPointF( 13, 15.25 ), QPointF( 6.25, 8.25 ), QPointF( 16, 9.5 ) }, // folder-open
			{ QPointF( 9, 8.5 ), QPointF( 9, 15.5 ), QPointF( 18, 7 ) },       // arrow-right: beside the shaft and beyond the upper head
			{ QPointF( 15, 8.5 ), QPointF( 15, 15.5 ), QPointF( 6, 7 ) },      // arrow-left: beside the shaft and beyond the upper head
		};
		QCOMPARE( clear.size(), teacherConsoleCommandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < teacherConsoleCommandRows().size(); ++index )
		{
			const auto& row = teacherConsoleCommandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 %2 dpr %3").arg( QLatin1String( row.role ), QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s (%s) ink %.1f %% of the 20 px box", dpr, row.role, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// the five roles draw three different glyphs: the three directory roles draw the identical folder-open, and every pair
	// of different glyphs (folder-open, arrow-right, arrow-left) is shape-distinct without colour in every palette, mode
	// and scale at 20 logical px, so a swapped, collapsed or mis-mapped role fails. Each glyph pair's weakest coverage
	// difference over all of them is logged before it is checked against the 0.10 floor of
	// administrationPagesAreDistinct() (identical glyphs give 0)
	void teacherConsoleCommandsAreDistinct()
	{
		const auto& rows = teacherConsoleCommandRows();
		QSet<QString> glyphs;
		QSet<QString> roles;
		for( const auto& row : rows )
		{
			glyphs.insert( QLatin1String( row.glyph ) );
			roles.insert( QLatin1String( row.role ) );
		}
		QCOMPARE( glyphs.size(), 3 );
		QCOMPARE( roles.size(), 5 );
		constexpr qreal CoverageFloor = 0.10;
		QMap<QString, qreal> weakest;
		int samePairs = 0;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( int a = 0; a < rows.size(); ++a )
					{
						const auto first = renderAt( NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( rows[a].command, palette ), LogicalSize, dpr, mode, QIcon::Off );
						for( int b = a + 1; b < rows.size(); ++b )
						{
							const auto where = QStringLiteral("%1 vs %2 %3 dpr %4 mode %5").arg( QLatin1String( rows[a].role ), QLatin1String( rows[b].role ),
																	 paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
							const auto second = renderAt( NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( rows[b].command, palette ), LogicalSize, dpr, mode, QIcon::Off );
							if( qstrcmp( rows[a].glyph, rows[b].glyph ) == 0 )
							{
								QVERIFY2( maxDifference( first, second ) == 0, qPrintable( where ) );
								++samePairs;
								continue;
							}
							QVERIFY2( maxDifference( first, second ) > 128, qPrintable( where ) );
							const auto pair = QStringLiteral("%1 vs %2").arg( QLatin1String( rows[a].glyph ), QLatin1String( rows[b].glyph ) );
							const auto ratio = alphaDifferenceRatio( first, second );
							weakest[pair] = weakest.contains( pair ) ? qMin( weakest[pair], ratio ) : ratio;
						}
					}
				}
			}
		}
		// three same-glyph directory pairs in each of 3 palettes x 4 scales x 4 modes
		QCOMPARE( samePairs, int( 3 * 3 * Scales.size() * Modes.size() ) );
		QCOMPARE( weakest.size(), 3 );
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			qInfo( "20 px: %s weakest coverage difference %.4f", qPrintable( it.key() ), it.value() );
		}
		for( auto it = weakest.cbegin(); it != weakest.cend(); ++it )
		{
			QVERIFY2( it.value() > CoverageFloor, qPrintable( it.key() ) );
		}
	}

	// the page's spatial meaning is kept: Disable selected feature moves features from the All features list on the left
	// to the Disabled features list on the right, so its arrow head is on the right; Enable selected feature moves them
	// back, so its head is on the left. At every scale the head arms are inked on their own side (view-box points on the
	// arms) and clear on the other (3 units from the shaft, beyond the 1.05 stroke half-width)
	void teacherConsoleFeatureCommandsKeepDirection()
	{
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		const QList<QPointF> rightHead{ QPointF( 16, 9 ), QPointF( 16, 15 ) };
		const QList<QPointF> leftHead{ QPointF( 8, 9 ), QPointF( 8, 15 ) };
		const QList<QPair<AdministrationTeacherConsoleCommand, bool>> commands{
			{ AdministrationTeacherConsoleCommand::DisableFeature, true },
			{ AdministrationTeacherConsoleCommand::EnableFeature, false },
		};
		for( const auto& [command, pointsRight] : commands )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( command, palette );
			for( const auto dpr : Scales )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					const auto where = QStringLiteral("%1 dpr %2 state %3").arg( int( command ) ).arg( dpr ).arg( int( state ) );
					const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, state ).convertToFormat( QImage::Format_ARGB32 );
					for( const auto& point : pointsRight ? rightHead : leftHead )
					{
						QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) > 128, qPrintable( where + QStringLiteral(" head") ) );
					}
					for( const auto& point : pointsRight ? leftHead : rightHead )
					{
						QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0, qPrintable( where + QStringLiteral(" opposite side") ) );
					}
				}
			}
		}
	}

	// all five roles draw the reviewed generated data itself: the three directory commands render pixel-identically to
	// the Load positions action, Disable selected feature to the Access Control Authorize selected group button and
	// Enable selected feature to its Remove selected authorized group button, in every palette, mode, scale and state,
	// through both the paint and the pixmap paths. Only this test names those reviewed roles; the Configurator uses
	// AdministrationTeacherConsoleCommand alone
	void teacherConsoleCommandsReuseReviewedGlyphs()
	{
		struct Reuse { AdministrationTeacherConsoleCommand command; std::function<QIcon( const QPalette& )> reviewed; };
		const QList<Reuse> reuses{
			{ AdministrationTeacherConsoleCommand::OpenUserConfigurationDirectory, []( const QPalette& p ) { return TeacherActionIcons::forViewControl( ViewControl::LoadComputerPositions, p ); } },
			{ AdministrationTeacherConsoleCommand::OpenScreenshotDirectory, []( const QPalette& p ) { return TeacherActionIcons::forViewControl( ViewControl::LoadComputerPositions, p ); } },
			{ AdministrationTeacherConsoleCommand::OpenConfigurationTemplatesDirectory, []( const QPalette& p ) { return TeacherActionIcons::forViewControl( ViewControl::LoadComputerPositions, p ); } },
			{ AdministrationTeacherConsoleCommand::DisableFeature, []( const QPalette& p ) { return TeacherActionIcons::forAdministrationAccessControlCommand( AdministrationAccessControlCommand::AddAuthorizedGroup, p ); } },
			{ AdministrationTeacherConsoleCommand::EnableFeature, []( const QPalette& p ) { return TeacherActionIcons::forAdministrationAccessControlCommand( AdministrationAccessControlCommand::RemoveAuthorizedGroup, p ); } },
		};
		QCOMPARE( reuses.size(), 5 );
		for( const auto& reuse : reuses )
		{
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto command = NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( reuse.command, palette );
				const auto reviewed = reuse.reviewed( palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( int( reuse.command ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto image = renderAt( command, LogicalSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, renderAt( reviewed, LogicalSize, dpr, mode, state ) ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( command.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(),
													 reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage() ) == 0,
									  qPrintable( where ) );
						}
					}
				}
			}
		}
	}

	// the five V3bf sheet columns are appended after the 86 reviewed columns, so every reviewed cell keeps its place;
	// each new column equals exactly the reviewed and new columns that draw the same glyph and differs from every other
	// column in every palette band, row and scale, with the 64 floor of fileTransferColumnsAppended()
	void teacherConsoleCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[85].feature, QStringLiteral("ClearLogFiles") );
		QCOMPARE( entries[85].parentKind, QStringLiteral("general") );
		QCOMPARE( entries[84].feature, QStringLiteral("OpenLogDirectory") );
		QCOMPARE( entries[84].parentKind, QStringLiteral("general") );
		QCOMPARE( entries[33].feature, QStringLiteral("LoadComputerPositions") );
		QCOMPARE( entries[47].feature, QStringLiteral("CollectFilesOpenDestination") );
		QCOMPARE( entries[72].feature, QStringLiteral("LoadSettings") );
		QCOMPARE( entries[76].feature, QStringLiteral("AddAuthorizedGroup") );
		QCOMPARE( entries[76].parentKind, QStringLiteral("access") );
		QCOMPARE( entries[77].feature, QStringLiteral("RemoveAuthorizedGroup") );
		QCOMPARE( entries[77].parentKind, QStringLiteral("access") );
		// 0-based reviewed and new columns drawing the same glyph in every row
		const QList<QList<int>> sameGlyph{ { 33, 47, 72, 84, 87, 88 }, { 33, 47, 72, 84, 86, 88 }, { 33, 47, 72, 84, 86, 87 }, { 76 }, { 77 } };
		QCOMPARE( sameGlyph.size(), teacherConsoleCommandRows().size() );
		for( int i = 0; i < teacherConsoleCommandRows().size(); ++i )
		{
			QCOMPARE( entries[86 + i].feature, QString::fromLatin1( teacherConsoleCommandRows()[i].role ) );
			QCOMPARE( entries[86 + i].parentKind, QStringLiteral("console") );
			QCOMPARE( entries[86 + i].state, QIcon::Off );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < teacherConsoleCommandRows().size(); ++i )
					{
						const auto cell = cellAt( 86 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 86 + i )
							{
								continue;
							}
							// V3bh: the Student Agent page columns are compared with every earlier column by
							// studentAgentCommandColumnsAppended()
							if( other >= 91 )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 87 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
	}

	void studentAgentCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < studentAgentCommandRows().size(); ++i )
		{
			QTest::newRow( studentAgentCommandRows()[i].id ) << i;
		}
	}

	// V3bh: at the page's logical 20 px each Student Agent page service command draws exactly its generated glyph (Start
	// the reviewed player-play, Stop the pinned player-stop) in the supplied palette's colours and identically in Off and
	// On, in every palette, mode and scale, through both the paint and the pixmap paths
	void studentAgentCommands()
	{
		QFETCH( int, index );
		const auto& row = studentAgentCommandRows()[index];
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationStudentAgentCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( icon, LogicalSize, dpr, mode, QIcon::Off );
					QVERIFY2( maxDifference( off, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
							  qPrintable( where ) );
					QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, QIcon::On ), off ) == 0, qPrintable( where ) );
					const auto offPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off );
					const auto onPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::On );
					QCOMPARE( offPixmap.devicePixelRatio(), dpr );
					QCOMPARE( offPixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
					QVERIFY2( maxDifference( offPixmap.toImage(), onPixmap.toImage() ) == 0, qPrintable( where ) );
					QVERIFY2( maxDifference( offPixmap.toImage(), off ) <= 2, qPrintable( where ) );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText, which the page shows on whichever service button it
	// disables for the current service state; Selected: Active HighlightedText; in both states, at every scale, at 20
	// logical px; and the same role follows each synthetic palette
	void studentAgentCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : studentAgentCommandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationStudentAgentCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationStudentAgentCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px both commands stay open outlines inside their box: nothing is drawn outside the requested
	// rectangle in any mode or state, the probed clear areas of each source (24-unit view-box points, pre-checked
	// numerically to stay clear of the 2.1-unit stroke at every scale: build/v3bh-logs/probe-clearance.py) stay untouched,
	// so neither the play triangle nor the stop square is filled, and coverage stays below half of the box, as in the
	// reviewed 20-px open-outline tests
	void studentAgentCommandsAreOpenOutline()
	{
		// one list per studentAgentCommandRows() entry, in the same order
		const QList<QList<QPointF>> clear{
			{ QPointF( 11, 12 ), QPointF( 4, 12 ), QPointF( 17, 6 ) },                            // player-play: centroid, left of the base, above the upper edge
			{ QPointF( 12, 12 ), QPointF( 8.5, 8.5 ), QPointF( 15.5, 15.5 ), QPointF( 2.5, 12 ) }, // player-stop: centre, two inner corners, left of the square
		};
		QCOMPARE( clear.size(), studentAgentCommandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < studentAgentCommandRows().size(); ++index )
		{
			const auto& row = studentAgentCommandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationStudentAgentCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 %2 dpr %3").arg( QLatin1String( row.role ), QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s (%s) ink %.1f %% of the 20 px box", dpr, row.role, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// Start and Stop draw two different glyphs that stay shape-distinct without colour in every palette, mode and scale
	// (100/125/150/200 %) at 20 logical px, so a swapped or collapsed role fails: they differ by more than 128 somewhere,
	// the stop square's top edge (x 16, y 5 units) is inked where the play triangle is clear (pre-checked by
	// build/v3bh-logs/probe-clearance.py), and the weakest coverage difference is logged before it is checked against
	// the 0.10 floor of administrationPagesAreDistinct()
	void studentAgentCommandsAreDistinct()
	{
		const auto& rows = studentAgentCommandRows();
		QCOMPARE( rows.size(), 2 );
		QVERIFY( qstrcmp( rows[0].glyph, rows[1].glyph ) != 0 );
		constexpr qreal CoverageFloor = 0.10;
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		const QPointF stopTopEdge( 16, 5 );
		qreal weakest = 1.0;
		int compared = 0;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto start = renderAt( NextSpaceActionIcons::forAdministrationStudentAgentCommand( AdministrationStudentAgentCommand::StartService, palette ),
												 LogicalSize, dpr, mode, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
					const auto stop = renderAt( NextSpaceActionIcons::forAdministrationStudentAgentCommand( AdministrationStudentAgentCommand::StopService, palette ),
												LogicalSize, dpr, mode, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
					QVERIFY2( maxDifference( start, stop ) > 128, qPrintable( where ) );
					QVERIFY2( maxAlphaInUnits( stop, dpr, at( stopTopEdge ) ) > 128, qPrintable( where + QStringLiteral(" stop top edge") ) );
					QVERIFY2( maxAlphaInUnits( start, dpr, at( stopTopEdge ) ) == 0, qPrintable( where + QStringLiteral(" play clear") ) );
					weakest = qMin( weakest, alphaDifferenceRatio( start, stop ) );
					++compared;
				}
			}
		}
		QCOMPARE( compared, int( 3 * Scales.size() * Modes.size() ) );
		qInfo( "20 px: player-play vs player-stop weakest coverage difference %.4f", weakest );
		QVERIFY( weakest > CoverageFloor );
	}

	// Start service draws the reviewed generated player-play itself: it renders pixel-identically to the Slideshow
	// start/pause button's Off state in every palette, mode, scale and state, through both the paint and the pixmap paths.
	// Only this test names that reviewed role; the Configurator uses AdministrationStudentAgentCommand alone
	void studentAgentStartReusesSlideshowPlay()
	{
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto start = NextSpaceActionIcons::forAdministrationStudentAgentCommand( AdministrationStudentAgentCommand::StartService, palette );
			const auto reviewed = TeacherActionIcons::forSecondaryControl( SecondaryControl::SlideshowStartPause, palette );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto play = renderAt( reviewed, LogicalSize, dpr, mode, QIcon::Off );
					const auto playPixmap = reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off ).toImage();
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto where = QStringLiteral("%1 dpr %2 mode %3 state %4").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) ).arg( int( state ) );
						const auto image = renderAt( start, LogicalSize, dpr, mode, state );
						QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
						QVERIFY2( maxDifference( image, play ) == 0, qPrintable( where ) );
						QVERIFY2( maxDifference( start.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), playPixmap ) == 0,
								  qPrintable( where ) );
					}
				}
			}
		}
	}

	// the two V3bh sheet columns are appended after the 91 reviewed columns, so every reviewed cell keeps its place; in
	// the Normal, Disabled and Selected rows Start service equals exactly the reviewed Slideshow start (Off) column, and
	// otherwise each new column differs from every other column (including the other new one) in every palette band, row
	// and scale, with the 64 floor of fileTransferColumnsAppended() (the Checked row paints every column's On state,
	// where the slideshow column shows pause)
	void studentAgentCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[90].feature, QStringLiteral("EnableFeature") );
		QCOMPARE( entries[90].parentKind, QStringLiteral("console") );
		QCOMPARE( entries[40].feature, QStringLiteral("SlideshowStartPause") );
		QCOMPARE( entries[40].parentKind, QStringLiteral("secondary") );
		QCOMPARE( entries[40].state, QIcon::Off );
		QCOMPARE( entries[41].feature, QStringLiteral("SlideshowStartPause") );
		QCOMPARE( entries[41].state, QIcon::On );
		// 0-based reviewed columns drawing the same glyph in the Normal, Disabled and Selected rows
		const QList<QList<int>> sameGlyph{ { 40 }, {} };
		QCOMPARE( sameGlyph.size(), studentAgentCommandRows().size() );
		for( int i = 0; i < studentAgentCommandRows().size(); ++i )
		{
			QCOMPARE( entries[91 + i].feature, QString::fromLatin1( studentAgentCommandRows()[i].role ) );
			QCOMPARE( entries[91 + i].parentKind, QStringLiteral("agent") );
			QCOMPARE( entries[91 + i].state, QIcon::Off );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < studentAgentCommandRows().size(); ++i )
					{
						const auto cell = cellAt( 91 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 91 + i )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 92 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( row < 3 && sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
	}

	void toastIcons_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < toastRows().size(); ++i )
		{
			QTest::newRow( toastRows()[i].id ) << i;
		}
	}

	// V3bi: each of the five existing Toast resources is the deterministic alpha mask of its pinned Tabler glyph: the
	// resource Toast loads for its Icon value at its unchanged size; white RGB everywhere with the shape only in alpha
	// (so Toast's RGB recolouring leaves no halo); the shape of the QPainter reference of the generated glyph (2.1-unit
	// stroke, round caps and joins) and closer to it than to any other of the five glyphs; inked stroke points and clear
	// interior points (24-unit view-box points pre-checked by build/v3bi-logs/probe-points.py); and an open outline that
	// Toast's own logical size (18 px state icons, 10 px close) draws in white inside its box at 100/125/150/200 %
	// (distinction at those sizes: toastIconsAreDistinct())
	void toastIcons()
	{
		QFETCH( int, index );
		const auto& row = toastRows()[index];
		constexpr qreal MaskReferenceCeiling = 0.06;
		const QImage mask( QLatin1String( row.resource ) );
		QVERIFY2( mask.isNull() == false, row.resource );
		QCOMPARE( mask.size(), QSize( row.pixels, row.pixels ) );
		QVERIFY( mask.hasAlphaChannel() );
		const auto straight = mask.convertToFormat( QImage::Format_ARGB32 );
		int coloured = 0;
		qint64 alpha = 0;
		for( int y = 0; y < straight.height(); ++y )
		{
			for( int x = 0; x < straight.width(); ++x )
			{
				const auto pixel = straight.pixel( x, y );
				coloured += ( qRed( pixel ) != 255 || qGreen( pixel ) != 255 || qBlue( pixel ) != 255 ) ? 1 : 0;
				alpha += qAlpha( pixel );
			}
		}
		QCOMPARE( coloured, 0 );
		QVERIFY2( alpha > 0, row.id );
		QVERIFY2( alpha < qint64( 255 ) * row.pixels * row.pixels / 2, row.id );
		for( const auto& corner : { QPoint( 0, 0 ), QPoint( row.pixels - 1, 0 ), QPoint( 0, row.pixels - 1 ), QPoint( row.pixels - 1, row.pixels - 1 ) } )
		{
			QCOMPARE( qAlpha( straight.pixel( corner ) ), 0 );
		}
		// Toast loads exactly this resource for the row's Icon value
		Toast toast;
		if( row.icon == Toast::Icon::Close )
		{
			toast.setCloseButtonIcon( row.icon );
			QCOMPARE( maxDifference( toast.getCloseButtonIcon().toImage(), mask ), 0 );
		}
		else
		{
			toast.setIcon( row.icon );
			QCOMPARE( maxDifference( toast.getIcon().toImage(), mask ), 0 );
		}
		// the generated Tabler glyph: the mask matches its QPainter reference, and no other of the five glyphs as closely
		for( const auto& other : toastRows() )
		{
			QVERIFY2( hasGlyph( other.glyph ), other.glyph );
		}
		const auto own = alphaDifferenceRatio( straight, maskReference( row.glyph, row.pixels ) );
		qInfo( "%s: %s mask vs QPainter reference coverage difference %.4f", row.role, row.glyph, own );
		QVERIFY2( own < MaskReferenceCeiling, row.id );
		for( const auto& other : toastRows() )
		{
			if( qstrcmp( other.glyph, row.glyph ) == 0 )
			{
				continue;
			}
			const auto ratio = alphaDifferenceRatio( straight, maskReference( other.glyph, row.pixels ) );
			QVERIFY2( ratio > 0.10 && ratio > 2 * own, qPrintable( QStringLiteral("%1 vs %2: %3").arg( QLatin1String( row.id ), QLatin1String( other.glyph ) ).arg( ratio ) ) );
		}
		// probes at the mask's own resolution: one list per toastRows() entry, in the same order
		const QList<QList<QPointF>> inked{
			{ QPointF( 3, 12 ), QPointF( 9, 12 ), QPointF( 15, 10 ) },                                   // ring, check start and end
			{ QPointF( 12, 11 ), QPointF( 12, 16 ), QPointF( 4.5, 20 ) },                                // exclamation bar and dot, base
			{ QPointF( 3, 12 ), QPointF( 12, 12 ), QPointF( 10, 10 ) },                                  // ring, x crossing and arm end
			{ QPointF( 3, 12 ), QPointF( 12, 9 ), QPointF( 12, 14 ) },                                   // ring, i dot and stem
			{ QPointF( 12, 12 ), QPointF( 6, 6 ), QPointF( 18, 18 ) },                                   // x crossing and arm ends
		};
		const QList<QList<QPointF>> clear{
			{ QPointF( 12, 6 ), QPointF( 6.5, 12 ), QPointF( 16, 16 ) },                                 // inside the ring
			{ QPointF( 12, 18.2 ), QPointF( 8, 15 ), QPointF( 16, 15 ), QPointF( 4, 6 ), QPointF( 20, 6 ) }, // inside and outside the triangle
			{ QPointF( 12, 6.5 ), QPointF( 6.5, 12 ), QPointF( 17.5, 12 ), QPointF( 12, 17.5 ) },        // between the x arms
			{ QPointF( 8, 12 ), QPointF( 16, 12 ), QPointF( 12, 6 ) },                                   // beside and above the i
			{ QPointF( 12, 7 ), QPointF( 7, 12 ), QPointF( 17, 12 ), QPointF( 12, 17 ) },                // between the x arms
		};
		QCOMPARE( inked.size(), toastRows().size() );
		QCOMPARE( clear.size(), toastRows().size() );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( const auto& point : inked[index] )
		{
			QVERIFY2( maxAlphaAt( straight, row.pixels, 1.0, at( point ) ) >= 250, qPrintable( QStringLiteral("inked %1,%2").arg( point.x() ).arg( point.y() ) ) );
		}
		for( const auto& point : clear[index] )
		{
			QVERIFY2( maxAlphaAt( straight, row.pixels, 1.0, at( point ) ) == 0, qPrintable( QStringLiteral("clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
		}
		// Toast's own logical size at four scales
		const QIcon icon( QPixmap::fromImage( mask ) );
		for( const auto dpr : Scales )
		{
			const auto where = QStringLiteral("%1 %2 px dpr %3").arg( QLatin1String( row.role ) ).arg( row.logical ).arg( dpr );
			const auto image = renderAt( icon, row.logical, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
			const auto ink = measure( image, QRect( 0, 0, row.logical, row.logical ), QColor( Qt::white ) );
			QVERIFY2( ink.pixels > 0, qPrintable( where ) );
			QVERIFY2( ink.colourMismatches == 0, qPrintable( where ) );
			QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
			const auto padded = renderAt( icon, row.logical, dpr, QIcon::Normal, QIcon::Off, Pad );
			QVERIFY2( measure( padded, QRect( Pad, Pad, row.logical, row.logical ) ).outside == 0, qPrintable( QStringLiteral("overflowing ink: ") + where ) );
			qInfo( "%s: ink %.1f %% of the box", qPrintable( where ), 100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
		}
	}

	// V3bi: each Toast preset keeps its Icon value and accent colour. Toast::applyPreset() is byte-identical but cannot
	// run here: its dark-mode test calls VeyonCore::useDarkMode(), which reads VeyonCore::config() without a VeyonCore
	// instance, and the offscreen platform ignores a requested colour scheme (build/v3bi-logs/colour-scheme-probe). So
	// each preset's Icon value and accent go through the same public setters applyPreset() calls: the new mask is loaded
	// unchanged, the shown 18-px icon keeps exactly the scaled mask alpha and takes the accent everywhere (no halo), and
	// the duration bar takes the accent; the close button keeps its exact 10-px alpha in the light (#000000) and dark
	// (#C9C9C9) close colours; the constructor defaults (duration, sizes, colours, fades) are unchanged
	void toastPresetMappings()
	{
		struct PresetRow { Toast::Preset preset; Toast::Icon icon; const char* resource; const char* accent; };
		const QList<PresetRow> presets{
			{ Toast::Preset::Success, Toast::Icon::Success, ":/core/toast-success.png", "#3E9141" },
			{ Toast::Preset::Warning, Toast::Icon::Warning, ":/core/toast-warning.png", "#E8B849" },
			{ Toast::Preset::Error, Toast::Icon::Error, ":/core/toast-error.png", "#BA2626" },
			{ Toast::Preset::Information, Toast::Icon::Information, ":/core/toast-information.png", "#007FFF" },
		};
		for( const auto& preset : presets )
		{
			const auto where = QStringLiteral("preset %1").arg( int( preset.preset ) );
			const QColor accent( QString::fromLatin1( preset.accent ) );
			Toast toast;
			QCOMPARE( toast.getDuration(), 5000 );
			QVERIFY( toast.isShowDurationBar() );
			QCOMPARE( toast.getIconSize(), QSize( 18, 18 ) );
			QCOMPARE( toast.getCloseButtonIconSize(), QSize( 10, 10 ) );
			QCOMPARE( toast.getCloseButtonSize(), QSize( 24, 24 ) );
			QCOMPARE( toast.getIconColor(), QColor( QStringLiteral("#5C5C5C") ) );
			QCOMPARE( toast.getDurationBarColor(), QColor( QStringLiteral("#5C5C5C") ) );
			QCOMPARE( toast.getCloseButtonIconColor(), QColor( QStringLiteral("#000000") ) );
			QCOMPARE( toast.getFadeInDuration(), 250 );
			QCOMPARE( toast.getFadeOutDuration(), 250 );
			// the per-preset steps of Toast::applyPreset(): icon, icon colour, duration bar colour
			toast.setIcon( preset.icon );
			toast.setIconColor( accent );
			toast.setDurationBarColor( accent );
			QCOMPARE( toast.getIconColor(), accent );
			QCOMPARE( toast.getDurationBarColor(), accent );
			QCOMPARE( maxDifference( toast.getIcon().toImage(), QImage( QLatin1String( preset.resource ) ) ), 0 );
			auto* iconWidget = toast.findChild<QPushButton*>( QStringLiteral("toast-icon-widget") );
			QVERIFY( iconWidget != nullptr );
			QCOMPARE( iconWidget->iconSize(), QSize( 18, 18 ) );
			const auto shown = iconWidget->icon().pixmap( QSize( 18, 18 ) ).toImage();
			const auto scaled = QIcon( toast.getIcon() ).pixmap( QSize( 18, 18 ) ).toImage();
			const auto problem = recolourProblem( shown, scaled, accent );
			QVERIFY2( problem.isEmpty(), qPrintable( where + QStringLiteral(": ") + problem ) );
			auto* close = toast.findChild<QPushButton*>( QStringLiteral("toast-close-button") );
			QVERIFY( close != nullptr );
			const auto closeScaled = QIcon( toast.getCloseButtonIcon() ).pixmap( QSize( 10, 10 ) ).toImage();
			const auto light = recolourProblem( close->icon().pixmap( QSize( 10, 10 ) ).toImage(), closeScaled, QColor( QStringLiteral("#000000") ) );
			QVERIFY2( light.isEmpty(), qPrintable( where + QStringLiteral(" close (light): ") + light ) );
			toast.setCloseButtonIconColor( QColor( QStringLiteral("#C9C9C9") ) );
			const auto dark = recolourProblem( close->icon().pixmap( QSize( 10, 10 ) ).toImage(), closeScaled, QColor( QStringLiteral("#C9C9C9") ) );
			QVERIFY2( dark.isEmpty(), qPrintable( where + QStringLiteral(" close (dark): ") + dark ) );
		}
	}

	// the four state icons stay shape-distinct without colour: at Toast's 18 logical px in every scale each pair differs
	// by more than 128 somewhere and by more than the 0.10 coverage floor, and at full resolution each ordered pair has a
	// point inked in the first and clear in the second (build/v3bi-logs/probe-points.py); the close mask at its 10 logical
	// px differs from every state icon drawn at 10 px by the same floor in every scale; the weakest coverage difference is
	// logged before it is checked
	void toastIconsAreDistinct()
	{
		const auto& rows = toastRows();
		QCOMPARE( rows.size(), 5 );
		constexpr qreal CoverageFloor = 0.10;
		struct Pair { int inked; int clear; QPointF point; };
		const QList<Pair> pairs{
			{ 0, 1, QPointF( 9, 12 ) }, { 1, 0, QPointF( 4.5, 20 ) }, { 0, 2, QPointF( 9, 12 ) }, { 2, 0, QPointF( 10, 10 ) },
			{ 0, 3, QPointF( 9, 12 ) }, { 3, 0, QPointF( 12, 9 ) }, { 1, 2, QPointF( 4.5, 20 ) }, { 2, 1, QPointF( 10, 10 ) },
			{ 1, 3, QPointF( 4.5, 20 ) }, { 3, 1, QPointF( 3, 12 ) }, { 2, 3, QPointF( 10, 10 ) }, { 3, 2, QPointF( 12, 9 ) },
		};
		QCOMPARE( pairs.size(), 12 );
		QList<QImage> masks;
		for( const auto& row : rows )
		{
			masks.append( QImage( QLatin1String( row.resource ) ).convertToFormat( QImage::Format_ARGB32 ) );
		}
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( const auto& pair : pairs )
		{
			const auto where = QStringLiteral("%1 over %2 at %3,%4").arg( QLatin1String( rows[pair.inked].role ), QLatin1String( rows[pair.clear].role ) )
								   .arg( pair.point.x() ).arg( pair.point.y() );
			QVERIFY2( maxAlphaAt( masks[pair.inked], 196, 1.0, at( pair.point ) ) >= 250, qPrintable( where ) );
			QVERIFY2( maxAlphaAt( masks[pair.clear], 196, 1.0, at( pair.point ) ) == 0, qPrintable( where ) );
		}
		const auto draw = [&]( int index, int size, qreal dpr ) {
			return renderAt( QIcon( QPixmap::fromImage( masks[index] ) ), size, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
		};
		qreal weakest = 1.0;
		int compared = 0;
		for( const auto dpr : Scales )
		{
			for( int a = 0; a < 4; ++a )
			{
				for( int b = a + 1; b < 4; ++b )
				{
					const auto where = QStringLiteral("%1 vs %2 dpr %3").arg( QLatin1String( rows[a].role ), QLatin1String( rows[b].role ) ).arg( dpr );
					const auto first = draw( a, 18, dpr );
					const auto second = draw( b, 18, dpr );
					QVERIFY2( maxDifference( first, second ) > 128, qPrintable( where ) );
					weakest = qMin( weakest, alphaDifferenceRatio( first, second ) );
					++compared;
				}
				const auto close = draw( 4, 10, dpr );
				const auto state = draw( a, 10, dpr );
				QVERIFY2( maxDifference( close, state ) > 128, qPrintable( QStringLiteral("Close vs %1 dpr %2").arg( QLatin1String( rows[a].role ) ).arg( dpr ) ) );
				weakest = qMin( weakest, alphaDifferenceRatio( close, state ) );
				++compared;
			}
		}
		QCOMPARE( compared, int( Scales.size() * ( 6 + 4 ) ) );
		qInfo( "Toast icons: weakest coverage difference %.4f", weakest );
		QVERIFY( weakest > CoverageFloor );
	}

	// V3bi: the icon-only close button carries the exact tooltip and accessible name "Close notification" and is
	// otherwise unchanged: no text or description, its 10-px icon in the 24-px button; the state icon widget gains no
	// text, and the toast holds only these two buttons (the real close click, keyboard focus and UIA are Not Run)
	void toastCloseAccessibility()
	{
		Toast toast;
		auto* close = toast.findChild<QPushButton*>( QStringLiteral("toast-close-button") );
		QVERIFY( close != nullptr );
		QCOMPARE( close->toolTip(), QStringLiteral("Close notification") );
		QCOMPARE( close->accessibleName(), QStringLiteral("Close notification") );
		QVERIFY( close->text().isEmpty() );
		QVERIFY( close->accessibleDescription().isEmpty() );
		QCOMPARE( close->iconSize(), QSize( 10, 10 ) );
		QCOMPARE( toast.getCloseButtonSize(), QSize( 24, 24 ) );
		QVERIFY( toast.isShowCloseButton() );
		auto* iconWidget = toast.findChild<QPushButton*>( QStringLiteral("toast-icon-widget") );
		QVERIFY( iconWidget != nullptr );
		QVERIFY( iconWidget->toolTip().isEmpty() );
		QVERIFY( iconWidget->accessibleName().isEmpty() );
		QVERIFY( iconWidget->text().isEmpty() );
		QCOMPARE( toast.findChildren<QPushButton*>().size(), 2 );
	}

	// the five V3bi sheet columns are appended after the 93 reviewed columns, so every reviewed cell keeps its place; each
	// Toast column differs from every other column (including the other Toast columns) by the 64 floor of
	// fileTransferColumnsAppended() in every palette band, row and scale, except the reviewed vector columns of its own
	// Tabler glyph (circle-check: Finished and both Test commands; info-circle: About and Administration About; x: Remote
	// Access Exit), where at most the logged fraction of pixels may differ by more than 64 (anti-aliasing of the raster
	// against the vector outline)
	void toastColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[92].feature, QStringLiteral("StopService") );
		QCOMPARE( entries[92].parentKind, QStringLiteral("agent") );
		QCOMPARE( entries[59].feature, QStringLiteral("Finished") );
		QCOMPARE( entries[75].feature, QStringLiteral("Test") );
		QCOMPARE( entries[83].feature, QStringLiteral("TestAuthentication") );
		QCOMPARE( entries[35].feature, QStringLiteral("About") );
		QCOMPARE( entries[73].feature, QStringLiteral("About") );
		QCOMPARE( entries[55].feature, QStringLiteral("Exit") );
		constexpr qreal SameGlyphCeiling = 0.10;
		// 0-based reviewed columns drawing the same Tabler glyph
		const QList<QList<int>> sameGlyph{ { 59, 75, 83 }, {}, {}, { 35, 73 }, { 55 } };
		QCOMPARE( sameGlyph.size(), toastRows().size() );
		for( int i = 0; i < toastRows().size(); ++i )
		{
			QCOMPARE( entries[93 + i].feature, QString::fromLatin1( toastRows()[i].role ) );
			QCOMPARE( entries[93 + i].parentKind, QStringLiteral("toast") );
			QCOMPARE( entries[93 + i].state, QIcon::Off );
		}
		qreal worstSame = 0;
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < toastRows().size(); ++i )
					{
						const auto cell = cellAt( 93 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 93 + i )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 94 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								const auto fraction = differingFraction( cell, cellAt( other ), 64 );
								worstSame = qMax( worstSame, fraction );
								QVERIFY2( fraction < SameGlyphCeiling, qPrintable( where ) );
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
		qInfo( "Toast columns: largest fraction of pixels differing by more than 64 from a same-glyph vector column %.4f", worstSame );
	}

	void authenticationKeysCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < authenticationKeysCommandRows().size(); ++i )
		{
			QTest::newRow( authenticationKeysCommandRows()[i].id ) << i;
		}
	}

	// V3bj: at the page's logical 20 px each Authentication Keys page directory command maps to exactly its generated
	// glyph, the reviewed folder-open, drawn in the supplied palette's colours in every palette, mode and scale, through
	// both the paint and the pixmap paths
	void authenticationKeysCommands()
	{
		QFETCH( int, index );
		const auto& row = authenticationKeysCommandRows()[index];
		QCOMPARE( QByteArray( row.glyph ), QByteArray( "folder-open" ) );
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( icon, LogicalSize, dpr, mode, QIcon::Off );
					QVERIFY2( maxDifference( off, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
							  qPrintable( where ) );
					const auto offPixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off );
					QCOMPARE( offPixmap.devicePixelRatio(), dpr );
					QCOMPARE( offPixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
					QVERIFY2( maxDifference( offPixmap.toImage(), off ) <= 2, qPrintable( where ) );
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 20 logical px; and the same role follows each synthetic palette
	void authenticationKeysCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : authenticationKeysCommandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px both commands stay open outlines inside their box: nothing is drawn outside the requested
	// rectangle in any mode or state, the reviewed folder-open's probed clear areas (24-unit view-box points, pre-checked
	// numerically to stay clear of the 2.1-unit stroke at every scale: build/v3bf-logs/probe-clearance.py) stay
	// untouched and coverage stays below half of the box, as in the reviewed 20-px open-outline tests
	void authenticationKeysCommandsAreOpenOutline()
	{
		// one list per authenticationKeysCommandRows() entry, in the same order
		const QList<QList<QPointF>> clear{
			{ QPointF( 13, 15.25 ), QPointF( 6.25, 8.25 ), QPointF( 16, 9.5 ) }, // folder-open: front flap, back panel and right of the tab
			{ QPointF( 13, 15.25 ), QPointF( 6.25, 8.25 ), QPointF( 16, 9.5 ) }, // folder-open
		};
		QCOMPARE( clear.size(), authenticationKeysCommandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < authenticationKeysCommandRows().size(); ++index )
		{
			const auto& row = authenticationKeysCommandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 %2 dpr %3").arg( QLatin1String( row.role ), QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s (%s) ink %.1f %% of the 20 px box", dpr, row.role, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// both roles draw the reviewed generated folder-open itself: through the neutral NextSpaceActionIcons name and the
	// legacy TeacherActionIcons name alike they render pixel-identically to the reviewed Load positions action in every
	// palette, mode, scale and state, through both the paint and the pixmap paths. Only this test names that reviewed
	// role; the Authentication Keys page uses AdministrationAuthenticationKeysCommand alone
	void authenticationKeysCommandsReuseReviewedFolderOpen()
	{
		int compared = 0;
		for( const auto& row : authenticationKeysCommandRows() )
		{
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto neutral = NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( row.command, palette );
				const auto legacy = TeacherActionIcons::forAdministrationAuthenticationKeysCommand( row.command, palette );
				const auto reviewed = TeacherActionIcons::forViewControl( ViewControl::LoadComputerPositions, palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( QLatin1String( row.role ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto expected = renderAt( reviewed, LogicalSize, dpr, mode, state );
							const auto image = renderAt( neutral, LogicalSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, expected ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( renderAt( legacy, LogicalSize, dpr, mode, state ), expected ) == 0, qPrintable( where ) );
							const auto reviewedPixmap = reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage();
							QVERIFY2( maxDifference( neutral.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), reviewedPixmap ) == 0,
									  qPrintable( where ) );
							QVERIFY2( maxDifference( legacy.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), reviewedPixmap ) == 0,
									  qPrintable( where ) );
							++compared;
						}
					}
				}
			}
		}
		QCOMPARE( compared, int( 2 * 3 * Scales.size() * Modes.size() * 2 ) );
	}

	// neither directory button is checkable, and both roles draw one stable glyph: in every palette, mode and scale the
	// On state equals the Off state through both the paint and the pixmap paths, a repeated request returns the same
	// image, and the separate public and private roles draw the same outline
	void authenticationKeysCommandsStableAcrossStates()
	{
		const auto& rows = authenticationKeysCommandRows();
		QCOMPARE( rows.size(), 2 );
		QVERIFY( rows[0].command != rows[1].command );
		QVERIFY( qstrcmp( rows[0].role, rows[1].role ) != 0 );
		int compared = 0;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto publicIcon = NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( AdministrationAuthenticationKeysCommand::OpenPublicKeyDirectory, palette );
			const auto privateIcon = NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( AdministrationAuthenticationKeysCommand::OpenPrivateKeyDirectory, palette );
			const auto again = NextSpaceActionIcons::forAdministrationAuthenticationKeysCommand( AdministrationAuthenticationKeysCommand::OpenPublicKeyDirectory, palette );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( publicIcon, LogicalSize, dpr, mode, QIcon::Off );
					QVERIFY2( measure( off, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
					const auto offPixmap = publicIcon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off ).toImage();
					for( const auto& icon : { publicIcon, privateIcon, again } )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, state ), off ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), offPixmap ) == 0,
									  qPrintable( where ) );
							++compared;
						}
					}
				}
			}
		}
		QCOMPARE( compared, int( 3 * Scales.size() * Modes.size() * 3 * 2 ) );
	}

	// the two V3bj sheet columns are appended after the 98 reviewed columns, so every reviewed cell keeps its place; each
	// new column equals exactly the reviewed and new columns that draw the same folder-open (Load positions, the Collect
	// Files destination, Load settings, Open log directory, the three Teacher Console directory commands and the other
	// new column) and differs from every other column in every palette band, row and scale, with the 64 floor of
	// fileTransferColumnsAppended()
	void authenticationKeysCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[97].feature, QStringLiteral("Close") );
		QCOMPARE( entries[97].parentKind, QStringLiteral("toast") );
		QCOMPARE( entries[33].feature, QStringLiteral("LoadComputerPositions") );
		QCOMPARE( entries[47].feature, QStringLiteral("CollectFilesOpenDestination") );
		QCOMPARE( entries[72].feature, QStringLiteral("LoadSettings") );
		QCOMPARE( entries[84].feature, QStringLiteral("OpenLogDirectory") );
		QCOMPARE( entries[86].feature, QStringLiteral("OpenUserConfigurationDirectory") );
		QCOMPARE( entries[87].feature, QStringLiteral("OpenScreenshotDirectory") );
		QCOMPARE( entries[88].feature, QStringLiteral("OpenConfigurationTemplatesDirectory") );
		// 0-based reviewed and new columns drawing the same glyph in every row
		const QList<QList<int>> sameGlyph{ { 33, 47, 72, 84, 86, 87, 88, 99 }, { 33, 47, 72, 84, 86, 87, 88, 98 } };
		QCOMPARE( sameGlyph.size(), authenticationKeysCommandRows().size() );
		for( int i = 0; i < authenticationKeysCommandRows().size(); ++i )
		{
			QCOMPARE( entries[98 + i].feature, QString::fromLatin1( authenticationKeysCommandRows()[i].role ) );
			QCOMPARE( entries[98 + i].parentKind, QStringLiteral("authkeys") );
			QCOMPARE( entries[98 + i].state, QIcon::Off );
		}
		int equal = 0;
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < authenticationKeysCommandRows().size(); ++i )
					{
						const auto cell = cellAt( 98 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 98 + i )
							{
								continue;
							}
							// V3bm: the File transfer page directory columns are compared with every earlier column by
							// fileTransferDirectoryCommandColumnsAppended()
							if( other >= 112 )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 99 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
								++equal;
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
		QCOMPARE( equal, int( 2 * 8 * Scales.size() * 3 * 4 ) );
	}

	void builtinDirectoryCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < builtinDirectoryCommandRows().size(); ++i )
		{
			QTest::newRow( builtinDirectoryCommandRows()[i].id ) << i;
		}
	}

	// V3bk: at the page's logical 20 px each Locations & computers command maps to exactly its generated glyph, the
	// reviewed plus, trash, arrow-up or arrow-down, drawn in the supplied palette's colours in every palette, mode, scale
	// and state, through both the paint and the pixmap paths
	void builtinDirectoryCommands()
	{
		QFETCH( int, index );
		const auto& row = builtinDirectoryCommandRows()[index];
		const QList<QByteArray> reviewed{ "plus", "trash", "arrow-up", "arrow-down" };
		QVERIFY2( reviewed.contains( QByteArray( row.glyph ) ), row.glyph );
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto where = QStringLiteral("%1 dpr %2 mode %3 state %4").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) ).arg( int( state ) );
						const auto image = renderAt( icon, LogicalSize, dpr, mode, state );
						QVERIFY2( maxDifference( image, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
								  qPrintable( where ) );
						const auto pixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state );
						QCOMPARE( pixmap.devicePixelRatio(), dpr );
						QCOMPARE( pixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
						QVERIFY2( maxDifference( pixmap.toImage(), image ) <= 2, qPrintable( where ) );
					}
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 20 logical px; and the same role follows each synthetic palette
	void builtinDirectoryCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : builtinDirectoryCommandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px all eight commands stay open outlines inside their box: nothing is drawn outside the requested
	// rectangle in any mode or state, the probed clear areas of the reviewed plus, trash, arrow-up and arrow-down (the
	// 24-unit view-box points of accessControlCommandsAreOpenOutline()) stay untouched and coverage stays below half of
	// the box, as in the reviewed 20-px open-outline tests
	void builtinDirectoryCommandsAreOpenOutline()
	{
		const QList<QPointF> plus{ QPointF( 8, 8 ), QPointF( 16, 8 ), QPointF( 8, 16 ), QPointF( 16, 16 ) }; // the four quadrants
		const QList<QPointF> trash{ QPointF( 7.5, 10 ), QPointF( 16.5, 10 ), QPointF( 12, 9 ) }; // inside the bin beside and above the bars
		const QList<QPointF> up{ QPointF( 8.5, 15 ), QPointF( 15.5, 15 ), QPointF( 7, 6 ) }; // beside the shaft and beyond the left head
		const QList<QPointF> down{ QPointF( 8.5, 9 ), QPointF( 15.5, 9 ), QPointF( 7, 18 ) }; // beside the shaft and beyond the left head
		// one list per builtinDirectoryCommandRows() entry, in the same order
		const QList<QList<QPointF>> clear{ plus, trash, up, down, plus, trash, up, down };
		QCOMPARE( clear.size(), builtinDirectoryCommandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < builtinDirectoryCommandRows().size(); ++index )
		{
			const auto& row = builtinDirectoryCommandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 %2 dpr %3").arg( QLatin1String( row.role ), QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s (%s) ink %.1f %% of the 20 px box", dpr, row.role, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// every role draws the reviewed generated glyph itself: through the neutral NextSpaceActionIcons name and the legacy
	// TeacherActionIcons name alike each renders pixel-identically to the reviewed role that already draws it (the Access
	// Control Add rule plus, the Screenshots Delete trash and the Access Control Move rule up/down arrows) in every
	// palette, mode, scale and state, through both the paint and the pixmap paths. Only this test names those reviewed
	// roles; the Locations & computers page uses AdministrationBuiltinDirectoryCommand alone
	void builtinDirectoryCommandsReuseReviewedGlyphs()
	{
		using Reviewed = std::function<QIcon( const QPalette& )>;
		const Reviewed plus = []( const QPalette& p ) { return TeacherActionIcons::forAdministrationAccessControlCommand( AdministrationAccessControlCommand::AddRule, p ); };
		const Reviewed trash = []( const QPalette& p ) { return TeacherActionIcons::forSecondaryControl( SecondaryControl::DeleteScreenshot, p ); };
		const Reviewed up = []( const QPalette& p ) { return TeacherActionIcons::forAdministrationAccessControlCommand( AdministrationAccessControlCommand::MoveRuleUp, p ); };
		const Reviewed down = []( const QPalette& p ) { return TeacherActionIcons::forAdministrationAccessControlCommand( AdministrationAccessControlCommand::MoveRuleDown, p ); };
		// one reviewed role per builtinDirectoryCommandRows() entry, in the same order
		const QList<Reviewed> reviewedFor{ plus, trash, up, down, plus, trash, up, down };
		QCOMPARE( reviewedFor.size(), builtinDirectoryCommandRows().size() );
		int compared = 0;
		for( int index = 0; index < builtinDirectoryCommandRows().size(); ++index )
		{
			const auto& row = builtinDirectoryCommandRows()[index];
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto neutral = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( row.command, palette );
				const auto legacy = TeacherActionIcons::forAdministrationBuiltinDirectoryCommand( row.command, palette );
				const auto reviewed = reviewedFor[index]( palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( QLatin1String( row.role ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto expected = renderAt( reviewed, LogicalSize, dpr, mode, state );
							const auto image = renderAt( neutral, LogicalSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, expected ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( renderAt( legacy, LogicalSize, dpr, mode, state ), expected ) == 0, qPrintable( where ) );
							const auto reviewedPixmap = reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage();
							QVERIFY2( maxDifference( neutral.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), reviewedPixmap ) == 0,
									  qPrintable( where ) );
							QVERIFY2( maxDifference( legacy.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), reviewedPixmap ) == 0,
									  qPrintable( where ) );
							++compared;
						}
					}
				}
			}
		}
		QCOMPARE( compared, int( 8 * 3 * Scales.size() * Modes.size() * 2 ) );
	}

	// no Locations & computers button is checkable, and every role draws one stable glyph: in every palette, mode and
	// scale the On state equals the Off state through both the paint and the pixmap paths and a repeated request returns
	// the same image; the eight roles are distinct, the location and the computer role of one command draw the same
	// outline, and the four commands (add, remove, move up, move down) differ from each other
	void builtinDirectoryCommandsStableAcrossStates()
	{
		const auto& rows = builtinDirectoryCommandRows();
		QCOMPARE( rows.size(), 8 );
		for( int a = 0; a < rows.size(); ++a )
		{
			for( int b = a + 1; b < rows.size(); ++b )
			{
				QVERIFY( rows[a].command != rows[b].command );
				QVERIFY( qstrcmp( rows[a].role, rows[b].role ) != 0 );
			}
		}
		int compared = 0;
		int distinct = 0;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( int index = 0; index < 4; ++index )
			{
				QCOMPARE( QByteArray( rows[index].glyph ), QByteArray( rows[index + 4].glyph ) );
				const auto location = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( rows[index].command, palette );
				const auto computer = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( rows[index + 4].command, palette );
				const auto again = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( rows[index].command, palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						const auto where = QStringLiteral("%1 %2 dpr %3 mode %4").arg( QLatin1String( rows[index].role ), paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
						const auto off = renderAt( location, LogicalSize, dpr, mode, QIcon::Off );
						QVERIFY2( measure( off, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
						const auto offPixmap = location.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off ).toImage();
						for( const auto& icon : { location, computer, again } )
						{
							for( const auto state : { QIcon::Off, QIcon::On } )
							{
								QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, state ), off ) == 0, qPrintable( where ) );
								QVERIFY2( maxDifference( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), offPixmap ) == 0,
										  qPrintable( where ) );
								++compared;
							}
						}
						for( int other = 0; other < 4; ++other )
						{
							if( other != index )
							{
								const auto otherIcon = NextSpaceActionIcons::forAdministrationBuiltinDirectoryCommand( rows[other].command, palette );
								QVERIFY2( maxDifference( renderAt( otherIcon, LogicalSize, dpr, mode, QIcon::Off ), off ) > 128,
										  qPrintable( where + QStringLiteral(" vs ") + QLatin1String( rows[other].role ) ) );
								++distinct;
							}
						}
					}
				}
			}
		}
		QCOMPARE( compared, int( 3 * 4 * Scales.size() * Modes.size() * 3 * 2 ) );
		QCOMPARE( distinct, int( 3 * 4 * Scales.size() * Modes.size() * 3 ) );
	}

	// the eight V3bk sheet columns are appended after the 100 reviewed columns, so every reviewed cell keeps its place;
	// each new column equals exactly the reviewed and new columns that draw the same glyph (the Access Control Add rule
	// plus; the Screenshots Delete, Reset configuration, Remove rule and Clear log files trash; the Move rule up and down
	// arrows; and the new column of the other group's same command) and differs from every other column in every
	// palette band, row and scale, with the 64 floor of fileTransferColumnsAppended()
	void builtinDirectoryCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[99].feature, QStringLiteral("OpenPrivateKeyDirectory") );
		QCOMPARE( entries[99].parentKind, QStringLiteral("authkeys") );
		QCOMPARE( entries[37].feature, QStringLiteral("DeleteScreenshot") );
		QCOMPARE( entries[74].feature, QStringLiteral("ResetConfiguration") );
		QCOMPARE( entries[78].feature, QStringLiteral("AddRule") );
		QCOMPARE( entries[79].feature, QStringLiteral("RemoveRule") );
		QCOMPARE( entries[81].feature, QStringLiteral("MoveRuleUp") );
		QCOMPARE( entries[82].feature, QStringLiteral("MoveRuleDown") );
		QCOMPARE( entries[85].feature, QStringLiteral("ClearLogFiles") );
		// 0-based reviewed and new columns drawing the same glyph in every row
		const QList<QList<int>> sameGlyph{ { 78, 104 }, { 37, 74, 79, 85, 105 }, { 81, 106 }, { 82, 107 },
									   { 78, 100 }, { 37, 74, 79, 85, 101 }, { 81, 102 }, { 82, 103 } };
		QCOMPARE( sameGlyph.size(), builtinDirectoryCommandRows().size() );
		for( int i = 0; i < builtinDirectoryCommandRows().size(); ++i )
		{
			QCOMPARE( entries[100 + i].feature, QString::fromLatin1( builtinDirectoryCommandRows()[i].role ) );
			QCOMPARE( entries[100 + i].parentKind, QStringLiteral("builtindirectory") );
			QCOMPARE( entries[100 + i].state, QIcon::Off );
		}
		int equal = 0;
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < builtinDirectoryCommandRows().size(); ++i )
					{
						const auto cell = cellAt( 100 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 100 + i )
							{
								continue;
							}
							// V3bl: the Applications & websites page columns are compared with every earlier column by
							// desktopServicesCommandColumnsAppended()
							if( other >= 108 )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 101 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
								++equal;
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
		QCOMPARE( equal, int( 22 * Scales.size() * 3 * 4 ) );
	}

	void desktopServicesCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < desktopServicesCommandRows().size(); ++i )
		{
			QTest::newRow( desktopServicesCommandRows()[i].id ) << i;
		}
	}

	// V3bl: at the page's logical 20 px each Applications & websites command maps to exactly its generated glyph, the
	// reviewed plus or trash, drawn in the supplied palette's colours in every palette, mode, scale and state, through
	// both the paint and the pixmap paths
	void desktopServicesCommands()
	{
		QFETCH( int, index );
		const auto& row = desktopServicesCommandRows()[index];
		const QList<QByteArray> reviewed{ "plus", "trash" };
		QVERIFY2( reviewed.contains( QByteArray( row.glyph ) ), row.glyph );
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto where = QStringLiteral("%1 dpr %2 mode %3 state %4").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) ).arg( int( state ) );
						const auto image = renderAt( icon, LogicalSize, dpr, mode, state );
						QVERIFY2( maxDifference( image, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
								  qPrintable( where ) );
						const auto pixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state );
						QCOMPARE( pixmap.devicePixelRatio(), dpr );
						QCOMPARE( pixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
						QVERIFY2( maxDifference( pixmap.toImage(), image ) <= 2, qPrintable( where ) );
					}
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 20 logical px; and the same role follows each synthetic palette
	void desktopServicesCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : desktopServicesCommandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px all four commands stay open outlines inside their box: nothing is drawn outside the requested
	// rectangle in any mode or state, the probed clear areas of the reviewed plus and trash (the 24-unit view-box points
	// of accessControlCommandsAreOpenOutline()) stay untouched and coverage stays below half of the box, as in the
	// reviewed 20-px open-outline tests
	void desktopServicesCommandsAreOpenOutline()
	{
		const QList<QPointF> plus{ QPointF( 8, 8 ), QPointF( 16, 8 ), QPointF( 8, 16 ), QPointF( 16, 16 ) }; // the four quadrants
		const QList<QPointF> trash{ QPointF( 7.5, 10 ), QPointF( 16.5, 10 ), QPointF( 12, 9 ) }; // inside the bin beside and above the bars
		// one list per desktopServicesCommandRows() entry, in the same order
		const QList<QList<QPointF>> clear{ plus, trash, plus, trash };
		QCOMPARE( clear.size(), desktopServicesCommandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < desktopServicesCommandRows().size(); ++index )
		{
			const auto& row = desktopServicesCommandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 %2 dpr %3").arg( QLatin1String( row.role ), QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s (%s) ink %.1f %% of the 20 px box", dpr, row.role, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// every role draws the reviewed generated glyph itself: through the neutral NextSpaceActionIcons name and the legacy
	// TeacherActionIcons name alike each renders pixel-identically to the reviewed role that already draws it (the Access
	// Control Add rule plus and the Screenshots Delete trash) in every palette, mode, scale and state, through both the
	// paint and the pixmap paths. Only this test names those reviewed roles; the Applications & websites page uses
	// AdministrationDesktopServicesCommand alone
	void desktopServicesCommandsReuseReviewedGlyphs()
	{
		using Reviewed = std::function<QIcon( const QPalette& )>;
		const Reviewed plus = []( const QPalette& p ) { return TeacherActionIcons::forAdministrationAccessControlCommand( AdministrationAccessControlCommand::AddRule, p ); };
		const Reviewed trash = []( const QPalette& p ) { return TeacherActionIcons::forSecondaryControl( SecondaryControl::DeleteScreenshot, p ); };
		// one reviewed role per desktopServicesCommandRows() entry, in the same order
		const QList<Reviewed> reviewedFor{ plus, trash, plus, trash };
		QCOMPARE( reviewedFor.size(), desktopServicesCommandRows().size() );
		int compared = 0;
		for( int index = 0; index < desktopServicesCommandRows().size(); ++index )
		{
			const auto& row = desktopServicesCommandRows()[index];
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto neutral = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( row.command, palette );
				const auto legacy = TeacherActionIcons::forAdministrationDesktopServicesCommand( row.command, palette );
				const auto reviewed = reviewedFor[index]( palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( QLatin1String( row.role ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto expected = renderAt( reviewed, LogicalSize, dpr, mode, state );
							const auto image = renderAt( neutral, LogicalSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, expected ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( renderAt( legacy, LogicalSize, dpr, mode, state ), expected ) == 0, qPrintable( where ) );
							const auto reviewedPixmap = reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage();
							QVERIFY2( maxDifference( neutral.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), reviewedPixmap ) == 0,
									  qPrintable( where ) );
							QVERIFY2( maxDifference( legacy.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), reviewedPixmap ) == 0,
									  qPrintable( where ) );
							++compared;
						}
					}
				}
			}
		}
		QCOMPARE( compared, int( 4 * 3 * Scales.size() * Modes.size() * 2 ) );
	}

	// no Applications & websites button is checkable, and every role draws one stable glyph: in every palette, mode and
	// scale the On state equals the Off state through both the paint and the pixmap paths and a repeated request returns
	// the same image; the four roles are distinct, the application and the website role of one command draw the same
	// outline, and the two commands (add, remove) differ from each other
	void desktopServicesCommandsStableAcrossStates()
	{
		const auto& rows = desktopServicesCommandRows();
		QCOMPARE( rows.size(), 4 );
		for( int a = 0; a < rows.size(); ++a )
		{
			for( int b = a + 1; b < rows.size(); ++b )
			{
				QVERIFY( rows[a].command != rows[b].command );
				QVERIFY( qstrcmp( rows[a].role, rows[b].role ) != 0 );
			}
		}
		int compared = 0;
		int distinct = 0;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			for( int index = 0; index < 2; ++index )
			{
				QCOMPARE( QByteArray( rows[index].glyph ), QByteArray( rows[index + 2].glyph ) );
				const auto application = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( rows[index].command, palette );
				const auto website = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( rows[index + 2].command, palette );
				const auto again = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( rows[index].command, palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						const auto where = QStringLiteral("%1 %2 dpr %3 mode %4").arg( QLatin1String( rows[index].role ), paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
						const auto off = renderAt( application, LogicalSize, dpr, mode, QIcon::Off );
						QVERIFY2( measure( off, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
						const auto offPixmap = application.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off ).toImage();
						for( const auto& icon : { application, website, again } )
						{
							for( const auto state : { QIcon::Off, QIcon::On } )
							{
								QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, state ), off ) == 0, qPrintable( where ) );
								QVERIFY2( maxDifference( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), offPixmap ) == 0,
										  qPrintable( where ) );
								++compared;
							}
						}
						const auto otherIcon = NextSpaceActionIcons::forAdministrationDesktopServicesCommand( rows[1 - index].command, palette );
						QVERIFY2( maxDifference( renderAt( otherIcon, LogicalSize, dpr, mode, QIcon::Off ), off ) > 128,
								  qPrintable( where + QStringLiteral(" vs ") + QLatin1String( rows[1 - index].role ) ) );
						++distinct;
					}
				}
			}
		}
		QCOMPARE( compared, int( 3 * 2 * Scales.size() * Modes.size() * 3 * 2 ) );
		QCOMPARE( distinct, int( 3 * 2 * Scales.size() * Modes.size() ) );
	}

	// the four V3bl sheet columns are appended after the 108 reviewed columns, so every reviewed cell keeps its place;
	// each new column equals exactly the reviewed and new columns that draw the same glyph (the Access Control Add rule
	// and the Locations & computers Add location/computer plus; the Screenshots Delete, Reset configuration, Remove rule,
	// Clear log files and Locations & computers Remove location/computer trash; and the new column of the other group's
	// same command) and differs from every other column in every palette band, row and scale, with the 64 floor of
	// fileTransferColumnsAppended()
	void desktopServicesCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[107].feature, QStringLiteral("MoveComputerDown") );
		QCOMPARE( entries[107].parentKind, QStringLiteral("builtindirectory") );
		QCOMPARE( entries[37].feature, QStringLiteral("DeleteScreenshot") );
		QCOMPARE( entries[74].feature, QStringLiteral("ResetConfiguration") );
		QCOMPARE( entries[78].feature, QStringLiteral("AddRule") );
		QCOMPARE( entries[79].feature, QStringLiteral("RemoveRule") );
		QCOMPARE( entries[85].feature, QStringLiteral("ClearLogFiles") );
		QCOMPARE( entries[100].feature, QStringLiteral("AddLocation") );
		QCOMPARE( entries[101].feature, QStringLiteral("RemoveLocation") );
		QCOMPARE( entries[104].feature, QStringLiteral("AddComputer") );
		QCOMPARE( entries[105].feature, QStringLiteral("RemoveComputer") );
		// 0-based reviewed and new columns drawing the same glyph in every row
		const QList<QList<int>> sameGlyph{ { 78, 100, 104, 110 }, { 37, 74, 79, 85, 101, 105, 111 },
									   { 78, 100, 104, 108 }, { 37, 74, 79, 85, 101, 105, 109 } };
		QCOMPARE( sameGlyph.size(), desktopServicesCommandRows().size() );
		for( int i = 0; i < desktopServicesCommandRows().size(); ++i )
		{
			QCOMPARE( entries[108 + i].feature, QString::fromLatin1( desktopServicesCommandRows()[i].role ) );
			QCOMPARE( entries[108 + i].parentKind, QStringLiteral("desktopservices") );
			QCOMPARE( entries[108 + i].state, QIcon::Off );
		}
		int equal = 0;
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < desktopServicesCommandRows().size(); ++i )
					{
						const auto cell = cellAt( 108 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 108 + i )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 109 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( sameGlyph[i].contains( other ) )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
								++equal;
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
		QCOMPARE( equal, int( 22 * Scales.size() * 3 * 4 ) );
	}

	void fileTransferDirectoryCommands_data()
	{
		QTest::addColumn<int>("index");
		for( int i = 0; i < fileTransferCommandRows().size(); ++i )
		{
			QTest::newRow( fileTransferCommandRows()[i].id ) << i;
		}
	}

	// V3bm: at the page's logical 20 px each File transfer directory command maps to exactly its generated glyph, the
	// reviewed folder-open, drawn in the supplied palette's colours in every palette, mode, scale and state, through
	// both the paint and the pixmap paths
	void fileTransferDirectoryCommands()
	{
		QFETCH( int, index );
		const auto& row = fileTransferCommandRows()[index];
		QCOMPARE( QByteArray( row.glyph ), QByteArray( "folder-open" ) );
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			const auto icon = NextSpaceActionIcons::forAdministrationFileTransferCommand( row.command, palette );
			QVERIFY( icon.isNull() == false );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto where = QStringLiteral("%1 dpr %2 mode %3 state %4").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) ).arg( int( state ) );
						const auto image = renderAt( icon, LogicalSize, dpr, mode, state );
						QVERIFY2( maxDifference( image, referenceAt( QLatin1String( row.glyph ), expectedColour( palette, mode ), LogicalSize, dpr ) ) == 0,
								  qPrintable( where ) );
						const auto pixmap = icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state );
						QCOMPARE( pixmap.devicePixelRatio(), dpr );
						QCOMPARE( pixmap.size(), ( QSizeF( LogicalSize, LogicalSize ) * dpr ).toSize() );
						QVERIFY2( maxDifference( pixmap.toImage(), image ) <= 2, qPrintable( where ) );
					}
				}
			}
		}
	}

	// Normal/Active: WindowText; Disabled: Disabled WindowText; Selected: Active HighlightedText; in both states, at
	// every scale, at 20 logical px; and the same role follows each synthetic palette
	void fileTransferDirectoryCommandPaletteRoles()
	{
		const auto probe = probePalette();
		const QList<QPair<QIcon::Mode, QColor>> expected{
			{ QIcon::Normal, QColor( 192, 0, 0 ) },
			{ QIcon::Active, QColor( 192, 0, 0 ) },
			{ QIcon::Disabled, QColor( 0, 160, 0 ) },
			{ QIcon::Selected, QColor( 0, 0, 192 ) },
		};
		for( const auto& row : fileTransferCommandRows() )
		{
			const auto icon = NextSpaceActionIcons::forAdministrationFileTransferCommand( row.command, probe );
			for( const auto& [mode, colour] : expected )
			{
				for( const auto state : { QIcon::Off, QIcon::On } )
				{
					for( const auto dpr : Scales )
					{
						const auto ink = measure( renderAt( icon, LogicalSize, dpr, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ), colour );
						QVERIFY2( ink.colourSamples > 10, row.id );
						QVERIFY2( ink.colourMismatches == 0, row.id );
					}
				}
			}
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto themed = NextSpaceActionIcons::forAdministrationFileTransferCommand( row.command, palette );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto ink = measure( renderAt( themed, LogicalSize, 1.5, mode, state ), QRect( 0, 0, LogicalSize, LogicalSize ),
												  expectedColour( palette, mode ) );
						QVERIFY2( ink.colourSamples > 10 && ink.colourMismatches == 0, qPrintable( paletteName( kind ) + QLatin1Char(' ') + QLatin1String( row.id ) ) );
					}
				}
			}
		}
	}

	// at 20 logical px all four commands stay open outlines inside their box: nothing is drawn outside the requested
	// rectangle in any mode or state, the reviewed folder-open's probed clear areas (the 24-unit view-box points of
	// authenticationKeysCommandsAreOpenOutline(), pre-checked in build/v3bf-logs/probe-clearance.py) stay untouched and
	// coverage stays below half of the box, as in the reviewed 20-px open-outline tests
	void fileTransferDirectoryCommandsAreOpenOutline()
	{
		const QList<QPointF> folder{ QPointF( 13, 15.25 ), QPointF( 6.25, 8.25 ), QPointF( 16, 9.5 ) }; // front flap, back panel and right of the tab
		// one list per fileTransferCommandRows() entry, in the same order
		const QList<QList<QPointF>> clear{ folder, folder, folder, folder };
		QCOMPARE( clear.size(), fileTransferCommandRows().size() );
		const auto palette = syntheticPalette( PaletteKind::Light );
		const auto at = []( const QPointF& unit ) { return QRectF( unit.x() - 0.1, unit.y() - 0.1, 0.2, 0.2 ); };
		for( int index = 0; index < fileTransferCommandRows().size(); ++index )
		{
			const auto& row = fileTransferCommandRows()[index];
			const auto icon = NextSpaceActionIcons::forAdministrationFileTransferCommand( row.command, palette );
			for( const auto dpr : Scales )
			{
				const auto where = QStringLiteral("%1 %2 dpr %3").arg( QLatin1String( row.role ), QLatin1String( row.glyph ) ).arg( dpr );
				const auto image = renderAt( icon, LogicalSize, dpr, QIcon::Normal, QIcon::Off ).convertToFormat( QImage::Format_ARGB32 );
				for( const auto& point : clear[index] )
				{
					QVERIFY2( maxAlphaInUnits( image, dpr, at( point ) ) == 0,
							  qPrintable( where + QStringLiteral(" clear %1,%2").arg( point.x() ).arg( point.y() ) ) );
				}
				const auto ink = measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) );
				QVERIFY2( ink.pixels > 0, qPrintable( where ) );
				QVERIFY2( ink.alpha < qint64( 255 ) * image.width() * image.height() / 2, qPrintable( where ) );
				for( const auto mode : Modes )
				{
					for( const auto state : { QIcon::Off, QIcon::On } )
					{
						const auto padded = renderAt( icon, LogicalSize, dpr, mode, state, Pad );
						const auto paddedInk = measure( padded, QRect( Pad, Pad, LogicalSize, LogicalSize ), expectedColour( palette, mode ) );
						QVERIFY2( paddedInk.pixels > 0, qPrintable( where ) );
						QVERIFY2( paddedInk.outside == 0, qPrintable( QStringLiteral("clipped/overflowing ink: ") + where ) );
						QVERIFY2( paddedInk.colourMismatches == 0, qPrintable( where ) );
					}
				}
				qInfo( "dpr %.2f: %s (%s) ink %.1f %% of the 20 px box", dpr, row.role, row.glyph,
					   100.0 * ink.alpha / ( 255.0 * image.width() * image.height() ) );
			}
		}
	}

	// every role draws the reviewed generated folder-open itself: through the neutral NextSpaceActionIcons name and the
	// legacy TeacherActionIcons name alike each renders pixel-identically to the reviewed Load positions action in every
	// palette, mode, scale and state, through both the paint and the pixmap paths. Only this test names that reviewed
	// role; the File transfer page uses AdministrationFileTransferCommand alone
	void fileTransferDirectoryCommandsReuseReviewedFolderOpen()
	{
		int compared = 0;
		for( const auto& row : fileTransferCommandRows() )
		{
			for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
			{
				const auto palette = syntheticPalette( kind );
				const auto neutral = NextSpaceActionIcons::forAdministrationFileTransferCommand( row.command, palette );
				const auto legacy = TeacherActionIcons::forAdministrationFileTransferCommand( row.command, palette );
				const auto reviewed = TeacherActionIcons::forViewControl( ViewControl::LoadComputerPositions, palette );
				for( const auto dpr : Scales )
				{
					for( const auto mode : Modes )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							const auto where = QStringLiteral("%1 %2 dpr %3 mode %4 state %5").arg( QLatin1String( row.role ) ).arg( paletteName( kind ) )
												   .arg( dpr ).arg( int( mode ) ).arg( int( state ) );
							const auto expected = renderAt( reviewed, LogicalSize, dpr, mode, state );
							const auto image = renderAt( neutral, LogicalSize, dpr, mode, state );
							QVERIFY2( measure( image, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
							QVERIFY2( maxDifference( image, expected ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( renderAt( legacy, LogicalSize, dpr, mode, state ), expected ) == 0, qPrintable( where ) );
							const auto reviewedPixmap = reviewed.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage();
							QVERIFY2( maxDifference( neutral.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), reviewedPixmap ) == 0,
									  qPrintable( where ) );
							QVERIFY2( maxDifference( legacy.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), reviewedPixmap ) == 0,
									  qPrintable( where ) );
							++compared;
						}
					}
				}
			}
		}
		QCOMPARE( compared, int( 4 * 3 * Scales.size() * Modes.size() * 2 ) );
	}

	// no File transfer directory button is checkable, and every role draws one stable glyph: the four roles are
	// distinct, and in every palette, mode and scale the On state equals the Off state through both the paint and the
	// pixmap paths, a repeated request returns the same image, and the four separate roles draw the same outline
	void fileTransferDirectoryCommandsStableAcrossStates()
	{
		const auto& rows = fileTransferCommandRows();
		QCOMPARE( rows.size(), 4 );
		for( int a = 0; a < rows.size(); ++a )
		{
			QCOMPARE( QByteArray( rows[a].glyph ), QByteArray( "folder-open" ) );
			for( int b = a + 1; b < rows.size(); ++b )
			{
				QVERIFY( rows[a].command != rows[b].command );
				QVERIFY( qstrcmp( rows[a].role, rows[b].role ) != 0 );
			}
		}
		int compared = 0;
		for( const auto kind : { PaletteKind::Light, PaletteKind::Dark, PaletteKind::HighContrast } )
		{
			const auto palette = syntheticPalette( kind );
			QList<QIcon> icons;
			for( const auto& row : rows )
			{
				icons.append( NextSpaceActionIcons::forAdministrationFileTransferCommand( row.command, palette ) );
			}
			icons.append( NextSpaceActionIcons::forAdministrationFileTransferCommand( rows[0].command, palette ) );
			QCOMPARE( icons.size(), 5 );
			for( const auto dpr : Scales )
			{
				for( const auto mode : Modes )
				{
					const auto where = QStringLiteral("%1 dpr %2 mode %3").arg( paletteName( kind ) ).arg( dpr ).arg( int( mode ) );
					const auto off = renderAt( icons[0], LogicalSize, dpr, mode, QIcon::Off );
					QVERIFY2( measure( off, QRect( 0, 0, LogicalSize, LogicalSize ) ).pixels > 0, qPrintable( where ) );
					const auto offPixmap = icons[0].pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, QIcon::Off ).toImage();
					for( const auto& icon : icons )
					{
						for( const auto state : { QIcon::Off, QIcon::On } )
						{
							QVERIFY2( maxDifference( renderAt( icon, LogicalSize, dpr, mode, state ), off ) == 0, qPrintable( where ) );
							QVERIFY2( maxDifference( icon.pixmap( QSize( LogicalSize, LogicalSize ), dpr, mode, state ).toImage(), offPixmap ) == 0,
									  qPrintable( where ) );
							++compared;
						}
					}
				}
			}
		}
		QCOMPARE( compared, int( 3 * Scales.size() * Modes.size() * 5 * 2 ) );
	}

	// the four V3bm sheet columns are appended after the 112 reviewed columns, so every reviewed cell keeps its place;
	// each new column equals exactly the reviewed folder-open columns (Load positions, the Collect Files destination, Load
	// settings, Open log directory, the three Teacher Console and the two Authentication Keys directory commands) and the
	// three other new columns, and differs from every other column in every palette band, row and scale, with the 64
	// floor of fileTransferColumnsAppended()
	void fileTransferDirectoryCommandColumnsAppended()
	{
		const auto& entries = sheetEntries();
		QCOMPARE( entries.size(), 116 );
		QCOMPARE( entries[111].feature, QStringLiteral("RemoveWebsite") );
		QCOMPARE( entries[111].parentKind, QStringLiteral("desktopservices") );
		QCOMPARE( entries[33].feature, QStringLiteral("LoadComputerPositions") );
		QCOMPARE( entries[47].feature, QStringLiteral("CollectFilesOpenDestination") );
		QCOMPARE( entries[72].feature, QStringLiteral("LoadSettings") );
		QCOMPARE( entries[84].feature, QStringLiteral("OpenLogDirectory") );
		QCOMPARE( entries[86].feature, QStringLiteral("OpenUserConfigurationDirectory") );
		QCOMPARE( entries[87].feature, QStringLiteral("OpenScreenshotDirectory") );
		QCOMPARE( entries[88].feature, QStringLiteral("OpenConfigurationTemplatesDirectory") );
		QCOMPARE( entries[98].feature, QStringLiteral("OpenPublicKeyDirectory") );
		QCOMPARE( entries[99].feature, QStringLiteral("OpenPrivateKeyDirectory") );
		// 0-based reviewed columns drawing the same folder-open in every row; every new column also equals the other
		// three new columns
		const QList<int> reviewedFolderOpen{ 33, 47, 72, 84, 86, 87, 88, 98, 99 };
		for( int i = 0; i < fileTransferCommandRows().size(); ++i )
		{
			QCOMPARE( entries[112 + i].feature, QString::fromLatin1( fileTransferCommandRows()[i].role ) );
			QCOMPARE( entries[112 + i].parentKind, QStringLiteral("filetransfer") );
			QCOMPARE( entries[112 + i].state, QIcon::Off );
		}
		int equal = 0;
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			for( int band = 0; band < 3; ++band )
			{
				for( int row = 0; row < 4; ++row )
				{
					const auto cellAt = [&]( int column ) {
						return sheet.copy( qRound( ( 104 + column * 32 ) * dpr ), qRound( ( 40 + band * 154 + 18 + row * 32 ) * dpr ),
										   qRound( 32 * dpr ), qRound( 32 * dpr ) );
					};
					for( int i = 0; i < fileTransferCommandRows().size(); ++i )
					{
						const auto cell = cellAt( 112 + i );
						for( int other = 0; other < entries.size(); ++other )
						{
							if( other == 112 + i )
							{
								continue;
							}
							const auto where = QStringLiteral("column %1 vs %2 dpr %3 band %4 row %5").arg( 113 + i ).arg( other + 1 )
												   .arg( dpr ).arg( band ).arg( row );
							if( reviewedFolderOpen.contains( other ) || other >= 112 )
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) == 0, qPrintable( where ) );
								++equal;
							}
							else
							{
								QVERIFY2( maxDifference( cell, cellAt( other ) ) > 64, qPrintable( where ) );
							}
						}
					}
				}
			}
		}
		QCOMPARE( equal, int( 4 * 12 * Scales.size() * 3 * 4 ) );
	}

	void contactSheets()
	{
		const auto first = contactSheet( 1.25 );
		QCOMPARE( maxDifference( first, contactSheet( 1.25 ) ), 0 );
		const auto dir = qEnvironmentVariable( "V3AN_CONTACT_SHEET_DIR" );
		if( dir.isEmpty() )
		{
			QSKIP( "V3AN_CONTACT_SHEET_DIR not set; contact sheets not written" );
		}
		for( const auto dpr : Scales )
		{
			const auto sheet = contactSheet( dpr );
			const auto base = QStringLiteral("contact-sheet-%1x").arg( dpr, 0, 'f', 2 );
			QVERIFY( sheet.save( QDir( dir ).filePath( base + QStringLiteral(".png") ) ) );
			// nearest-neighbour enlargement for inspection only
			const auto zoom = sheet.scaled( sheet.size() * 3, Qt::IgnoreAspectRatio, Qt::FastTransformation );
			QVERIFY( zoom.save( QDir( dir ).filePath( base + QStringLiteral("-zoom3.png") ) ) );
		}
	}
};


QTEST_MAIN(TeacherActionIconsTest)
#include "TeacherActionIconsTest.moc"
