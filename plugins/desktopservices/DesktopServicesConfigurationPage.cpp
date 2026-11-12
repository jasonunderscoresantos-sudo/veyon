/*
 * DesktopServicesConfigurationPage.cpp - implementation of the DesktopServicesConfigurationPage class
 *
 * Copyright (c) 2018-2026 Tobias Junghans <tobydox@veyon.io>
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

#include "DesktopServicesConfiguration.h"
#include "DesktopServicesConfigurationPage.h"
#include "NextSpaceActionIcons.h"
#include "ObjectManager.h"
#include "Configuration/UiMapping.h"

#include "ui_DesktopServicesConfigurationPage.h"

DesktopServicesConfigurationPage::DesktopServicesConfigurationPage( DesktopServicesConfiguration& configuration, QWidget* parent ) :
	ConfigurationPage( parent ),
	ui( new Ui::DesktopServicesConfigurationPage ),
	m_configuration( configuration )
{
	ui->setupUi( this );

	// V3bl: palette-aware Applications & websites command icons at 20 logical px; each icon-only button's accessible
	// name is its existing translated .ui tooltip, so both cues stay identical; only then may palette events refresh
	// the icons (see event())
	updateCommandIcons();
	ui->addApplicationButton->setAccessibleName( ui->addApplicationButton->toolTip() );
	ui->removeApplicationButton->setAccessibleName( ui->removeApplicationButton->toolTip() );
	ui->addWebsiteButton->setAccessibleName( ui->addWebsiteButton->toolTip() );
	ui->removeWebsiteButton->setAccessibleName( ui->removeWebsiteButton->toolTip() );
	m_commandIconsReady = true;
}



DesktopServicesConfigurationPage::~DesktopServicesConfigurationPage()
{
	delete ui;
}



void DesktopServicesConfigurationPage::resetWidgets()
{
	loadObjects( m_configuration.predefinedApplications(), ui->applicationTable );
	loadObjects( m_configuration.predefinedWebsites(), ui->websiteTable );
}



void DesktopServicesConfigurationPage::connectWidgetsToProperties()
{
}



void DesktopServicesConfigurationPage::applyConfiguration()
{
}



void DesktopServicesConfigurationPage::addApplication()
{
	auto applications = m_configuration.predefinedApplications();

	addServiceObject( ui->applicationTable, DesktopServiceObject::Type::Application, tr( "New application" ), applications );

	m_configuration.setPredefinedApplications( applications );
}



void DesktopServicesConfigurationPage::updateApplication()
{
	auto applications = m_configuration.predefinedApplications();

	updateServiceObject( ui->applicationTable, DesktopServiceObject::Type::Application, applications );

	m_configuration.setPredefinedApplications( applications );
}



void DesktopServicesConfigurationPage::removeApplication()
{
	auto applications = m_configuration.predefinedApplications();

	removeServiceObject( ui->applicationTable, DesktopServiceObject::Type::Application, applications );

	m_configuration.setPredefinedApplications( applications );
}



void DesktopServicesConfigurationPage::addWebsite()
{
	auto websites = m_configuration.predefinedWebsites();

	addServiceObject( ui->websiteTable, DesktopServiceObject::Type::Website, tr( "New website" ), websites );

	m_configuration.setPredefinedWebsites( websites );

}

void DesktopServicesConfigurationPage::updateWebsite()
{
	auto websites = m_configuration.predefinedWebsites();

	updateServiceObject( ui->websiteTable, DesktopServiceObject::Type::Website, websites );

	m_configuration.setPredefinedWebsites( websites );
}



void DesktopServicesConfigurationPage::removeWebsite()
{
	auto websites = m_configuration.predefinedWebsites();

	removeServiceObject( ui->websiteTable, DesktopServiceObject::Type::Website, websites );

	m_configuration.setPredefinedWebsites( websites );
}



void DesktopServicesConfigurationPage::addServiceObject( QTableWidget* tableWidget, DesktopServiceObject::Type type,
														 const QString& name, QJsonArray& objects )
{
	ObjectManager<DesktopServiceObject> objectManager( objects );
	objectManager.add(DesktopServiceObject(type, objectManager.generateUniqueName(name)));
	objects = objectManager.objects();

	loadObjects( objects, tableWidget );

	tableWidget->setCurrentCell( tableWidget->rowCount()-1, 0 );
}



void DesktopServicesConfigurationPage::updateServiceObject( QTableWidget* tableWidget, DesktopServiceObject::Type type, QJsonArray& objects )
{
	auto currentServiceObjectIndex = tableWidget->currentIndex();
	if( currentServiceObjectIndex.isValid() == false )
	{
		return;
	}

	ObjectManager<DesktopServiceObject> objectManager( objects );
	objectManager.update( currentServiceObject( tableWidget, type ) );
	objects = objectManager.objects();

	loadObjects( objects, tableWidget );

	tableWidget->setCurrentIndex( currentServiceObjectIndex );
}



void DesktopServicesConfigurationPage::removeServiceObject( QTableWidget* tableWidget, DesktopServiceObject::Type type, QJsonArray& objects )
{
	ObjectManager<DesktopServiceObject> objectManager( objects );
	objectManager.remove( currentServiceObject( tableWidget, type ).uid() );
	objects = objectManager.objects();

	loadObjects( objects, tableWidget );
}



DesktopServiceObject DesktopServicesConfigurationPage::currentServiceObject( QTableWidget* tableWidget, DesktopServiceObject::Type type )
{
	const auto row = tableWidget->currentRow();

	if( row >= 0 )
	{
		auto nameItem = tableWidget->item( row, 0 );
		auto pathItem = tableWidget->item( row, 1 );

		return DesktopServiceObject( type,
									 nameItem->text(),
									 pathItem->text(),
									 nameItem->data( Qt::UserRole ).toUuid() );
	}

	return DesktopServiceObject();
}



void DesktopServicesConfigurationPage::loadObjects( const QJsonArray& objects, QTableWidget* tableWidget )
{
	tableWidget->setUpdatesEnabled( false );
	tableWidget->setRowCount( 0 );

	int rowCount = 0;

	for( const auto& jsonValue : objects )
	{
		const auto serviceObject = DesktopServiceObject( jsonValue.toObject() );

		auto item = new QTableWidgetItem( serviceObject.name() );
		item->setData( Qt::UserRole, serviceObject.uid() );

		tableWidget->setRowCount( rowCount+1 );
		tableWidget->setItem( rowCount, 0, item );
		tableWidget->setItem( rowCount, 1, new QTableWidgetItem( serviceObject.path() ) );
		++rowCount;
	}

	tableWidget->setUpdatesEnabled( true );
}



bool DesktopServicesConfigurationPage::event( QEvent* event )
{
	// the base class handles the event first; once the UI exists, a palette change queues a refresh of only the four
	// Applications & websites command icons, which then runs after Qt has resolved the child buttons' palettes (a
	// PaletteChange reaches the page before its children are updated); the event is neither consumed nor changed, and
	// the refresh never reloads a table, changes a selection, reconnects a control, creates, updates or removes a
	// service object or reads or writes the configuration
	const auto result = ConfigurationPage::event( event );
	if( m_commandIconsReady &&
		( event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange ) )
	{
		QMetaObject::invokeMethod( this, [this]() { updateCommandIcons(); }, Qt::QueuedConnection );
	}
	return result;
}



void DesktopServicesConfigurationPage::updateCommandIcons()
{
	// each Applications & websites button renders its own role in its own palette at 20 logical px; only the icon and
	// its size change (the DesktopServicesConfigurationPage.ui list-add and edit-delete icons stay as unused fallback
	// bytes), and no text, tooltip, accessible name, enabled state, selection, table, connection, service object or
	// configuration is touched
	using NextSpaceActionIcons::AdministrationDesktopServicesCommand;
	const auto assign = []( QAbstractButton* button, AdministrationDesktopServicesCommand command ) {
		button->setIcon( NextSpaceActionIcons::forAdministrationDesktopServicesCommand( command, button->palette() ) );
		button->setIconSize( QSize( 20, 20 ) );
	};
	assign( ui->addApplicationButton, AdministrationDesktopServicesCommand::AddApplication );
	assign( ui->removeApplicationButton, AdministrationDesktopServicesCommand::RemoveApplication );
	assign( ui->addWebsiteButton, AdministrationDesktopServicesCommand::AddWebsite );
	assign( ui->removeWebsiteButton, AdministrationDesktopServicesCommand::RemoveWebsite );
}
