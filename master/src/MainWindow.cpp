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

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFrame>
#include <QIcon>
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
#include <QSignalBlocker>
#include <QToolButton>
#include <QStackedLayout>
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
#include "NextSpaceHelp.h"
#include "PlatformUserFunctions.h"
#include "SlideshowPanel.h"
#include "SpotlightPanel.h"
#include "TeacherActionIcons.h"
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

constexpr int IdentityHeaderHeight = 80;
constexpr int IdentityIconSize = 44;
constexpr int IdentityTitleSize = 18;
constexpr int IdentitySubtitleSize = 12;
constexpr int IdentityHorizontalMargin = 24;
constexpr int IdentityVerticalMargin = 16;
constexpr int IdentityGap = 12;
constexpr int IdentityTextGap = 3;
constexpr int IdentityButtonMinimumHeight = 36;

struct ClassroomContextSnapshot
{
	QString location{};
	int total{0};
	int connected{0};
};

ClassroomContextSnapshot classroomContextSnapshot( const ComputerControlListModel& sourceModel )
{
	ClassroomContextSnapshot context;
	context.total = sourceModel.rowCount();
	bool singleLocation = context.total > 0;

	for( int row = 0; row < context.total; ++row )
	{
		const auto index = sourceModel.index( row, 0 );
		const auto controlInterface = sourceModel.data(
			index, ComputerControlListModel::ControlInterfaceRole ).value<ComputerControlInterface::Pointer>();
		const auto rowLocation = controlInterface.isNull()
			? QString{}
			: controlInterface->computer().location().trimmed();

		if( rowLocation.isEmpty() )
		{
			singleLocation = false;
		}
		else if( context.location.isEmpty() )
		{
			context.location = rowLocation;
		}
		else if( context.location != rowLocation )
		{
			singleLocation = false;
		}

		if( sourceModel.data( index, ComputerControlListModel::StateRole )
				.value<ComputerControlInterface::State>() == ComputerControlInterface::State::Connected )
		{
			++context.connected;
		}
	}

	if( singleLocation == false )
	{
		context.location.clear();
	}

	return context;
}

