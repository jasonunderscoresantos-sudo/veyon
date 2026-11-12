/*
 * FileTransferConfigurationPage.cpp - implementation of FileTransferConfigurationPage
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
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

#include "FileSystemBrowser.h"
#include "FileTransferConfiguration.h"
#include "FileTransferConfigurationPage.h"
#include "NextSpaceActionIcons.h"
#include "Configuration/UiMapping.h"

#include "ui_FileTransferConfigurationPage.h"

FileTransferConfigurationPage::FileTransferConfigurationPage( FileTransferConfiguration& configuration, QWidget* parent ) :
	ConfigurationPage( parent ),
	ui( new Ui::FileTransferConfigurationPage ),
	m_configuration( configuration )
{
	ui->setupUi(this);

	// V3bm: palette-aware File transfer directory command icons at 20 logical px; each icon-only button gets its exact
	// translated tooltip, and its accessible name is that tooltip, so both cues stay identical; only then may palette
	// events refresh the icons (see event()). The browse connections and hidden widgets below are unchanged
	updateCommandIcons();
	ui->browseDefaultSourceDirectory->setToolTip( tr( "Browse for default source directory" ) );
	ui->browseDefaultSourceDirectory->setAccessibleName( ui->browseDefaultSourceDirectory->toolTip() );
	ui->browseDestinationDirectory->setToolTip( tr( "Browse for file transfer destination directory" ) );
	ui->browseDestinationDirectory->setAccessibleName( ui->browseDestinationDirectory->toolTip() );
	ui->browseFilesToCollectSourceDirectory->setToolTip( tr( "Browse for files to collect source directory" ) );
	ui->browseFilesToCollectSourceDirectory->setAccessibleName( ui->browseFilesToCollectSourceDirectory->toolTip() );
	ui->browseCollectedFilesDestinationDirectory->setToolTip( tr( "Browse for collected files destination directory" ) );
	ui->browseCollectedFilesDestinationDirectory->setAccessibleName( ui->browseCollectedFilesDestinationDirectory->toolTip() );
	m_commandIconsReady = true;

	connect(ui->browseDefaultSourceDirectory, &QAbstractButton::clicked,
			this, &FileTransferConfigurationPage::browseDefaultSourceDirectory);

	connect(ui->browseDestinationDirectory, &QAbstractButton::clicked,
			this, &FileTransferConfigurationPage::browseDestinationDirectory);

	connect(ui->browseFilesToCollectSourceDirectory, &QAbstractButton::clicked,
			this, &FileTransferConfigurationPage::browseFilesToCollectSourceDirectory);

	connect(ui->browseCollectedFilesDestinationDirectory, &QAbstractButton::clicked,
			this, &FileTransferConfigurationPage::browseCollectedFilesDestinationDirectory);

	connect(ui->collectedFilesGroupingMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		const auto groupingAttributesRequired = ui->collectedFilesGroupingMode->currentIndex() > 0;
		ui->collectedFilesGroupingAttribute1->setEnabled(groupingAttributesRequired);
		ui->collectedFilesGroupingAttribute2->setEnabled(groupingAttributesRequired);
	});

	ui->collectingMode->hide();
	ui->collectingModeLabel->hide();
}



FileTransferConfigurationPage::~FileTransferConfigurationPage()
{
	delete ui;
}



void FileTransferConfigurationPage::resetWidgets()
{
	FOREACH_FILE_TRANSFER_CONFIG_PROPERTY(INIT_WIDGET_FROM_PROPERTY);
}



void FileTransferConfigurationPage::connectWidgetsToProperties()
{
	FOREACH_FILE_TRANSFER_CONFIG_PROPERTY(CONNECT_WIDGET_TO_PROPERTY)
}



void FileTransferConfigurationPage::applyConfiguration()
{
}


void FileTransferConfigurationPage::browseDefaultSourceDirectory()
{
	FileSystemBrowser(FileSystemBrowser::ExistingDirectory, this).exec(ui->fileTransferDefaultSourceDirectory);
}



void FileTransferConfigurationPage::browseDestinationDirectory()
{
	FileSystemBrowser(FileSystemBrowser::ExistingDirectory, this).exec(ui->fileTransferDestinationDirectory);
}



void FileTransferConfigurationPage::browseFilesToCollectSourceDirectory()
{
	FileSystemBrowser(FileSystemBrowser::ExistingDirectory, this).exec(ui->filesToCollectSourceDirectory);
}



void FileTransferConfigurationPage::browseCollectedFilesDestinationDirectory()
{
	FileSystemBrowser(FileSystemBrowser::ExistingDirectory, this).exec(ui->collectedFilesDestinationDirectory);
}



bool FileTransferConfigurationPage::event( QEvent* event )
{
	// the base class handles the event first; once the UI exists, a palette change queues a refresh of only the four
	// File transfer directory command icons, which then runs after Qt has resolved the child buttons' palettes (a
	// PaletteChange reaches the page before its children are updated); the event is neither consumed nor changed, and
	// the refresh never browses a directory, edits a line edit, reconnects or hides a control or reads or writes the
	// configuration
	const auto result = ConfigurationPage::event( event );
	if( m_commandIconsReady &&
		( event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange ) )
	{
		QMetaObject::invokeMethod( this, [this]() { updateCommandIcons(); }, Qt::QueuedConnection );
	}
	return result;
}



void FileTransferConfigurationPage::updateCommandIcons()
{
	// each File transfer directory button renders its own role in its own palette at 20 logical px; only the icon and
	// its size change (the FileTransferConfigurationPage.ui document-open icons stay as unused fallback bytes), and no
	// text, tooltip, accessible name, enabled state, visibility, line edit, browse target, connection or configuration
	// is touched
	using NextSpaceActionIcons::AdministrationFileTransferCommand;
	const auto assign = []( QAbstractButton* button, AdministrationFileTransferCommand command ) {
		button->setIcon( NextSpaceActionIcons::forAdministrationFileTransferCommand( command, button->palette() ) );
		button->setIconSize( QSize( 20, 20 ) );
	};
	assign( ui->browseDefaultSourceDirectory, AdministrationFileTransferCommand::BrowseDefaultSourceDirectory );
	assign( ui->browseDestinationDirectory, AdministrationFileTransferCommand::BrowseDestinationDirectory );
	assign( ui->browseFilesToCollectSourceDirectory, AdministrationFileTransferCommand::BrowseFilesToCollectSourceDirectory );
	assign( ui->browseCollectedFilesDestinationDirectory, AdministrationFileTransferCommand::BrowseCollectedFilesDestinationDirectory );
}
