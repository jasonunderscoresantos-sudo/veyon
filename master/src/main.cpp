/*
 * main.cpp - startup routine for Veyon Master Application
 *
 * Copyright (c) 2004-2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
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

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QSplashScreen>
#include <QStyleHints>
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <QAccessibilityHints>
#endif

#include "DocumentationFigureCreator.h"
#include "VeyonMaster.h"
#include "MainWindow.h"


// NextSpace palette (Teacher Console process only): the docs/visual-rules.md light/dark tokens over the UI
// style and color scheme VeyonCore has already applied. The system palette is kept when the system asks for
// high contrast.
static void applyNextSpacePalette( QApplication& app )
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
	if( QGuiApplication::styleHints()->accessibility()->contrastPreference() == Qt::ContrastPreference::HighContrast )
	{
		return;
	}
#endif

	const auto dark = VeyonCore::useDarkMode();
	const auto token = [dark]( const char* light, const char* darkColor ) { return QColor( dark ? darkColor : light ); };
	const auto canvas = token( "#F4F7FA", "#0B1420" );
	const auto surface = token( "#FFFFFF", "#142235" );
	const auto subtleSurface = token( "#EAF0F5", "#1C2E43" );
	const auto primaryText = token( "#10243A", "#F1F5F9" );
	const auto secondaryText = token( "#526477", "#B2C0CF" );
	const auto border = token( "#7F91A5", "#5A7188" );
	const auto accent = token( "#087F8C", "#39C6C5" );
	const auto selection = token( "#E3F5F5", "#183D47" );
	const auto selectedText = token( "#10243A", "#F1F5F9" );
	const auto toolTipText = token( "#FFFFFF", "#10243A" );

	// Only the roles the visual rules define; every other role keeps the existing style's value
	auto palette = QApplication::palette();
	for( const auto group : { QPalette::Active, QPalette::Inactive } )
	{
		palette.setColor( group, QPalette::Window, canvas );
		palette.setColor( group, QPalette::WindowText, primaryText );
		palette.setColor( group, QPalette::Base, surface );
		palette.setColor( group, QPalette::AlternateBase, subtleSurface );
		palette.setColor( group, QPalette::Mid, border );
		palette.setColor( group, QPalette::Text, primaryText );
		palette.setColor( group, QPalette::Button, surface );
		palette.setColor( group, QPalette::ButtonText, primaryText );
		palette.setColor( group, QPalette::ToolTipBase, accent );
		palette.setColor( group, QPalette::ToolTipText, toolTipText );
		palette.setColor( group, QPalette::Highlight, selection );
		palette.setColor( group, QPalette::HighlightedText, selectedText );
		palette.setColor( group, QPalette::Link, accent );
		palette.setColor( group, QPalette::LinkVisited, accent );
		palette.setColor( group, QPalette::PlaceholderText, secondaryText );
	}
	// Disabled text uses the secondary-text token; enablement is unchanged
	for( const auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::PlaceholderText } )
	{
		palette.setColor( QPalette::Disabled, role, secondaryText );
	}
	QApplication::setPalette( palette );

	// Appended to the core style sheet so its checked-button blue and the native accents no longer win. Buttons
	// drawn by these rules keep distinct hover/pressed fills; only ordinary tree/list views (not the computer
	// monitoring views or their headers) get the Border token. No size, margin, padding, font or text.
	app.setStyleSheet( app.styleSheet() + QStringLiteral(
		"QToolButton:checked { background-color: %1; border: 1px solid %2; color: %3; }"
		"QLineEdit, QComboBox, QAbstractSpinBox { background-color: %5; border: 1px solid %4; }"
		"QTreeView, QListView#list, QListView#fileListView { border: 1px solid %4; }"
		"QPushButton:default { background-color: %5; border: 1px solid %2; }"
		"QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus, QPushButton:focus, QToolButton:focus "
			"{ border: 2px solid %2; }"
		"QPushButton:focus { background-color: %5; }"
		"QPushButton:default:hover, QPushButton:focus:hover { background-color: %6; }"
		"QPushButton:default:pressed, QPushButton:focus:pressed { background-color: %1; }"
		"QToolButton:checked:hover, QToolButton:focus:hover { background-color: %6; }"
		"QToolButton:checked:pressed, QToolButton:focus:pressed { background-color: %2; color: %7; }"
		"QSlider::handle:horizontal, QSlider::handle:vertical { background-color: %2; }"
		"QToolTip { background-color: %2; color: %7; }" )
		.arg( selection.name(), accent.name(), selectedText.name(), border.name(), surface.name(),
			subtleSurface.name(), toolTipText.name() ) );
}


int main( int argc, char** argv )
{
	VeyonCore::setupApplicationParameters();

	QApplication app( argc, argv );
	app.connect( &app, &QApplication::lastWindowClosed, &QApplication::quit );

	VeyonCore core( &app, VeyonCore::Component::Master, QStringLiteral("Master") );

#ifdef VEYON_DEBUG
	if( qEnvironmentVariableIsSet( "VEYON_MASTER_CREATE_DOC_FIGURES") )
	{
		DocumentationFigureCreator().run();
		return 0;
	}
#endif

	// NextSpace palette: after VeyonCore's UI initialization, before authentication and the main window
	applyNextSpacePalette( app );

	// Keep the application/settings identifier intact; only change visible identity.
	// No application display name: Qt would append it to every window title of this process.
	QIcon appIcon;
	for( const auto size : { 16, 20, 24, 32, 40, 48, 64, 128, 256 } )
	{
		appIcon.addFile( QStringLiteral(":/master/branding/nextspace-teacher-console-%1.png").arg(size), QSize(size, size) );
	}
	app.setWindowIcon( appIcon );

	const auto pixelRatio = app.devicePixelRatio();
	QPixmap splashPixmap( QSize(480, 320) * pixelRatio );
	splashPixmap.setDevicePixelRatio( pixelRatio );
	splashPixmap.fill( Qt::white );
	{
		QPainter painter( &splashPixmap );
		const auto logo = QPixmap( QStringLiteral(":/master/branding/nextspace-classroom-logo.png") )
			.scaled( QSize(400, 280) * pixelRatio, Qt::KeepAspectRatio, Qt::SmoothTransformation );
		const QSize logoSize( qRound(logo.width() / pixelRatio), qRound(logo.height() / pixelRatio) );
		painter.drawPixmap( QRect( QPoint((480 - logoSize.width()) / 2, (320 - logoSize.height()) / 2), logoSize ), logo );
	}
	QSplashScreen splashScreen( splashPixmap );
	splashScreen.show();

	if( MainWindow::initAuthentication() == false ||
			MainWindow::initAccessControl() == false )
	{
		return -1;
	}

	VeyonMaster masterCore( &core );

	// hide splash-screen as soon as main-window is shown
	splashScreen.finish( masterCore.mainWindow() );

	masterCore.mainWindow()->show();

	return core.exec();
}
