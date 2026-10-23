/*
 * MainWindow.cpp - implementation of MainWindow class
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

#include <QCloseEvent>
#include <QFileDialog>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QHostAddress>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QSplitter>
#include <QVBoxLayout>

#include "AboutDialog.h"
#include "AccessControlProvider.h"
#include "MainWindow.h"
#include "BuiltinFeatures.h"
#include "AuthenticationCredentials.h"
#include "ComputerControlListModel.h"
#include "ComputerManager.h"
#include "ComputerSelectPanel.h"
#include "ScreenshotManagementPanel.h"
#include "FeatureManager.h"
#include "MonitoringMode.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "PlatformUserFunctions.h"
#include "SlideshowPanel.h"
#include "SpotlightPanel.h"
#include "ToolButton.h"
#include "VeyonConfiguration.h"
#include "VeyonMaster.h"
#include "UserConfig.h"

#include "ui_MainWindow.h"


namespace {

// NextSpace action grouping: secondary features shown in the "More" toolbar menu in this order;
// empty entries separate groups. Features not listed here stay on the toolbar.
const QStringList& moreMenuLayout()
{
	static const QStringList layout{
		QStringLiteral("RemoteView"), QStringLiteral("FileCollect"), QStringLiteral("Screenshot"), {},
		QStringLiteral("StartApp"), QStringLiteral("OpenWebsite"), {},
		QStringLiteral("PowerOn"), QStringLiteral("Reboot"), QStringLiteral("PowerDown"),
		QStringLiteral("UserLogin"), QStringLiteral("UserLogoff")
	};
	return layout;
}



// three horizontal dots drawn in the current text color; avoids bundling another icon asset
QIcon moreActionsIcon( const QColor& color )
{
	QIcon icon;
	for( const auto size : { 16, 20, 24, 32, 48, 64 } )
	{
		QPixmap pixmap( size, size );
		pixmap.fill( Qt::transparent );
		QPainter painter( &pixmap );
		painter.setRenderHint( QPainter::Antialiasing );
		painter.setPen( Qt::NoPen );
		painter.setBrush( color );
		const auto radius = size / 10.0;
		for( const auto x : { 0.25, 0.5, 0.75 } )
		{
			painter.drawEllipse( QPointF( size * x, size / 2.0 ), radius, radius );
		}
		painter.end();
		icon.addPixmap( pixmap );
	}
	return icon;
}

}


MainWindow::MainWindow( VeyonMaster &masterCore, QWidget* parent ) :
	QMainWindow( parent ),
	ui( new Ui::MainWindow ),
	m_master( masterCore ),
	m_modeGroup( new QButtonGroup( this ) )
{
	ui->setupUi( this );

	restoreState( QByteArray::fromBase64( m_master.userConfig().windowState().toUtf8() ) );
	restoreGeometry( QByteArray::fromBase64( m_master.userConfig().windowGeometry().toUtf8() ) );

	// add panel toggles to status bar
	ui->statusBar->addWidget( ui->panelButtons );
	// NextSpace accessibility: the Screenshots view-strip toggle takes keyboard focus like the other three
	ui->screenshotManagementPanelButton->setFocusPolicy( Qt::StrongFocus );

	// NextSpace layout: search, filters and view controls sit directly above the computer grid
	static constexpr int ControlHeight = 36;
	auto monitoringArea = new QWidget;
	monitoringArea->setObjectName( QStringLiteral("MonitoringArea") );
	auto monitoringAreaLayout = new QVBoxLayout( monitoringArea );
	monitoringAreaLayout->setContentsMargins( 0, 0, 0, 0 );
	monitoringAreaLayout->setSpacing( 0 );

	auto monitoringControlsLayout = new QHBoxLayout;
	monitoringControlsLayout->setContentsMargins( 12, 8, 12, 8 );
	monitoringControlsLayout->setSpacing( 8 );
	monitoringControlsLayout->addWidget( ui->filterLineEdit, 3 );
	monitoringControlsLayout->addWidget( ui->filterPoweredOnComputersButton );
	monitoringControlsLayout->addWidget( ui->filterComputersWithLoggedOnUsersButton );
	monitoringControlsLayout->addSpacing( 16 );

	// selection summary: describes, but does not change, which computers actions apply to
	auto selectionSummaryLabel = new QLabel;
	selectionSummaryLabel->setObjectName( QStringLiteral("SelectionSummaryLabel") );
	selectionSummaryLabel->setToolTip( tr("Toolbar and More actions apply to all computers shown. "
										  "Remote View and Remote Control open the selected computers, or ask for an address "
										  "if none is selected. Broadcasting a student's screen needs exactly one selected computer. "
										  "Right-click menu actions apply to the selected computers, or to ALL computers shown "
										  "when none is selected (including power actions).") );
	monitoringControlsLayout->addWidget( selectionSummaryLabel );
	monitoringControlsLayout->addSpacing( 16 );

	const auto updateSelectionSummary = [this, selectionSummaryLabel]() {
		selectionSummaryLabel->setText( tr("%1 shown · %2 selected")
											.arg( ui->computerMonitoringWidget->model()->rowCount() )
											.arg( ui->computerMonitoringWidget->selectionModel()->selectedIndexes().size() ) );
	};
	const auto monitoringModel = ui->computerMonitoringWidget->model();
	connect( ui->computerMonitoringWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateSelectionSummary );
	connect( monitoringModel, &QAbstractItemModel::rowsInserted, this, updateSelectionSummary );
	connect( monitoringModel, &QAbstractItemModel::rowsRemoved, this, updateSelectionSummary );
	connect( monitoringModel, &QAbstractItemModel::modelReset, this, updateSelectionSummary );
	connect( monitoringModel, &QAbstractItemModel::layoutChanged, this, updateSelectionSummary );
	updateSelectionSummary();
	monitoringControlsLayout->addWidget( ui->gridSizeSlider, 1 );
	monitoringControlsLayout->addWidget( ui->autoAdjustComputerIconSizeButton );
	monitoringControlsLayout->addWidget( ui->useCustomComputerPositionsButton );
	monitoringControlsLayout->addWidget( ui->alignComputersButton );

	ui->filterLineEdit->setMinimumHeight( ControlHeight );
	for( auto* button : { ui->filterPoweredOnComputersButton, ui->filterComputersWithLoggedOnUsersButton,
						  ui->autoAdjustComputerIconSizeButton, ui->useCustomComputerPositionsButton,
						  ui->alignComputersButton } )
	{
		button->setMinimumSize( ControlHeight, ControlHeight );
	}

	ui->centralLayout->removeWidget( ui->computerMonitoringWidget );
	monitoringAreaLayout->addLayout( monitoringControlsLayout );
	monitoringAreaLayout->addWidget( ui->computerMonitoringWidget, 1 );

	// NextSpace empty state: a non-interactive overlay inside the unchanged monitoring view, shown only while the
	// view's filtered model has no rows; the view keeps its focus, selection, drop and context-menu behavior
	auto emptyState = new QWidget( ui->computerMonitoringWidget->viewport() );
	emptyState->setObjectName( QStringLiteral("MonitoringEmptyState") );
	emptyState->setAttribute( Qt::WA_TransparentForMouseEvents );
	emptyState->setFocusPolicy( Qt::NoFocus );
	auto emptyStateLayout = new QVBoxLayout( emptyState );
	emptyStateLayout->setContentsMargins( 24, 24, 24, 24 );
	emptyStateLayout->setSpacing( 8 );
	emptyStateLayout->addStretch( 1 );
	auto emptyStateHeading = new QLabel( tr("No computers to display"), emptyState );
	auto emptyStateHelper = new QLabel( tr("Review your existing locations and filters."), emptyState );
	for( auto* label : { emptyStateHeading, emptyStateHelper } )
	{
		label->setTextFormat( Qt::PlainText );
		label->setAlignment( Qt::AlignCenter );
		label->setWordWrap( true );
		label->setFocusPolicy( Qt::NoFocus );
		label->setForegroundRole( QPalette::Text );
		emptyStateLayout->addWidget( label );
	}
	auto emptyStateHeadingFont = emptyStateHeading->font();
	emptyStateHeadingFont.setPixelSize( 24 );
	emptyStateHeadingFont.setWeight( QFont::DemiBold );
	emptyStateHeading->setFont( emptyStateHeadingFont );
	auto emptyStateHelperFont = emptyStateHelper->font();
	emptyStateHelperFont.setPixelSize( 14 );
	emptyStateHelper->setFont( emptyStateHelperFont );
	emptyStateLayout->addStretch( 1 );

	auto viewportLayout = new QVBoxLayout( ui->computerMonitoringWidget->viewport() );
	viewportLayout->setSizeConstraint( QLayout::SetNoConstraint );
	viewportLayout->setContentsMargins( 0, 0, 0, 0 );
	viewportLayout->addWidget( emptyState );

	const auto updateEmptyState = [this, emptyState]() {
		emptyState->setVisible( ui->computerMonitoringWidget->model()->rowCount() == 0 );
	};
	connect( monitoringModel, &QAbstractItemModel::rowsInserted, this, updateEmptyState );
	connect( monitoringModel, &QAbstractItemModel::rowsRemoved, this, updateEmptyState );
	connect( monitoringModel, &QAbstractItemModel::modelReset, this, updateEmptyState );
	connect( monitoringModel, &QAbstractItemModel::layoutChanged, this, updateEmptyState );
	updateEmptyState();

	// create all views
	auto mainSplitter = new QSplitter( Qt::Horizontal, ui->centralWidget );
	mainSplitter->setChildrenCollapsible( false );
	mainSplitter->setObjectName( QStringLiteral("MainSplitter") );

	auto monitoringSplitter = new QSplitter( Qt::Vertical, mainSplitter );
	monitoringSplitter->setChildrenCollapsible( false );
	monitoringSplitter->setObjectName( QStringLiteral("MonitoringSplitter") );

	auto slideshowSpotlightSplitter = new QSplitter( Qt::Horizontal, monitoringSplitter );
	slideshowSpotlightSplitter->setChildrenCollapsible( false );
	slideshowSpotlightSplitter->setObjectName( QStringLiteral("SlideshowSpotlightSplitter") );

	auto computerSelectPanel = new ComputerSelectPanel( m_master.computerManager() );
	auto screenshotManagementPanel = new ScreenshotManagementPanel();
	auto slideshowPanel = new SlideshowPanel( m_master.userConfig(), ui->computerMonitoringWidget );
	auto spotlightPanel = new SpotlightPanel( m_master.userConfig(), ui->computerMonitoringWidget );

	slideshowSpotlightSplitter->addWidget( slideshowPanel );
	slideshowSpotlightSplitter->addWidget( spotlightPanel );
	slideshowSpotlightSplitter->setStretchFactor( slideshowSpotlightSplitter->indexOf(slideshowPanel), 1 );
	slideshowSpotlightSplitter->setStretchFactor( slideshowSpotlightSplitter->indexOf(spotlightPanel), 1 );

	monitoringSplitter->addWidget( slideshowSpotlightSplitter );
	monitoringSplitter->addWidget( monitoringArea );
	monitoringSplitter->setStretchFactor( monitoringSplitter->indexOf(slideshowSpotlightSplitter), 1 );
	monitoringSplitter->setStretchFactor( monitoringSplitter->indexOf(monitoringArea), 1 );

	mainSplitter->addWidget( computerSelectPanel );
	mainSplitter->addWidget( screenshotManagementPanel );
	mainSplitter->addWidget( monitoringSplitter );

	mainSplitter->setStretchFactor( mainSplitter->indexOf(monitoringSplitter), 1 );


	static const QHash<QWidget *, QAbstractButton *> panelButtons{
		{ computerSelectPanel, ui->computerSelectPanelButton },
		{ screenshotManagementPanel, ui->screenshotManagementPanelButton },
		{ slideshowPanel, ui->slideshowPanelButton },
		{ spotlightPanel, ui->spotlightPanelButton }
	};

	for( auto it = panelButtons.constBegin(), end = panelButtons.constEnd(); it != end; ++it )
	{
		it.key()->hide();
		it.key()->installEventFilter( this );
		connect( *it, &QAbstractButton::toggled, it.key(), &QWidget::setVisible );
	}

	QList<int> splitterSizes;
	for( auto* splitter : { slideshowSpotlightSplitter, monitoringSplitter, mainSplitter } )
	{
		splitter->setHandleWidth( 7 );
		splitter->setStyleSheet( QStringLiteral("QSplitter::handle:hover{background-color:#66a0b3;}") );

		splitter->installEventFilter( this );

		int index = 0;

		const auto splitterStates = m_master.userConfig().splitterStates()[splitter->objectName()].toArray();
		splitterSizes.clear();
		splitterSizes.reserve( splitterStates.size() );

		for( const auto& sizeObject : splitterStates )
		{
			auto size = sizeObject.toInt();
			const auto widget = splitter->widget( index );
			const auto button = panelButtons.value( widget );

			if( widget )
			{
				if( button )
				{
					widget->setVisible( size > 0 );
					button->setChecked( size > 0 );
				}
				size = qAbs( size );
				if( splitter->orientation() == Qt::Horizontal )
				{
					widget->resize( size, widget->height() );
				}
				else
				{
					widget->resize( widget->width(), size );
				}

				widget->setProperty( originalSizePropertyName(), widget->size() );
			}
			splitterSizes.append( size );
			++index;
		}
		splitter->setSizes( splitterSizes );
	}

	const auto SplitterContentBaseSize = 500;

	if( spotlightPanel->property( originalSizePropertyName() ).isNull() ||
		slideshowPanel->property( originalSizePropertyName() ).isNull() )
	{
		slideshowSpotlightSplitter->setSizes( { SplitterContentBaseSize, SplitterContentBaseSize } );
	}

	if( slideshowSpotlightSplitter->property( originalSizePropertyName() ).isNull() ||
		monitoringArea->property( originalSizePropertyName() ).isNull() )
	{
		monitoringSplitter->setSizes( { SplitterContentBaseSize, SplitterContentBaseSize } );
	}

	ui->centralLayout->addWidget( mainSplitter );

	if( VeyonCore::config().autoOpenComputerSelectPanel() )
	{
		ui->computerSelectPanelButton->setChecked( true );
	}

	// initialize filter controls
	connect( ui->filterLineEdit, &QLineEdit::textChanged,
			 this, [this]( const QString& filter ) { ui->computerMonitoringWidget->setSearchFilter( filter ); } );
	connect( ui->filterPoweredOnComputersButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) { ui->computerMonitoringWidget->setFilterPoweredOnComputers( enabled ); } );
	connect( ui->filterComputersWithLoggedOnUsersButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) { ui->computerMonitoringWidget->setFilterComputersWithLoggedOnUsers( enabled ); } );
	ui->filterPoweredOnComputersButton->setChecked(m_master.userConfig().filterPoweredOnComputers());
	ui->filterComputersWithLoggedOnUsersButton->setChecked(m_master.userConfig().filterComputersWithLoggedOnUsers());
	// NextSpace accessibility: concise, translated names for the search field and the icon-only controls
	ui->filterLineEdit->setAccessibleName( tr("Search users and computers") );
	ui->filterPoweredOnComputersButton->setAccessibleName( tr("Only show powered on computers") );
	ui->filterComputersWithLoggedOnUsersButton->setAccessibleName( tr("Only show computers with logged on users") );

	// initialize monitoring screen size slider
	ui->gridSizeSlider->setMinimum( ComputerMonitoringWidget::MinimumComputerScreenSize );
	ui->gridSizeSlider->setMaximum( ComputerMonitoringWidget::MaximumComputerScreenSize );
	ui->gridSizeSlider->setAccessibleName( tr("Computer thumbnail size") );

	ui->autoAdjustComputerIconSizeButton->setChecked( ui->computerMonitoringWidget->autoAdjustIconSize() );

	connect( ui->gridSizeSlider, &QSlider::valueChanged,
			 this, [this]( int size ) { ui->computerMonitoringWidget->setComputerScreenSize( size ); } );
	connect( ui->computerMonitoringWidget, &ComputerMonitoringWidget::computerScreenSizeAdjusted,
			 ui->gridSizeSlider, &QSlider::setValue );
	connect( ui->autoAdjustComputerIconSizeButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) {
				 ui->computerMonitoringWidget->setAutoAdjustIconSize( enabled );
				 m_master.userConfig().setAutoAdjustMonitoringIconSize( enabled );
			 } );

	int size = ComputerMonitoringWidget::DefaultComputerScreenSize;
	if( m_master.userConfig().monitoringScreenSize() >= ComputerMonitoringWidget::MinimumComputerScreenSize )
	{
		size = m_master.userConfig().monitoringScreenSize();
	}

	ui->gridSizeSlider->setValue( size );
	ui->computerMonitoringWidget->setComputerScreenSize( size );

	// initialize computer placement controls
	auto customComputerPositionsControlMenu = new QMenu(this);
	const auto darkSuffix = VeyonCore::useDarkMode() ? QStringLiteral("-dark") : QString();
	customComputerPositionsControlMenu->addAction(QIcon(QStringLiteral(":/core/document-open%1.png").arg(darkSuffix)),
													tr("Load computer positions"),
													this, &MainWindow::loadComputerPositions);
	customComputerPositionsControlMenu->addAction(QIcon(QStringLiteral(":/core/document-save%1.png").arg(darkSuffix)),
													tr("Save computer positions"),
													this, &MainWindow::saveComputerPositions);
	ui->useCustomComputerPositionsButton->setMenu(customComputerPositionsControlMenu);
	customComputerPositionsControlMenu->setAccessibleName( tr("Custom computer arrangement") );
	ui->useCustomComputerPositionsButton->setAccessibleName( tr("Use custom computer arrangement") );

	ui->useCustomComputerPositionsButton->setChecked(m_master.userConfig().useCustomComputerPositions());
	connect(ui->useCustomComputerPositionsButton, &QToolButton::toggled,
			ui->computerMonitoringWidget, &ComputerMonitoringWidget::setUseCustomComputerPositions);
	connect(ui->alignComputersButton, &QToolButton::clicked,
			ui->computerMonitoringWidget, &ComputerMonitoringWidget::alignComputers);
	ui->alignComputersButton->setAccessibleName( tr("Align computers to grid") );


	const auto toolButtons = findChildren<QToolButton*>();
	for(auto* btn : toolButtons)
	{
		btn->setIconSize(QSize(20, 20));
	}

	if (VeyonCore::useDarkMode())
	{
		ui->aboutButton->setIcon(QIcon(QStringLiteral(":/core/help-about-dark.png")));
		ui->filterComputersWithLoggedOnUsersButton->setIcon(QIcon(QStringLiteral(":/core/user-group-new-dark.png")));
		ui->autoAdjustComputerIconSizeButton->setIcon(QIcon(QStringLiteral(":/master/zoom-fit-best-dark.png")));
		ui->alignComputersButton->setIcon(QIcon(QStringLiteral(":/master/align-grid-dark.png")));
		ui->useCustomComputerPositionsButton->setIcon(QIcon(QStringLiteral(":/master/exchange-positions-zorder-dark.png")));
		ui->filterPoweredOnComputersButton->setIcon(QIcon(QStringLiteral(":/master/powered-on-dark.png")));
	}

	// create the main toolbar
	ui->toolBar->layout()->setSpacing( 8 );
	ui->toolBar->toggleViewAction()->setEnabled( false );

	addToolBar( Qt::TopToolBarArea, ui->toolBar );

	addFeaturesToToolBar();
	reloadSubFeatures();

	// NextSpace layout: About sits at the far end of the toolbar
	auto toolBarSpacer = new QWidget( ui->toolBar );
	toolBarSpacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	ui->toolBar->addWidget( toolBarSpacer );
	ui->aboutButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	ui->toolBar->addWidget( ui->aboutButton );

	const auto monitoringModeButton = m_modeGroup->button(int(qHash(VeyonCore::builtinFeatures().monitoringMode().feature().uid()))); // clazy:exclude=qt6-qhash-signature
	if (monitoringModeButton)
	{
		monitoringModeButton->setChecked(true);
	}
}



MainWindow::~MainWindow()
{
	ui->computerMonitoringWidget->saveConfiguration();

	delete ui;
}



bool MainWindow::initAuthentication()
{
	if( VeyonCore::instance()->initAuthentication() )
	{
		return true;
	}

	if( VeyonCore::config().authenticationMethod() == VeyonCore::AuthenticationMethod::KeyFileAuthentication )
	{
		QMessageBox::information(nullptr,
								 tr("Authentication impossible"),
								 tr("No authentication key files were found or your current ones "
									"are outdated. Please create new key files using "
									"NextSpace Classroom Administration. Alternatively set up logon authentication "
									"using NextSpace Classroom Administration. Otherwise you won't be "
									"able to access computers using NextSpace Teacher Console."));
	}

	return false;
}



bool MainWindow::initAccessControl()
{
	if (VeyonCore::config().accessControlForMasterEnabled())
	{
		const auto accessingUser = VeyonCore::authenticationCredentials().hasCredentials(AuthenticationCredentials::Type::UserLogon) ?
									   VeyonCore::authenticationCredentials().logonUsername() :
									   VeyonCore::platform().userFunctions().queryCurrentUserProperty(PlatformUserFunctions::UserProperty::LoginName);
		const auto accessControlResult = VeyonCore::builtinFeatures().accessControlProvider()
										 .checkAccess(accessingUser,
													  QHostAddress(QHostAddress::LocalHost).toString(),
													  {});
		if( accessControlResult.access == AccessControlProvider::Access::Deny )
		{
			vWarning() << "user" << VeyonCore::authenticationCredentials().logonUsername()
					   << "is not allowed to access computers";
			QMessageBox::critical( nullptr, tr( "Access denied" ),
								   tr( "According to the local configuration you're not allowed "
									   "to access computers in the network. Please log in with a different "
									   "account or let your system administrator check the local configuration." ) );
			return false;
		}
	}

	return true;
}



void MainWindow::reloadSubFeatures()
{
	for( const auto& feature : m_master.features() )
	{
		auto button = ui->toolBar->findChild<QToolButton *>( feature.name() );
		if( button )
		{
			addSubFeaturesToToolButton( button, feature );
		}
	}

	reloadMoreMenu();
}



ComputerControlInterfaceList MainWindow::selectedComputerControlInterfaces() const
{
	return ui->computerMonitoringWidget->selectedComputerControlInterfaces();
}



void MainWindow::closeEvent( QCloseEvent* event )
{
	if( m_master.currentMode() != VeyonCore::builtinFeatures().monitoringMode().feature().uid() )
	{
		const Feature& activeFeature = VeyonCore::featureManager().feature( m_master.currentMode() );

		QMessageBox::information(this, tr("Feature active"),
								 tr("The feature \"%1\" is still active. Please stop it before closing NextSpace Teacher Console.")
								 .arg(activeFeature.displayName()));
		event->ignore();
		return;
	}

	QJsonObject splitterStates;
	const auto splitters = findChildren<QSplitter *>();
	for( const auto* splitter : splitters )
	{
		const auto sizes = splitter->sizes();
		QJsonArray splitterSizes;
		int i = 0;
		int hiddenSize = 0;
		for( auto size : sizes )
		{
			auto widget = splitter->widget(i);
			const auto originalSize = widget->property( originalSizePropertyName() ).toSize();
			if( widget->size().isEmpty() && originalSize.isEmpty() == false )
			{
				size = splitter->orientation() == Qt::Horizontal ? -originalSize.width() : -originalSize.height();
				hiddenSize += qAbs(size);
			}
			else
			{
				size -= hiddenSize;
			}

			splitterSizes.append( size );
			++i;
		}
		splitterStates[splitter->objectName()] = splitterSizes;
	}

	m_master.userConfig().setSplitterStates( splitterStates );

	m_master.userConfig().setWindowState( QString::fromLatin1( saveState().toBase64() ) );
	m_master.userConfig().setWindowGeometry( QString::fromLatin1( saveGeometry().toBase64() ) );

	QMainWindow::closeEvent( event );
}



bool MainWindow::eventFilter( QObject* object, QEvent* event )
{
	if( event->type() == QEvent::Resize )
	{
		const auto widget = qobject_cast<QWidget *>( object );
		const auto resizeEvent = static_cast<QResizeEvent *>( event );

		if( resizeEvent->oldSize().isEmpty() == false )
		{
			widget->setProperty( originalSizePropertyName(), resizeEvent->oldSize() );
		}
	}

	return QMainWindow::eventFilter( object, event );
}



void MainWindow::keyPressEvent( QKeyEvent* event )
{
	switch( event->key() )
	{
	case Qt::Key_F5:
		VeyonCore::networkObjectDirectoryManager().configuredDirectory()->update();
		m_master.computerControlListModel().reload();
		event->accept();
		break;
	case Qt::Key_F11:
		QWidget::setWindowState( QWidget::windowState() ^ Qt::WindowFullScreen );
		event->accept();
		break;
	default:
		QMainWindow::keyPressEvent( event );
		break;
	}
}



void MainWindow::showAboutDialog()
{
	AboutDialog( this ).exec();
}



bool MainWindow::isMoreMenuFeature( const Feature& feature )
{
	// mode features always stay on the toolbar so their checked state, stop/unlock label and
	// mode sub-features remain visible
	return feature.name().isEmpty() == false &&
		   feature.testFlag( Feature::Flag::Mode ) == false &&
		   moreMenuLayout().contains( feature.name() );
}



QString MainWindow::toolBarLabel( const Feature& feature ) const
{
	const QHash<QString, QString> labels{
		{ QStringLiteral("MonitoringMode"), tr("Monitor") },
		{ QStringLiteral("Demo"), tr("Broadcast") },
		{ QStringLiteral("RemoteControl"), tr("Remote Control") },
		{ QStringLiteral("TextMessage"), tr("Message") },
		{ QStringLiteral("DistributeFiles"), tr("Send Files") },
		{ QStringLiteral("RemoteView"), tr("Remote View") },
		{ QStringLiteral("FileCollect"), tr("Collect Files") },
		{ QStringLiteral("StartApp"), tr("Start Application") },
		{ QStringLiteral("OpenWebsite"), tr("Open Website") },
		{ QStringLiteral("PowerOn"), tr("Power On") },
		{ QStringLiteral("PowerDown"), tr("Power Down") },
		{ QStringLiteral("UserLogin"), tr("Log In") },
		{ QStringLiteral("UserLogoff"), tr("Log Off") },
	};

	return labels.value( feature.name(), feature.displayName() );
}



QString MainWindow::toolBarActiveLabel( const Feature& feature ) const
{
	const QHash<QString, QString> labels{
		{ QStringLiteral("MonitoringMode"), tr("Monitor") },
		{ QStringLiteral("Demo"), tr("Stop Broadcast") },
	};

	return labels.value( feature.name(), feature.displayNameActive() );
}



void MainWindow::addFeaturesToToolBar()
{
	for( const auto& feature : m_master.features() )
	{
		if( feature.testFlag( Feature::Flag::Meta ) || isMoreMenuFeature( feature ) )
		{
			continue;
		}

		auto btn = new ToolButton( QIcon( feature.iconUrl() ),
										  toolBarLabel( feature ),
										  toolBarActiveLabel( feature ),
										  feature.description(),
										  feature.shortcut() );
		connect(btn, &QToolButton::clicked, this, [=, this] () {
			m_master.runFeature( feature );
			updateModeButtonGroup();
			if( feature.testFlag( Feature::Flag::Mode ) )
			{
				reloadSubFeatures();
			}
		});
		btn->setObjectName( feature.name() );
		btn->addTo( ui->toolBar );

		if( feature.testFlag( Feature::Flag::Mode ) )
		{
			btn->setCheckable( true );
			m_modeGroup->addButton( btn, buttonId( feature ) );
		}
	}

	m_moreButton = new ToolButton( moreActionsIcon( palette().color( QPalette::WindowText ) ),
								   tr("More"), {}, tr("More classroom actions") );
	m_moreButton->setObjectName( QStringLiteral("MoreActionsButton") );
	m_moreButton->addTo( ui->toolBar );
}



void MainWindow::reloadMoreMenu()
{
	if( m_moreButton == nullptr )
	{
		return;
	}

	if( auto oldMenu = m_moreButton->menu() )
	{
		oldMenu->close();
		// plugins look up sub-feature menus by name, so don't let them find menus pending deletion
		const auto oldSubMenus = oldMenu->findChildren<QMenu *>();
		for( auto* subMenu : oldSubMenus )
		{
			subMenu->setObjectName( {} );
		}
		oldMenu->deleteLater();
		m_moreButton->setMenu( nullptr );
	}

	auto menu = new QMenu( m_moreButton );
	menu->setToolTipsVisible( true );
	menu->setAccessibleName( tr("More classroom actions") );

	const auto& features = m_master.features();
	bool separatorPending = false;

	for( const auto& featureName : moreMenuLayout() )
	{
		if( featureName.isEmpty() )
		{
			separatorPending = menu->isEmpty() == false;
			continue;
		}

		const auto it = std::find_if( features.cbegin(), features.cend(),
									  [&featureName]( const Feature& f ) { return f.name() == featureName; } );
		if( it == features.cend() || it->testFlag( Feature::Flag::Meta ) || isMoreMenuFeature( *it ) == false )
		{
			continue;
		}

		const auto feature = *it;

		if( separatorPending )
		{
			menu->addSeparator();
			separatorPending = false;
		}

		// same behavior as a toolbar button: features with sub-features only open their menu
		QAction* action = nullptr;
		if( const auto subFeatures = m_master.subFeatures( feature.uid() ); subFeatures.isEmpty() )
		{
			action = menu->addAction( QIcon( feature.iconUrl() ), toolBarLabel( feature ), this, [=, this]() {
				m_master.runFeature( feature );
				updateModeButtonGroup();
			} );
			action->setShortcut( feature.shortcut() );
			// keep a shortcut usable while the menu is closed
			addAction( action );
		}
		else
		{
			auto subMenu = menu->addMenu( QIcon( feature.iconUrl() ), toolBarLabel( feature ) );
			subMenu->setObjectName( feature.name() );
			subMenu->setToolTipsVisible( true );
			subMenu->setAccessibleName( toolBarLabel( feature ) );
			addSubFeatureActions( subMenu, feature, subFeatures, nullptr );
			action = subMenu->menuAction();
		}
		action->setToolTip( feature.description() );
	}

	m_moreButton->setEnabled( menu->isEmpty() == false );
	m_moreButton->setMenu( menu );
	m_moreButton->setPopupMode( ToolButton::InstantPopup );
}



void MainWindow::addSubFeaturesToToolButton( QToolButton* button, const Feature& parentFeature )
{
	if( button->menu() )
	{
		button->menu()->close();
		button->menu()->deleteLater();
		button->setMenu( nullptr );
	}

	const auto parentFeatureIsMode = parentFeature.testFlag( Feature::Flag::Mode );
	const auto subFeatures = m_master.subFeatures( parentFeature.uid() );

	if( subFeatures.isEmpty() ||
		( parentFeatureIsMode && button->isChecked() ) )
	{
		return;
	}

	auto menu = new QMenu( button );
	menu->setObjectName( parentFeature.name() );
	menu->setToolTipsVisible( true );
	// NextSpace accessibility: popups are announced by their displayed feature label, not the application name
	menu->setAccessibleName( toolBarLabel( parentFeature ) );

	addSubFeatureActions( menu, parentFeature, subFeatures, button );

	button->setMenu( menu );
	button->setPopupMode( ToolButton::InstantPopup );
}



void MainWindow::addSubFeatureActions( QMenu* menu, const Feature& parentFeature, const FeatureList& subFeatures,
									   QToolButton* modeButton )
{
	const auto parentFeatureIsMode = parentFeature.testFlag( Feature::Flag::Mode ) && modeButton;

	for( const auto& subFeature : subFeatures )
	{
		auto action = menu->addAction(QIcon(subFeature.iconUrl()), subFeature.displayName(),
							   #if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
									  subFeature.shortcut(),
							   #endif
									  this, [=, this]()
		{
			m_master.runFeature( subFeature );
			if( parentFeatureIsMode )
			{
				if( subFeature.testFlag( Feature::Flag::Option ) == false )
				{
					modeButton->setChecked( true );
				}
				reloadSubFeatures();
			}
		}
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
		,subFeature.shortcut()
#endif
		);
		action->setToolTip( subFeature.description() );
		action->setObjectName( subFeature.uid().toString() );

		if( subFeature.testFlag( Feature::Flag::Option ) )
		{
			action->setCheckable( true );
			action->setChecked( subFeature.testFlag( Feature::Flag::Checked ) );
		}
	}
}



void MainWindow::updateModeButtonGroup()
{
	const auto& monitoringMode = VeyonCore::builtinFeatures().monitoringMode().feature();

	if( m_master.currentMode() == monitoringMode.uid() )
	{
		m_modeGroup->button( buttonId( monitoringMode ) )->setChecked( true );
	}
}



void MainWindow::loadComputerPositions()
{
	if (const auto fileName = QFileDialog::getOpenFileName(this, tr("Load computer positions"),
														   QDir::homePath(), tr("JSON files (*.json)"));
		!fileName.isEmpty())
	{
		if (QFile file(fileName); file.open(QFile::ReadOnly))
		{
			const auto& computerPositionsProperty = m_master.userConfig().computerPositionsProperty();
			const auto config = QJsonDocument::fromJson(file.readAll()).object();
			const auto uiConfig = config[computerPositionsProperty.parentKey()].toObject();
			const auto computerPositions = uiConfig[computerPositionsProperty.key()].toObject()[QStringLiteral("JsonStoreArray")].toArray();

			ui->computerMonitoringWidget->loadPositions(computerPositions);
			ui->computerMonitoringWidget->setUseCustomComputerPositions(true);
			ui->computerMonitoringWidget->doItemsLayout();
		}
	}
}



void MainWindow::saveComputerPositions()
{
	if (const auto fileName = QFileDialog::getSaveFileName(this, tr("Save computer positions"),
														   QDir::homePath(), tr("JSON files (*.json)"));
		!fileName.isEmpty())
	{
		if (QFile file(fileName); file.open(QFile::WriteOnly | QFile::Truncate))
		{
			const auto& computerPositionsProperty = m_master.userConfig().computerPositionsProperty();

			// create structure identical to UserConfig so file with positions can be used as template for UserConfig
			QJsonObject computerPositions;
			computerPositions[QStringLiteral("JsonStoreArray")] = ui->computerMonitoringWidget->savePositions();
			QJsonObject uiConfig;
			uiConfig[computerPositionsProperty.key()] = computerPositions;
			QJsonObject config;
			config[computerPositionsProperty.parentKey()] = uiConfig;
			file.write(QJsonDocument(config).toJson());
		}
	}
}
