/*
 * MainWindow.cpp - implementation of MainWindow class
 *
 * Copyright (c) 2010-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QActionGroup>
#include <QDir>
#include <QProcess>
#include <QCloseEvent>
#include <QFileDialog>
#include <QHash>
#include <QPushButton>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>

#include "Configuration/JsonStore.h"
#include "Configuration/UiMapping.h"

#include "AboutDialog.h"
#include "ConfigurationPagePluginInterface.h"
#include "ConfigurationManager.h"
#include "VeyonConfiguration.h"
#include "MainWindow.h"
#include "NextSpaceActionIcons.h"
#include "PluginManager.h"

#include "ui_MainWindow.h"



namespace {

// V3bb: the approved Administration page roles by the exact, case-sensitive and non-translated value of a page root's
// nextspaceActionIconRole property; any other value, or none, leaves the page's selector icon unchanged
const QHash<QString, NextSpaceActionIcons::AdministrationPage>& administrationPageRoles()
{
	using NextSpaceActionIcons::AdministrationPage;
	static const QHash<QString, AdministrationPage> roles{
		{ QStringLiteral("General"), AdministrationPage::General },
		{ QStringLiteral("StudentAgent"), AdministrationPage::StudentAgent },
		{ QStringLiteral("TeacherConsole"), AdministrationPage::TeacherConsole },
		{ QStringLiteral("AccessControl"), AdministrationPage::AccessControl },
		{ QStringLiteral("AuthenticationKeys"), AdministrationPage::AuthenticationKeys },
		{ QStringLiteral("LocationsAndComputers"), AdministrationPage::LocationsAndComputers },
		{ QStringLiteral("Broadcast"), AdministrationPage::Broadcast },
		{ QStringLiteral("ApplicationsAndWebsites"), AdministrationPage::ApplicationsAndWebsites },
		{ QStringLiteral("FileTransfer"), AdministrationPage::FileTransfer },
		{ QStringLiteral("Windows"), AdministrationPage::Windows },
		{ QStringLiteral("LdapDirectory"), AdministrationPage::LdapDirectory },
	};
	return roles;
}

}


MainWindow::MainWindow( QWidget* parent ) :
	QMainWindow( parent ),
	ui( new Ui::MainWindow ),
	m_configChanged( false )
{
	ui->setupUi( this );

	setWindowTitle(tr("NextSpace Classroom Administration - Development build based on Veyon %1").arg(VeyonCore::versionString()));

	loadConfigurationPagePlugins();

	// V3bb: palette-aware Administration page identities, refreshed when the selector's palette changes
	updatePageIcons();
	ui->pageSelector->installEventFilter( this );

	// V3bc: palette-aware Administration menu-command icons, refreshed when the owning menu's palette changes
	updateCommandIcons();
	ui->menu_File->installEventFilter( this );
	ui->menu_Help->installEventFilter( this );

	// reset all widget's values to current configuration
	reset( true );

	// if local configuration is incomplete, re-enable the apply button
	if( VeyonConfiguration().data().size() < VeyonCore::config().data().size() )
	{
		configurationChanged();
	}

	const auto pages = findChildren<ConfigurationPage *>();
	for( auto page : pages )
	{
		page->connectWidgetsToProperties();
		connect( page, &ConfigurationPage::widgetsChanged, this, &MainWindow::updateView );
	}

	connect( ui->buttonBox, &QDialogButtonBox::clicked, this, &MainWindow::resetOrApply );

	connect( ui->actionLoadSettings, &QAction::triggered, this, &MainWindow::loadSettingsFromFile );
	connect( ui->actionSaveSettings, &QAction::triggered, this, &MainWindow::saveSettingsToFile );

	auto viewModeGroup = new QActionGroup( this );
	viewModeGroup->addAction( ui->viewModeStandard );
	viewModeGroup->addAction( ui->viewModeAdvanced );
	viewModeGroup->setExclusive( true );
	ui->viewModeStandard->setChecked( true );

	connect( viewModeGroup, &QActionGroup::triggered, this, &MainWindow::updateView );

	connect( ui->actionAboutQt, &QAction::triggered, QApplication::instance(), &QApplication::aboutQt );

	connect( &VeyonCore::config(), &VeyonConfiguration::configurationChanged, this, &MainWindow::configurationChanged );

	connect( ui->configPages, &QStackedWidget::currentChanged, this, &MainWindow::updateSizes );

	resize( ui->pageSelector->width() + ui->generalConfigurationPage->minimumSizeHint().width(),
			ui->generalConfigurationPage->minimumSizeHint().height() );

	updateView();
}



MainWindow::~MainWindow()
{
	delete ui;
}



void MainWindow::reset( bool onlyUI )
{
	if( onlyUI == false )
	{
		VeyonCore::config().clear();
		VeyonCore::config().reloadFromStore();
	}

	const auto pages = findChildren<ConfigurationPage *>();
	for( auto page : pages )
	{
		page->resetWidgets();
	}

	ui->buttonBox->setEnabled( false );
	m_configChanged = false;
}




void MainWindow::apply()
{
	const auto showError = [this](const ConfigurationManager& configurationManager) {
		vCritical() << configurationManager.errorString().toUtf8().constData();

		QMessageBox::critical(this, tr("NextSpace Classroom Administration"), configurationManager.errorString());
	};

	ConfigurationManager configurationManager;
	if (configurationManager.saveConfiguration())
	{
		if (configurationManager.applyConfiguration() == false)
		{
			showError(configurationManager);
		}

		const auto pages = findChildren<ConfigurationPage *>();
		for( auto page : pages )
		{
			page->applyConfiguration();
		}

		ui->buttonBox->setEnabled( false );
		m_configChanged = false;
	}
	else
	{
		showError(configurationManager);
	}
}



void MainWindow::resizeEvent( QResizeEvent* event )
{
	QMainWindow::resizeEvent( event );

	updateSizes();
}




bool MainWindow::eventFilter( QObject* watched, QEvent* event )
{
	// only the mapped page icons follow a palette change; the current row, labels, order, flags, visibility and every
	// other icon stay as they are
	if( watched == ui->pageSelector &&
		( event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange ) )
	{
		updatePageIcons();
	}

	// V3bc: only the four mapped menu-command icons follow a palette change of their owning menu; every action label,
	// mnemonic, shortcut, tooltip, state and connection and the menu order stay as they are
	if( ( watched == ui->menu_File || watched == ui->menu_Help ) &&
		( event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange ) )
	{
		updateCommandIcons();
	}

	return QMainWindow::eventFilter( watched, event );
}




void MainWindow::configurationChanged()
{
	ui->buttonBox->setEnabled( true );
	m_configChanged = true;
}




void MainWindow::resetOrApply( QAbstractButton *btn )
{
	if( ui->buttonBox->standardButton( btn ) & QDialogButtonBox::Apply )
	{
		apply();
	}
	else if( ui->buttonBox->standardButton( btn ) & QDialogButtonBox::Reset )
	{
		reset();
	}
}



void MainWindow::loadSettingsFromFile()
{
	QString fileName = QFileDialog::getOpenFileName( this, tr( "Load settings from file" ),
													 QDir::homePath(), tr( "JSON files (*.json)" ) );
	if( !fileName.isEmpty() )
	{
		// write current configuration to output file
		Configuration::JsonStore(Configuration::JsonStore::Scope::System, fileName).load(&VeyonCore::config());
		reset( true );
		configurationChanged();	// give user a chance to apply possible changes
	}
}




void MainWindow::saveSettingsToFile()
{
	QString fileName = QFileDialog::getSaveFileName( this, tr( "Save settings to file" ),
													 QDir::homePath(), tr( "JSON files (*.json)" ) );
	if( !fileName.isEmpty() )
	{
		if( !fileName.endsWith( QStringLiteral(".json"), Qt::CaseInsensitive ) )
		{
			fileName += QStringLiteral(".json");
		}

		bool configChangedPrevious = m_configChanged;

		// write current configuration to output file
		Configuration::JsonStore(Configuration::JsonStore::Scope::System, fileName).flush(&VeyonCore::config());

		m_configChanged = configChangedPrevious;
		ui->buttonBox->setEnabled( m_configChanged );
	}
}



void MainWindow::resetConfiguration()
{
	if( QMessageBox::warning( this, tr( "Reset configuration" ),
							  tr( "Do you really want to reset the local configuration and revert "
								  "all settings to their defaults?" ), QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) ==
		QMessageBox::Yes )
	{
		ConfigurationManager().clearConfiguration();
		reset( false );
	}
}



void MainWindow::aboutVeyon()
{
	AboutDialog( this ).exec();
}



void MainWindow::updateSizes()
{
	ui->configPages->setMinimumSize( ui->scrollArea->width() - ui->scrollArea->verticalScrollBar()->width(),
									 ui->configPages->currentWidget()->minimumSizeHint().height() );
}



void MainWindow::updateView()
{
	if( ui->viewModeAdvanced->isChecked() )
	{
		switchToAdvancedView();
	}
	else
	{
		switchToStandardView();
	}

	QTimer::singleShot( 0, this, &MainWindow::updateSizes );
}



void MainWindow::switchToStandardView()
{
	const auto widgets = findChildren<QWidget *>();

	// hide widgets with advanced property flag
	for( auto widget : widgets )
	{
		const auto flags = Configuration::UiMapping::flags( widget );
		if( flags & Configuration::Property::Flag::Hidden )
		{
			widget->hide();
		}
		else if( flags & Configuration::Property::Flag::Advanced )
		{
			widget->hide();
		}
	}

	// hide page selector items for advanced pages
	for( int i = 0; i < ui->pageSelector->count(); ++i )
	{
		auto item = ui->pageSelector->item( i );
		auto page = item->data( Qt::UserRole ).value<ConfigurationPage *>();
		if( page )
		{
			const auto flags = Configuration::UiMapping::flags( page );

			ui->pageSelector->setRowHidden( i, flags & Configuration::Property::Flag::Advanced );
			ui->configPages->widget( i )->setVisible( i == ui->pageSelector->currentRow() );
			if( flags & Configuration::Property::Flag::Advanced &&
				i == ui->pageSelector->currentRow() )
			{
				ui->pageSelector->setCurrentRow( 0 );
			}
		}
	}
}



void MainWindow::switchToAdvancedView()
{
	const auto widgets = findChildren<QWidget *>();
	for( auto widget : widgets )
	{
		const auto flags = Configuration::UiMapping::flags( widget );
		if( flags & Configuration::Property::Flag::Advanced )
		{
			widget->show();
		}
	}

	for( int i = 0; i < ui->pageSelector->count(); ++i )
	{
		ui->pageSelector->setRowHidden( i, false );
		ui->configPages->widget( i )->setVisible( i == ui->pageSelector->currentRow() );
	}
}



void MainWindow::loadConfigurationPagePlugins()
{
	for( auto pluginObject : std::as_const( VeyonCore::pluginManager().pluginObjects() ) )
	{
		auto pluginInterface = qobject_cast<PluginInterface *>( pluginObject );
		auto configurationPagePluginInterface = qobject_cast<ConfigurationPagePluginInterface *>( pluginObject );

		if( pluginInterface && configurationPagePluginInterface )
		{
			auto page = configurationPagePluginInterface->createConfigurationPage();
			if( page )
			{
				ui->configPages->addWidget( page );

				auto item = new QListWidgetItem( page->windowIcon(), page->windowTitle() );
				item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
				item->setData( Qt::UserRole, QVariant::fromValue( page ) );
				ui->pageSelector->addItem( item );
			}
		}
	}

	// adjust minimum size
	ui->pageSelector->setMinimumSize( ui->pageSelector->sizeHintForColumn(0) + 3 * ui->pageSelector->spacing(),
									  ui->pageSelector->minimumHeight() );
}



void MainWindow::updatePageIcons()
{
	// selector row i shows stacked page i (the built-in pages of MainWindow.ui, then the plugin pages in load order);
	// only a page with an approved nextspaceActionIconRole gets the shared palette-aware icon, every other row keeps its
	// current icon (the built-in fallback art or the plugin page's windowIcon())
	const auto palette = ui->pageSelector->palette();
	for( int i = 0; i < ui->pageSelector->count() && i < ui->configPages->count(); ++i )
	{
		const auto role = ui->configPages->widget( i )->property( "nextspaceActionIconRole" );
		if( role.typeId() != QMetaType::QString )
		{
			continue;
		}
		const auto it = administrationPageRoles().constFind( role.toString() );
		if( it != administrationPageRoles().constEnd() )
		{
			ui->pageSelector->item( i )->setIcon( NextSpaceActionIcons::forAdministrationPage( *it, palette ) );
		}
	}
}



void MainWindow::updateCommandIcons()
{
	// the File-menu commands render from the File menu's palette and About from the Help menu's palette; only these four
	// icons are replaced (the MainWindow.ui icons stay as unused fallback bytes), and no action text, shortcut, state or
	// connection is touched
	using NextSpaceActionIcons::AdministrationCommand;
	const auto filePalette = ui->menu_File->palette();
	ui->actionSaveSettings->setIcon( NextSpaceActionIcons::forAdministrationCommand( AdministrationCommand::SaveSettings, filePalette ) );
	ui->actionLoadSettings->setIcon( NextSpaceActionIcons::forAdministrationCommand( AdministrationCommand::LoadSettings, filePalette ) );
	ui->actionResetConfiguration->setIcon( NextSpaceActionIcons::forAdministrationCommand( AdministrationCommand::ResetConfiguration, filePalette ) );
	ui->actionAboutVeyon->setIcon( NextSpaceActionIcons::forAdministrationCommand( AdministrationCommand::About, ui->menu_Help->palette() ) );
}



void MainWindow::closeEvent( QCloseEvent *closeEvent )
{
	if( m_configChanged &&
		QMessageBox::question( this, tr( "Unsaved settings" ),
							   tr( "There are unsaved settings. "
								   "Quit anyway?" ),
							   QMessageBox::Yes | QMessageBox::No ) !=
		QMessageBox::Yes )
	{
		closeEvent->ignore();
		return;
	}

	closeEvent->accept();
	QMainWindow::closeEvent( closeEvent );
}
