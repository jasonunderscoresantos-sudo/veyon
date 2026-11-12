/*
 * MasterConfigurationPage.cpp - page for configuring master application
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

#include "BuiltinFeatures.h"
#include "PluginManager.h"
#include "FeatureManager.h"
#include "FileSystemBrowser.h"
#include "MonitoringMode.h"
#include "VeyonCore.h"
#include "VeyonConfiguration.h"
#include "MasterConfigurationPage.h"
#include "Configuration/UiMapping.h"
#include "NextSpaceActionIcons.h"

#include "ui_MasterConfigurationPage.h"


MasterConfigurationPage::MasterConfigurationPage() :
	ConfigurationPage(),
	ui(new Ui::MasterConfigurationPage)
{
	ui->setupUi(this);

	// V3bf: palette-aware Teacher Console page command icons at 20 logical px and textual names for the five icon-only
	// directory and feature buttons; only then may palette events refresh the icons (see event())
	updateCommandIcons();
	ui->openUserConfigurationDirectory->setToolTip( tr( "Open user configuration directory" ) );
	ui->openUserConfigurationDirectory->setAccessibleName( tr( "Open user configuration directory" ) );
	ui->openScreenshotDirectory->setToolTip( tr( "Open screenshot directory" ) );
	ui->openScreenshotDirectory->setAccessibleName( tr( "Open screenshot directory" ) );
	ui->openConfigurationTemplatesDirectory->setToolTip( tr( "Open configuration templates directory" ) );
	ui->openConfigurationTemplatesDirectory->setAccessibleName( tr( "Open configuration templates directory" ) );
	ui->disableFeatureButton->setToolTip( tr( "Disable selected feature" ) );
	ui->disableFeatureButton->setAccessibleName( tr( "Disable selected feature" ) );
	ui->enableFeatureButton->setToolTip( tr( "Enable selected feature" ) );
	ui->enableFeatureButton->setAccessibleName( tr( "Enable selected feature" ) );
	m_commandIconsReady = true;

	Configuration::UiMapping::setFlags(ui->advancedSettingsGroupBox, Configuration::Property::Flag::Advanced);

	connect(ui->openUserConfigurationDirectory, &QPushButton::clicked,
			 this, &MasterConfigurationPage::openUserConfigurationDirectory);

	connect(ui->openScreenshotDirectory, &QPushButton::clicked,
			 this, &MasterConfigurationPage::openScreenshotDirectory);

	connect(ui->openConfigurationTemplatesDirectory, &QPushButton::clicked,
			 this, &MasterConfigurationPage::openConfigurationTemplatesDirectory);

	connect(ui->guestUserProperty, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		ui->guestUserValue->setEnabled(ui->guestUserProperty->currentIndex() > 0);
	});

	connect(ui->guestUserIdentityExtensionType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		ui->guestUserIdentityExtensionValue->setEnabled(ui->guestUserIdentityExtensionType->currentIndex() > 0);
	});


	populateFeatureComboBox();
}



MasterConfigurationPage::~MasterConfigurationPage()
{
	delete ui;
}



void MasterConfigurationPage::resetWidgets()
{
	FOREACH_VEYON_DIRECTORIES_CONFIG_PROPERTY(INIT_WIDGET_FROM_PROPERTY);
	FOREACH_VEYON_MASTER_CONFIG_PROPERTY(INIT_WIDGET_FROM_PROPERTY);

	m_disabledFeatures = VeyonCore::config().disabledFeatures();

	updateFeatureLists();
}



void MasterConfigurationPage::connectWidgetsToProperties()
{
	FOREACH_VEYON_DIRECTORIES_CONFIG_PROPERTY(CONNECT_WIDGET_TO_PROPERTY);
	FOREACH_VEYON_MASTER_CONFIG_PROPERTY(CONNECT_WIDGET_TO_PROPERTY);
}



void MasterConfigurationPage::applyConfiguration()
{
}



void MasterConfigurationPage::enableFeature()
{
	const auto items = ui->disabledFeaturesListWidget->selectedItems();

	for( auto item : items )
	{
		m_disabledFeatures.removeAll( item->data( Qt::UserRole ).toString() );
	}

	VeyonCore::config().setDisabledFeatures( m_disabledFeatures );

	updateFeatureLists();
}



void MasterConfigurationPage::disableFeature()
{
	const auto items = ui->allFeaturesListWidget->selectedItems();

	for( auto item : items )
	{
		QString featureUid = item->data( Qt::UserRole ).toString();
		m_disabledFeatures.removeAll( featureUid );
		m_disabledFeatures.append( featureUid );
	}

	VeyonCore::config().setDisabledFeatures( m_disabledFeatures );

	updateFeatureLists();
}



void MasterConfigurationPage::openUserConfigurationDirectory()
{
	FileSystemBrowser(FileSystemBrowser::ExistingDirectory, this).exec(ui->userConfigurationDirectory);
}



void MasterConfigurationPage::openScreenshotDirectory()
{
	FileSystemBrowser(FileSystemBrowser::ExistingDirectory, this).exec(ui->screenshotDirectory);
}



void MasterConfigurationPage::openConfigurationTemplatesDirectory()
{
	FileSystemBrowser(FileSystemBrowser::ExistingDirectory, this).exec(ui->configurationTemplatesDirectory);
}



void MasterConfigurationPage::populateFeatureComboBox()
{
	ui->computerDoubleClickFeature->addItem( QIcon(), tr( "<no feature>" ), QUuid() );
	ui->computerDoubleClickFeature->insertSeparator( ui->computerDoubleClickFeature->count() );

	for( const auto& feature : VeyonCore::featureManager().features() )
	{
		if( feature.testFlag( Feature::Flag::Master ) &&
			feature.testFlag( Feature::Flag::Meta ) == false )
		{
			ui->computerDoubleClickFeature->addItem( NextSpaceActionIcons::forFeature( feature, ui->computerDoubleClickFeature->palette() ),
													 feature.displayName(),
													 feature.uid() );
		}
	}
}



void MasterConfigurationPage::updateFeatureLists()
{
	ui->allFeaturesListWidget->setUpdatesEnabled( false );
	ui->disabledFeaturesListWidget->setUpdatesEnabled( false );

	ui->allFeaturesListWidget->clear();
	ui->disabledFeaturesListWidget->clear();

	for( const auto& feature : std::as_const( VeyonCore::featureManager().features() ) )
	{
		if( feature.testFlag( Feature::Flag::Master ) == false ||
			feature.testFlag( Feature::Flag::Meta ) ||
			feature == VeyonCore::builtinFeatures().monitoringMode().feature() )
		{
			continue;
		}

		// V3bg: the disabled-feature membership chooses the destination list first, so the item's icon is drawn in that
		// list's own palette
		auto destination = ui->allFeaturesListWidget;
		if( m_disabledFeatures.contains( feature.uid().toString() ) )
		{
			destination = ui->disabledFeaturesListWidget;
		}

		auto item = new QListWidgetItem( NextSpaceActionIcons::forFeature( feature, destination->palette() ),
										 feature.displayName() );
		item->setData( Qt::UserRole, feature.uid().toString() );
		destination->addItem( item );
	}

	ui->allFeaturesListWidget->setUpdatesEnabled( true );
	ui->disabledFeaturesListWidget->setUpdatesEnabled( true );
}



bool MasterConfigurationPage::event( QEvent* event )
{
	// the base class handles the event first; once the UI exists, a palette change queues a refresh of only the five
	// command icons and then (V3bg) the feature item icons, which runs after Qt has resolved the child controls' palettes
	// (a PaletteChange reaches the page before its children are updated); the event is neither consumed nor changed
	const auto result = ConfigurationPage::event( event );
	if( m_commandIconsReady &&
		( event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange ) )
	{
		QMetaObject::invokeMethod( this, [this]() { updateCommandIcons(); updateFeatureIcons(); }, Qt::QueuedConnection );
	}
	return result;
}



void MasterConfigurationPage::updateCommandIcons()
{
	// each command button renders its role in its own palette at 20 logical px; only the icon and its size change (the
	// MasterConfigurationPage.ui icons stay as unused fallback bytes), and no text, tooltip, tab stop, state, connection,
	// feature list or disabled-feature configuration is touched
	using NextSpaceActionIcons::AdministrationTeacherConsoleCommand;
	const auto assign = []( QAbstractButton* button, AdministrationTeacherConsoleCommand command ) {
		button->setIcon( NextSpaceActionIcons::forAdministrationTeacherConsoleCommand( command, button->palette() ) );
		button->setIconSize( QSize( 20, 20 ) );
	};
	assign( ui->openUserConfigurationDirectory, AdministrationTeacherConsoleCommand::OpenUserConfigurationDirectory );
	assign( ui->openScreenshotDirectory, AdministrationTeacherConsoleCommand::OpenScreenshotDirectory );
	assign( ui->openConfigurationTemplatesDirectory, AdministrationTeacherConsoleCommand::OpenConfigurationTemplatesDirectory );
	assign( ui->disableFeatureButton, AdministrationTeacherConsoleCommand::DisableFeature );
	assign( ui->enableFeatureButton, AdministrationTeacherConsoleCommand::EnableFeature );
}



void MasterConfigurationPage::updateFeatureIcons()
{
	// V3bg: each existing feature item is redrawn in place in its own combo box's or list's palette; the iconless
	// <no feature> row and the separator (no feature UID) and any UID the feature manager no longer resolves keep their
	// icon, and no item is added, removed, reordered, selected, renamed or given other data, no list is rebuilt, no
	// feature runs and no configuration is written
	const auto& featureManager = VeyonCore::featureManager();

	auto comboBox = ui->computerDoubleClickFeature;
	for( int index = 0; index < comboBox->count(); ++index )
	{
		const auto uid = comboBox->itemData( index ).toUuid();
		if( uid.isNull() )
		{
			continue;
		}
		const auto& feature = featureManager.feature( uid );
		if( feature.uid() == uid )
		{
			comboBox->setItemIcon( index, NextSpaceActionIcons::forFeature( feature, comboBox->palette() ) );
		}
	}

	for( auto listWidget : { ui->allFeaturesListWidget, ui->disabledFeaturesListWidget } )
	{
		for( int row = 0; row < listWidget->count(); ++row )
		{
			auto item = listWidget->item( row );
			const Feature::Uid uid( item->data( Qt::UserRole ).toString() );
			if( uid.isNull() )
			{
				continue;
			}
			const auto& feature = featureManager.feature( uid );
			if( feature.uid() == uid )
			{
				item->setIcon( NextSpaceActionIcons::forFeature( feature, listWidget->palette() ) );
			}
		}
	}
}
