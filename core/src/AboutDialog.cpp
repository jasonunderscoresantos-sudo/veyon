/*
 * AboutDialog.cpp - implementation of AboutDialog
 *
 * Copyright (c) 2011-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

#include "AboutDialog.h"
#include "VeyonCore.h"

#include "ui_AboutDialog.h"

#include <veyonconfig.h>


AboutDialog::AboutDialog( QWidget *parent ) :
	QDialog( parent ),
	ui( new Ui::AboutDialog )
{
	ui->setupUi( this );

	ui->versionLabel->setText( VeyonCore::versionString() );

	// The branding resources belong to Master. Other components retain their UI.
	if( VeyonCore::component() == VeyonCore::Component::Master )
	{
		setWindowTitle( tr("About NextSpace Teacher Console") );
		setWindowIcon( QGuiApplication::windowIcon() );
		// NextSpace dark mode: runtime copies of the four About tab icons in the tab widget's text color. Each copy
		// keeps its source pixel size, device-pixel ratio and alpha; the source icons and light mode are unchanged.
		if( VeyonCore::useDarkMode() )
		{
			const auto tint = ui->tabWidget->palette().color( QPalette::WindowText );
			const std::pair<int, QString> tabIcons[] = {
				{ 0, QStringLiteral(":/core/help-about.png") },
				{ 1, QStringLiteral(":/core/user-group-new.png") },
				{ 2, QStringLiteral(":/core/languages.png") },
				{ 3, QStringLiteral(":/core/license.png") } };
			for( const auto& [index, resource] : tabIcons )
			{
				const QPixmap source( resource );
				QPixmap tinted( source.size() );
				tinted.setDevicePixelRatio( source.devicePixelRatio() );
				tinted.fill( Qt::transparent );
				QPainter painter( &tinted );
				painter.drawPixmap( 0, 0, source );
				painter.setCompositionMode( QPainter::CompositionMode_SourceIn );
				painter.fillRect( tinted.rect(), tint );
				painter.end();
				ui->tabWidget->setTabIcon( index, QIcon( tinted ) );
			}
		}
		auto logo = QPixmap( QStringLiteral(":/master/branding/nextspace-classroom-logo.png") )
			.scaled( QSize(160, 120) * devicePixelRatioF(), Qt::KeepAspectRatio, Qt::SmoothTransformation );
		logo.setDevicePixelRatio( devicePixelRatioF() );
		ui->label->setPixmap( logo );
		// NextSpace dark mode: the unchanged logo on a light Surface plate (Border token, 10 px radius, 8 px clear
		// space)
		if( VeyonCore::useDarkMode() )
		{
			ui->label->setStyleSheet( QStringLiteral(
				"QLabel { background-color: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 10px; padding: 8px; }" ) );
		}
		ui->label_2->setText( QStringLiteral("NextSpace Teacher Console") );
		ui->label_3->setText( QStringLiteral("NextSpace Classroom Management — EduLearn Technologies Inc") );
		ui->label_3->setWordWrap( true );
		ui->versionLabel->setText( QStringLiteral(NEXTSPACE_DISPLAY_VERSION) );
		ui->versionLabel->setWordWrap( true );
		ui->label_4->setText( QStringLiteral(
			"<a href=\"https://edulearntechnologies.com\">edulearntechnologies.com</a><br>"
			"<a href=\"mailto:support@edulearntechnologies.com\">support@edulearntechnologies.com</a>") );
		ui->label_4->setWordWrap( true );
		ui->label_9->setText( tr("Website / support:") );
		ui->label_5->setText( tr("For NextSpace translation support, contact support@edulearntechnologies.com. "
			"Upstream contributors and license information are retained in their respective tabs.") );
	}
	// The Configurator owns its own copy of the logo and uses its Administration application icon.
	else if( VeyonCore::component() == VeyonCore::Component::Configurator )
	{
		setWindowTitle( tr("About NextSpace Classroom Administration") );
		setWindowIcon( QGuiApplication::windowIcon() );
		auto logo = QPixmap( QStringLiteral(":/configurator/branding/nextspace-classroom-logo.png") )
			.scaled( QSize(160, 120) * devicePixelRatioF(), Qt::KeepAspectRatio, Qt::SmoothTransformation );
		logo.setDevicePixelRatio( devicePixelRatioF() );
		ui->label->setPixmap( logo );
		ui->label_2->setText( QStringLiteral("NextSpace Classroom Administration") );
		ui->label_3->setText( QStringLiteral("NextSpace Classroom Management — EduLearn Technologies Inc") );
		ui->label_3->setWordWrap( true );
		ui->versionLabel->setText( QStringLiteral(NEXTSPACE_DISPLAY_VERSION) );
		ui->versionLabel->setWordWrap( true );
		ui->label_4->setText( QStringLiteral(
			"<a href=\"https://edulearntechnologies.com\">edulearntechnologies.com</a><br>"
			"<a href=\"mailto:support@edulearntechnologies.com\">support@edulearntechnologies.com</a>") );
		ui->label_4->setWordWrap( true );
		ui->label_9->setText( tr("Website / support:") );
		ui->label_5->setText( tr("For NextSpace translation support, contact support@edulearntechnologies.com. "
			"Upstream contributors and license information are retained in their respective tabs.") );
	}

	QFile authors( QStringLiteral( ":/CONTRIBUTORS" ) );
	if (authors.open(QFile::ReadOnly))
	{
		ui->authors->setPlainText(QString::fromUtf8(authors.readAll()));
	}

	QFile license( QStringLiteral( ":/core/COPYING" ) );
	if (license.open(QFile::ReadOnly))
	{
		ui->license->setPlainText(QString::fromUtf8(license.readAll()));
	}

	// NextSpace: third-party notices follow the unchanged GPL text; without the resource the GPL text stays as is
	QFile thirdPartyNotices( QStringLiteral( ":/core/THIRD-PARTY-NOTICES.txt" ) );
	if (thirdPartyNotices.open(QFile::ReadOnly))
	{
		ui->license->appendPlainText(QStringLiteral("\n\nThird-party notices — Tabler Icons v3.49.0\n\n") +
									 QString::fromUtf8(thirdPartyNotices.readAll()));
	}
}



AboutDialog::~AboutDialog()
{
	delete ui;
}