QWidget* createTeacherIdentityHeader( QToolButton* helpButton, QToolButton* aboutButton, QWidget* parent )
{
	auto header = new QWidget( parent );
	header->setObjectName( QStringLiteral("TeacherIdentityHeader") );
	header->setBackgroundRole( QPalette::Window );
	header->setAutoFillBackground( true );
	header->setFixedHeight( IdentityHeaderHeight );

	auto headerLayout = new QVBoxLayout( header );
	headerLayout->setContentsMargins( 0, 0, 0, 0 );
	headerLayout->setSpacing( 0 );

	auto identityRow = new QWidget( header );
	auto identityRowLayout = new QHBoxLayout( identityRow );
	identityRowLayout->setContentsMargins( IdentityHorizontalMargin, IdentityVerticalMargin,
										 IdentityHorizontalMargin, IdentityVerticalMargin );
	identityRowLayout->setSpacing( IdentityGap );

	auto iconLabel = new QLabel( identityRow );
	iconLabel->setFixedSize( IdentityIconSize, IdentityIconSize );
	iconLabel->setPixmap( QIcon( QStringLiteral(":/master/branding/nextspace-teacher-console-64.png") )
						  .pixmap( QSize( IdentityIconSize, IdentityIconSize ), header->devicePixelRatioF() ) );
	iconLabel->setAlignment( Qt::AlignCenter );

	auto identityText = new QWidget( identityRow );
	auto identityTextLayout = new QVBoxLayout( identityText );
	identityTextLayout->setContentsMargins( 0, 0, 0, 0 );
	identityTextLayout->setSpacing( IdentityTextGap );
	identityTextLayout->addStretch( 1 );

	auto titleLabel = new QLabel( QStringLiteral("NextSpace Teacher Console"), identityText );
	auto titleFont = titleLabel->font();
	titleFont.setPixelSize( IdentityTitleSize );
	titleFont.setWeight( QFont::DemiBold );
	titleLabel->setFont( titleFont );
	identityTextLayout->addWidget( titleLabel );

	auto subtitleLabel = new QLabel( QStringLiteral("Classroom Management"), identityText );
	auto subtitleFont = subtitleLabel->font();
	subtitleFont.setPixelSize( IdentitySubtitleSize );
	subtitleFont.setWeight( QFont::Normal );
	subtitleLabel->setFont( subtitleFont );
	subtitleLabel->setForegroundRole( QPalette::PlaceholderText );
	identityTextLayout->addWidget( subtitleLabel );
	identityTextLayout->addStretch( 1 );

	for( auto* button : { helpButton, aboutButton } )
	{
		button->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		button->setMinimumHeight( IdentityButtonMinimumHeight );
	}

	identityRowLayout->addWidget( iconLabel );
	identityRowLayout->addWidget( identityText );
	identityRowLayout->addStretch( 1 );
	identityRowLayout->addWidget( helpButton );
	identityRowLayout->addWidget( aboutButton );
	headerLayout->addWidget( identityRow, 1 );

	auto separator = new QFrame( header );
	separator->setObjectName( QStringLiteral("TeacherIdentityHeaderSeparator") );
	separator->setFrameShape( QFrame::HLine );
	separator->setFrameShadow( QFrame::Plain );
	separator->setForegroundRole( QPalette::Mid );
	separator->setFixedHeight( 1 );
	headerLayout->addWidget( separator );

	return header;
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
	auto identityHeader = createTeacherIdentityHeader( ui->helpButton, ui->aboutButton, ui->centralWidget );

	// add panel toggles to status bar
	ui->statusBar->addWidget( ui->panelButtons );
	static constexpr int SecondaryNavigationHeight = 32;
	static constexpr int SecondaryNavigationGap = 8;
	ui->panelButtons->layout()->setSpacing( SecondaryNavigationGap );
	for( auto* button : { ui->computerSelectPanelButton, ui->screenshotManagementPanelButton,
						 ui->slideshowPanelButton, ui->spotlightPanelButton } )
	{
		button->setMinimumHeight( SecondaryNavigationHeight );
		button->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	}
	// NextSpace accessibility: the Screenshots view-strip toggle takes keyboard focus like the other three
	ui->screenshotManagementPanelButton->setFocusPolicy( Qt::StrongFocus );

	// NextSpace layout: search, filters and view controls sit directly above the computer grid
	static constexpr int FilterControlHeight = 36;
	static constexpr int FilterSurfaceInset = 12;
	static constexpr int FilterSurfaceGap = 8;
	static constexpr int FilterSurfaceRadius = 10;
	static constexpr int ControlMinimumWidth = 160;
	static constexpr int MonitoringRowsGap = 14;
	auto monitoringArea = new QWidget;
	monitoringArea->setObjectName( QStringLiteral("MonitoringArea") );
	auto monitoringAreaLayout = new QVBoxLayout( monitoringArea );
	monitoringAreaLayout->setContentsMargins( 0, 0, 0, 0 );
	monitoringAreaLayout->setSpacing( 0 );
	auto monitoringFilterArea = new QWidget( monitoringArea );
	monitoringFilterArea->setObjectName( QStringLiteral("MonitoringFilterArea") );
	auto monitoringFilterLayout = new QHBoxLayout( monitoringFilterArea );
	monitoringFilterLayout->setContentsMargins( FilterSurfaceInset, FilterSurfaceInset,
										 FilterSurfaceInset, FilterSurfaceInset );
	monitoringFilterLayout->setSpacing( FilterSurfaceGap );
	ui->filterLineEdit->setPlaceholderText( tr("Find a computer or user…") );
	ui->filterLineEdit->setMinimumWidth( ControlMinimumWidth );
	ui->filterLineEdit->setMinimumHeight( FilterControlHeight );
	auto searchSizePolicy = ui->filterLineEdit->sizePolicy();
	searchSizePolicy.setHorizontalPolicy( QSizePolicy::Expanding );
	ui->filterLineEdit->setSizePolicy( searchSizePolicy );
	ui->filterPoweredOnComputersButton->setText( tr("Powered on only") );
	ui->filterComputersWithLoggedOnUsersButton->setText( tr("Logged-in users only") );
	ui->filterPoweredOnComputersButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	ui->filterComputersWithLoggedOnUsersButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	monitoringFilterLayout->addWidget( ui->filterLineEdit, 1 );
	monitoringFilterLayout->addWidget( ui->filterPoweredOnComputersButton );
	monitoringFilterLayout->addWidget( ui->filterComputersWithLoggedOnUsersButton );
	monitoringArea->setStyleSheet( QStringLiteral(
		"QWidget#MonitoringFilterArea { background-color: palette(base); border: 1px solid palette(mid); "
		"border-radius: %1px; }"
		"QWidget#MonitoringFilterArea QLineEdit { background-color: palette(base); color: palette(text); "
		"border: 1px solid palette(mid); border-radius: 8px; padding: 0 12px; }"
		"QWidget#MonitoringFilterArea QLineEdit:focus { border: 2px solid palette(link); }"
		"QWidget#MonitoringFilterArea QToolButton { background-color: palette(button); color: palette(button-text); "
		"border: 1px solid transparent; border-radius: 8px; padding: 0 12px; }"
		"QWidget#MonitoringFilterArea QToolButton:hover { background-color: palette(alternate-base); "
		"border-color: palette(mid); }"
		"QWidget#MonitoringFilterArea QToolButton:checked { background-color: palette(highlight); "
		"color: palette(highlighted-text); }"
		"QWidget#MonitoringFilterArea QToolButton:focus { border: 2px solid palette(link); }"
		"QWidget#MonitoringViewControlsArea QToolButton { background-color: palette(button); "
		"color: palette(button-text); border: 1px solid transparent; border-radius: 8px; padding: 0 10px; }"
		"QWidget#MonitoringViewControlsArea QToolButton:hover { background-color: palette(alternate-base); "
		"border-color: palette(mid); }"
		"QWidget#MonitoringViewControlsArea QToolButton:checked { background-color: palette(highlight); "
		"color: palette(highlighted-text); }"
		"QWidget#MonitoringViewControlsArea QToolButton:focus { border: 2px solid palette(link); }"
		"QWidget#MonitoringViewControlsArea QToolButton:disabled { color: palette(mid); "
		"background-color: palette(base); }" )
		.arg( FilterSurfaceRadius ) );

	auto monitoringViewControlsArea = new QWidget( monitoringArea );
	monitoringViewControlsArea->setObjectName( QStringLiteral("MonitoringViewControlsArea") );
	auto monitoringViewControlsLayout = new QHBoxLayout( monitoringViewControlsArea );
	monitoringViewControlsLayout->setContentsMargins( 0, 0, 0, 0 );
	monitoringViewControlsLayout->setSpacing( FilterSurfaceGap );

	// selection summary: describes, but does not change, which computers actions apply to
	auto selectionSummaryLabel = new QLabel;
	selectionSummaryLabel->setObjectName( QStringLiteral("SelectionSummaryLabel") );
	selectionSummaryLabel->setToolTip( tr("Toolbar and More actions apply to all computers shown. "
										  "Remote View and Remote Control open the selected computers, or ask for an address "
										  "if none is selected. Broadcasting a student's screen needs exactly one selected computer. "
										  "Right-click menu actions apply to the selected computers, or to ALL computers shown "
										  "when none is selected (including power actions).") );
	const auto updateSelectionSummary = [this, selectionSummaryLabel]() {
		selectionSummaryLabel->setText( tr("%1 selected")
											.arg( ui->computerMonitoringWidget->selectionModel()->selectedIndexes().size() ) );
	};
	const auto monitoringModel = ui->computerMonitoringWidget->model();
	const auto sourceModel = &m_master.computerControlListModel();
	connect( ui->computerMonitoringWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateSelectionSummary );
	updateSelectionSummary();
	auto selectVisibleCheckBox = new QCheckBox( tr("Select visible"), monitoringViewControlsArea );
	selectVisibleCheckBox->setObjectName( QStringLiteral("SelectVisibleCheckBox") );
	selectVisibleCheckBox->setTristate( true );
	selectVisibleCheckBox->setMinimumHeight( FilterControlHeight );
	selectVisibleCheckBox->setToolTip( tr("Select or clear all computers currently shown.") );
	selectVisibleCheckBox->setAccessibleName( tr("Select visible computers") );
	const auto updateSelectVisible = [selectVisibleCheckBox, monitoringModel,
									 uiSelectionModel = ui->computerMonitoringWidget->selectionModel()]() {
		const auto visibleCount = monitoringModel->rowCount();
		int selectedVisibleCount = 0;
		for( int row = 0; row < visibleCount; ++row )
		{
			selectedVisibleCount += uiSelectionModel->isSelected( monitoringModel->index( row, 0 ) ) ? 1 : 0;
		}

		const QSignalBlocker blocker( selectVisibleCheckBox );
		selectVisibleCheckBox->setEnabled( visibleCount > 0 );
		selectVisibleCheckBox->setCheckState(
			visibleCount == 0 || selectedVisibleCount == 0 ? Qt::Unchecked :
			selectedVisibleCount == visibleCount ? Qt::Checked : Qt::PartiallyChecked );
	};
	connect( selectVisibleCheckBox, &QCheckBox::clicked, this,
		[monitoringModel, uiSelectionModel = ui->computerMonitoringWidget->selectionModel()]( bool checked ) {
			const auto visibleCount = monitoringModel->rowCount();
			if( visibleCount > 0 )
			{
				const QItemSelection visibleRows( monitoringModel->index( 0, 0 ),
										  monitoringModel->index( visibleCount - 1, 0 ) );
				uiSelectionModel->select( visibleRows, checked ? QItemSelectionModel::Select : QItemSelectionModel::Deselect );
			}
		} );
	connect( ui->computerMonitoringWidget->selectionModel(), &QItemSelectionModel::selectionChanged,
			 this, updateSelectVisible );
	for( const auto signal : { &QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved } )
	{
		connect( monitoringModel, signal, this, updateSelectVisible );
	}
	connect( monitoringModel, &QAbstractItemModel::modelReset, this, updateSelectVisible );
	connect( monitoringModel, &QAbstractItemModel::layoutChanged, this, updateSelectVisible );
	updateSelectVisible();
	monitoringViewControlsLayout->addWidget( selectVisibleCheckBox );
	monitoringViewControlsLayout->addStretch( 1 );
	auto thumbnailSizeLabel = new QLabel( tr("Thumbnail size"), monitoringViewControlsArea );
	thumbnailSizeLabel->setObjectName( QStringLiteral("ThumbnailSizeLabel") );
	thumbnailSizeLabel->setBuddy( ui->gridSizeSlider );
	ui->gridSizeSlider->setMinimumWidth( ControlMinimumWidth );
	ui->autoAdjustComputerIconSizeButton->setText( tr("Auto") );
	ui->useCustomComputerPositionsButton->setText( tr("Custom positions") );
	ui->alignComputersButton->setText( tr("Align") );
	ui->autoAdjustComputerIconSizeButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	ui->useCustomComputerPositionsButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	ui->alignComputersButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
	monitoringViewControlsLayout->addWidget( thumbnailSizeLabel );
	monitoringViewControlsLayout->addWidget( ui->gridSizeSlider );
	monitoringViewControlsLayout->addWidget( ui->autoAdjustComputerIconSizeButton );
	monitoringViewControlsLayout->addWidget( ui->useCustomComputerPositionsButton );
	monitoringViewControlsLayout->addWidget( ui->alignComputersButton );

	for( auto* button : { ui->filterPoweredOnComputersButton, ui->filterComputersWithLoggedOnUsersButton,
						  ui->autoAdjustComputerIconSizeButton, ui->useCustomComputerPositionsButton,
						  ui->alignComputersButton } )
	{
		button->setMinimumSize( FilterControlHeight, FilterControlHeight );
	}

	ui->centralLayout->removeWidget( ui->computerMonitoringWidget );
	ui->centralLayout->setDirection( QBoxLayout::TopToBottom );
	ui->centralLayout->addWidget( identityHeader );
	monitoringAreaLayout->addWidget( monitoringFilterArea );
	monitoringAreaLayout->addSpacing( MonitoringRowsGap );
	monitoringAreaLayout->addWidget( monitoringViewControlsArea );
	auto monitoringGridArea = new QWidget( monitoringArea );
	monitoringGridArea->setObjectName( QStringLiteral("MonitoringGridArea") );
	auto monitoringGridAreaLayout = new QStackedLayout( monitoringGridArea );
	monitoringGridAreaLayout->setContentsMargins( 0, 0, 0, 0 );
	monitoringGridAreaLayout->setSpacing( 0 );
	monitoringGridAreaLayout->setStackingMode( QStackedLayout::StackAll );
	monitoringGridAreaLayout->addWidget( ui->computerMonitoringWidget );
	monitoringAreaLayout->addWidget( monitoringGridArea, 1 );

	// NextSpace empty state: a non-interactive overlay inside the unchanged monitoring view, shown only while the
	// view's filtered model has no rows; the view keeps its focus, selection, drop and context-menu behavior
	auto emptyState = new QWidget( monitoringGridArea );
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

	monitoringGridAreaLayout->addWidget( emptyState );

	const auto updateEmptyState = [emptyState, emptyStateHeading, emptyStateHelper, monitoringGridAreaLayout,
								   monitoringModel, sourceModel, this]() {
		const auto filteredEmpty = monitoringModel->rowCount() == 0;
		if( sourceModel->rowCount() == 0 )
		{
			emptyStateHeading->setText( tr("No computers to display") );
			emptyStateHelper->setText( tr("Review your existing locations and filters.") );
		}
		else
		{
			emptyStateHeading->setText( tr("No matching computers") );
			emptyStateHelper->setText( tr("Clear the search or active filters.") );
		}
		emptyState->setVisible( filteredEmpty );
		monitoringGridAreaLayout->setCurrentWidget( filteredEmpty ? emptyState : ui->computerMonitoringWidget );
	};
	for( const auto signal : { &QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved } )
	{
		connect( sourceModel, signal, this, updateEmptyState );
		connect( monitoringModel, signal, this, updateEmptyState );
	}
	connect( sourceModel, &QAbstractItemModel::modelReset, this, updateEmptyState );
	connect( sourceModel, &QAbstractItemModel::layoutChanged, this, updateEmptyState );
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
	auto teacherWorkspace = new QWidget( mainSplitter );
	teacherWorkspace->setObjectName( QStringLiteral("TeacherWorkspace") );
	auto teacherWorkspaceLayout = new QVBoxLayout( teacherWorkspace );
	static constexpr int ClassroomOuterMargin = 24;
	static constexpr int ClassroomHeadingSize = 24;
	static constexpr int ClassroomSummarySize = 12;
	static constexpr int ClassroomHeadingGap = 8;
	static constexpr int ClassroomActionsTopGap = 20;
	static constexpr int ClassroomActionsBottomGap = 16;
	teacherWorkspaceLayout->setContentsMargins( ClassroomOuterMargin, ClassroomOuterMargin,
											ClassroomOuterMargin, ClassroomOuterMargin );
	teacherWorkspaceLayout->setSpacing( 0 );
	auto classroomContextArea = new QWidget( teacherWorkspace );
	classroomContextArea->setObjectName( QStringLiteral("ClassroomContextArea") );
	classroomContextArea->setMaximumHeight( QWIDGETSIZE_MAX );
	classroomContextArea->show();
	auto classroomContextLayout = new QHBoxLayout( classroomContextArea );
	classroomContextLayout->setContentsMargins( 0, 0, 0, 0 );
	classroomContextLayout->setSpacing( 16 );
	auto classroomText = new QWidget( classroomContextArea );
	auto classroomTextLayout = new QVBoxLayout( classroomText );
	classroomTextLayout->setContentsMargins( 0, 0, 0, 0 );
	classroomTextLayout->setSpacing( ClassroomHeadingGap );
	auto classroomTitleLabel = new QLabel( classroomText );
	classroomTitleLabel->setObjectName( QStringLiteral("ClassroomContextTitleLabel") );
	classroomTitleLabel->setTextFormat( Qt::PlainText );
	classroomTitleLabel->setAccessibleDescription( tr("Classroom context") );
	auto classroomHeadingFont = classroomTitleLabel->font();
	classroomHeadingFont.setPixelSize( ClassroomHeadingSize );
	classroomHeadingFont.setWeight( QFont::DemiBold );
	classroomTitleLabel->setFont( classroomHeadingFont );
	auto classroomSummaryLabel = new QLabel( classroomText );
	classroomSummaryLabel->setObjectName( QStringLiteral("ClassroomContextSummaryLabel") );
	classroomSummaryLabel->setTextFormat( Qt::PlainText );
	classroomSummaryLabel->setAccessibleDescription( tr("Classroom counts") );
	classroomSummaryLabel->setForegroundRole( QPalette::PlaceholderText );
	auto classroomSummaryFont = classroomSummaryLabel->font();
	classroomSummaryFont.setPixelSize( ClassroomSummarySize );
	classroomSummaryLabel->setFont( classroomSummaryFont );
	classroomTextLayout->addWidget( classroomTitleLabel );
	classroomTextLayout->addWidget( classroomSummaryLabel );
	classroomContextLayout->addWidget( classroomText );
	classroomContextLayout->addStretch( 1 );
	auto selectionSummaryFont = selectionSummaryLabel->font();
	selectionSummaryFont.setPixelSize( ClassroomSummarySize );
	selectionSummaryLabel->setFont( selectionSummaryFont );
	selectionSummaryLabel->setForegroundRole( QPalette::PlaceholderText );
	selectionSummaryLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
	classroomContextLayout->addWidget( selectionSummaryLabel, 0, Qt::AlignRight | Qt::AlignVCenter );
	auto classroomActionArea = new QWidget( teacherWorkspace );
	classroomActionArea->setObjectName( QStringLiteral("ClassroomActionArea") );
	auto actionAreaLayout = new QVBoxLayout( classroomActionArea );
	actionAreaLayout->setContentsMargins( 0, 0, 0, 0 );
	actionAreaLayout->setSpacing( 0 );
	teacherWorkspaceLayout->addWidget( classroomContextArea );
	teacherWorkspaceLayout->addSpacing( ClassroomActionsTopGap );
	teacherWorkspaceLayout->addWidget( classroomActionArea );
	teacherWorkspaceLayout->addSpacing( ClassroomActionsBottomGap );
	teacherWorkspaceLayout->addWidget( monitoringSplitter, 1 );

	const auto updateClassroomContext = [this, sourceModel, monitoringModel,
										 classroomTitleLabel, classroomSummaryLabel]() {
		const auto context = classroomContextSnapshot( *sourceModel );
		classroomTitleLabel->setText( context.location.isEmpty()
			? tr("Classroom overview")
			: context.location );
		classroomSummaryLabel->setText( tr("%1 visible · %2 computers · %3 connected")
			.arg( monitoringModel->rowCount() )
			.arg( context.total )
			.arg( context.connected ) );
	};
	for( const auto signal : { &QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved } )
	{
		connect( sourceModel, signal, this, updateClassroomContext );
		connect( monitoringModel, signal, this, updateClassroomContext );
	}
	connect( sourceModel, &QAbstractItemModel::modelReset, this, updateClassroomContext );
	connect( sourceModel, &QAbstractItemModel::layoutChanged, this, updateClassroomContext );
	connect( sourceModel, &ComputerControlListModel::stateChanged, this, updateClassroomContext );
	connect( monitoringModel, &QAbstractItemModel::modelReset, this, updateClassroomContext );
	connect( monitoringModel, &QAbstractItemModel::layoutChanged, this, updateClassroomContext );
	updateClassroomContext();

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
	mainSplitter->addWidget( teacherWorkspace );

	mainSplitter->setStretchFactor( mainSplitter->indexOf(teacherWorkspace), 1 );


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
		if( splitterSizes.isEmpty() == false )
		{
			splitter->setSizes( splitterSizes );
		}
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
			 this, [this, updateSelectVisible, updateEmptyState, updateClassroomContext]( const QString& filter ) {
		ui->computerMonitoringWidget->setSearchFilter( filter );
		updateSelectVisible();
		updateEmptyState();
		updateClassroomContext();
	} );
	connect( ui->filterPoweredOnComputersButton, &QToolButton::toggled,
			 this, [this, updateSelectVisible, updateEmptyState, updateClassroomContext]( bool enabled ) {
		ui->computerMonitoringWidget->setFilterPoweredOnComputers( enabled );
		updateSelectVisible();
		updateEmptyState();
		updateClassroomContext();
	} );
	connect( ui->filterComputersWithLoggedOnUsersButton, &QToolButton::toggled,
			 this, [this, updateSelectVisible, updateEmptyState, updateClassroomContext]( bool enabled ) {
		ui->computerMonitoringWidget->setFilterComputersWithLoggedOnUsers( enabled );
		updateSelectVisible();
		updateEmptyState();
		updateClassroomContext();
	} );
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
	customComputerPositionsControlMenu->addAction(TeacherActionIcons::forViewControl(TeacherActionIcons::ViewControl::LoadComputerPositions, customComputerPositionsControlMenu->palette()),
													tr("Load computer positions"),
													this, &MainWindow::loadComputerPositions);
	customComputerPositionsControlMenu->addAction(TeacherActionIcons::forViewControl(TeacherActionIcons::ViewControl::SaveComputerPositions, customComputerPositionsControlMenu->palette()),
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

	// NextSpace icons: the view-strip toggles, grid controls and About use the Tabler family in each button's own
	// palette; a checked button keeps the same glyph and the style's checked state remains the state cue
	const auto setViewControlIcon = []( QToolButton* button, TeacherActionIcons::ViewControl control ) {
		button->setIcon( TeacherActionIcons::forViewControl( control, button->palette() ) );
	};
	setViewControlIcon( ui->computerSelectPanelButton, TeacherActionIcons::ViewControl::ComputerSelectPanel );
	setViewControlIcon( ui->screenshotManagementPanelButton, TeacherActionIcons::ViewControl::ScreenshotManagementPanel );
	setViewControlIcon( ui->slideshowPanelButton, TeacherActionIcons::ViewControl::SlideshowPanel );
	setViewControlIcon( ui->spotlightPanelButton, TeacherActionIcons::ViewControl::SpotlightPanel );
	setViewControlIcon( ui->filterPoweredOnComputersButton, TeacherActionIcons::ViewControl::PoweredOnFilter );
	setViewControlIcon( ui->filterComputersWithLoggedOnUsersButton, TeacherActionIcons::ViewControl::LoggedOnUsersFilter );
	setViewControlIcon( ui->autoAdjustComputerIconSizeButton, TeacherActionIcons::ViewControl::AutoAdjustIconSize );
	setViewControlIcon( ui->useCustomComputerPositionsButton, TeacherActionIcons::ViewControl::CustomComputerPositions );
	setViewControlIcon( ui->alignComputersButton, TeacherActionIcons::ViewControl::AlignComputers );
	setViewControlIcon( ui->aboutButton, TeacherActionIcons::ViewControl::About );
	setViewControlIcon( ui->helpButton, TeacherActionIcons::ViewControl::Help );

	// NextSpace offline help (GC3-S2): F1, visible text, tooltip and accessible name come from MainWindow.ui
	connect( ui->helpButton, &QToolButton::clicked, this, &MainWindow::showOfflineHelp );

	// create the main toolbar
	ui->toolBar->layout()->setSpacing( 8 );
	ui->toolBar->toggleViewAction()->setEnabled( false );
	removeToolBar( ui->toolBar );
	ui->toolBar->setEmbeddedWorkspaceMode();
	ui->toolBar->setMovable( false );
	ui->toolBar->setFloatable( false );
	actionAreaLayout->addWidget( ui->toolBar );
	ui->toolBar->show();

	addFeaturesToToolBar();
	static constexpr int ActionButtonHeight = 36;
	static constexpr int ActionIconSize = 20;
	static constexpr int ActionButtonRadius = 8;
	ui->toolBar->setIconSize( QSize( ActionIconSize, ActionIconSize ) );
	ui->toolBar->setStyleSheet( QStringLiteral(
		"QToolBar { background-color: palette(base); border: 0; }"
		"QToolButton { background-color: palette(button); color: palette(button-text); "
		"border: 1px solid transparent; border-radius: %1px; padding: 6px 10px; }"
		"QToolButton:hover { background-color: palette(alternate-base); border-color: palette(mid); }"
		"QToolButton:checked { background-color: palette(highlight); color: palette(highlighted-text); }"
		"QToolButton:focus { border: 2px solid palette(highlight); }" ).arg( ActionButtonRadius ) );
	const auto actionButtonStyle = ToolButton::iconOnlyMode() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon;
	for( auto* button : ui->toolBar->findChildren<ToolButton *>() )
	{
		button->setToolButtonStyle( actionButtonStyle );
		button->setMinimumHeight( ActionButtonHeight );
		button->setIconSize( QSize( ActionIconSize, ActionIconSize ) );
	}
	reloadSubFeatures();
	// NextSpace accessibility: when a mode button receives focus, Qt removes Tab focus from the other buttons
	// of the exclusive mode group; give it back right away so every enabled mode button stays in the Tab cycle.
	// The group is the context object, so the connection ends with the group, also during window teardown.
	connect( qApp, &QApplication::focusChanged, m_modeGroup, [group = m_modeGroup]( QWidget*, QWidget* now ) {
		if( group->buttons().contains( qobject_cast<QAbstractButton *>( now ) ) )
		{
			for( auto* button : group->buttons() )
			{
				if( button->isEnabled() )
				{
					button->setFocusPolicy( Qt::FocusPolicy( button->focusPolicy() | Qt::TabFocus ) );
				}
			}
		}
	} );

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



void MainWindow::showOfflineHelp()
{
	NextSpaceHelp::open( this );
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

		auto btn = new ToolButton( TeacherActionIcons::forFeature( feature, ui->toolBar->palette() ),
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

	m_moreButton = new ToolButton( TeacherActionIcons::more( ui->toolBar->palette() ),
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
			action = menu->addAction( TeacherActionIcons::forFeature( feature, menu->palette() ), toolBarLabel( feature ), this, [=, this]() {
				m_master.runFeature( feature );
				updateModeButtonGroup();
			} );
			action->setShortcut( feature.shortcut() );
			// keep a shortcut usable while the menu is closed
			addAction( action );
		}
		else
		{
			auto subMenu = menu->addMenu( TeacherActionIcons::forFeature( feature, menu->palette() ), toolBarLabel( feature ) );
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
		auto action = menu->addAction(TeacherActionIcons::forFeature(subFeature, menu->palette()), subFeature.displayName(),
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
