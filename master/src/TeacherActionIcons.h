/*
 * TeacherActionIcons.h - NextSpace Teacher Console action icons drawn from bundled Tabler path data
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

#pragma once

#include <QIcon>

class Feature;
class QPalette;

// Tabler Icons outline (v3.49.0) action icons for the Teacher toolbar, More menu, their submenus and the
// computer-tile feature menu. Icons are drawn in the given palette's text colours; features without a NextSpace
// mapping keep QIcon( feature.iconUrl() ).
namespace TeacherActionIcons
{

QIcon forFeature( const Feature& feature, const QPalette& palette );
QIcon more( const QPalette& palette );

// semantic roles of the Teacher view-strip toggles, grid filters and layout controls, the custom-positions menu
// and the About button (V3ao); each role draws one outline for both checked states
enum class ViewControl
{
	ComputerSelectPanel,
	ScreenshotManagementPanel,
	SlideshowPanel,
	SpotlightPanel,
	PoweredOnFilter,
	LoggedOnUsersFilter,
	AutoAdjustIconSize,
	CustomComputerPositions,
	AlignComputers,
	LoadComputerPositions,
	SaveComputerPositions,
	About
};

QIcon forViewControl( ViewControl control, const QPalette& palette );

// semantic roles of the text-bearing Screenshots-panel buttons (V3ap), the Slideshow-panel Previous/Next tool
// buttons (V3aq), the checkable Slideshow-panel start/pause tool button (V3ar), the checkable Slideshow-panel
// separate-window toggle (V3as), the Spotlight-panel Add/Remove tool buttons and checkable realtime toggle (V3at)
// and the Collect Files destination browse and Open output directory buttons (V3av);
// each role but SlideshowStartPause and SpotlightRealtime draws one outline for both states, SlideshowStartPause
// draws play when unchecked (Off) and pause when checked (On), SpotlightRealtime draws the crossed-out refresh-off
// when unchecked (Off) and refresh when checked (On), and the button labels, tooltips, the toggles' checked button
// styling and the existing delete confirmation remain the primary cues
enum class SecondaryControl
{
	ShowScreenshot,
	DeleteScreenshot,
	PreviousSlide,
	NextSlide,
	SlideshowStartPause,
	SlideshowSeparateWindow,
	SpotlightAdd,
	SpotlightRemove,
	SpotlightRealtime,
	CollectFilesOpenDestination
};

QIcon forSecondaryControl( SecondaryControl control, const QPalette& palette );

// semantic roles of the six Remote Access toolbar controls (V3aw), mapped separately from the Teacher secondary
// controls; the checkable View-only and Fullscreen toggles draw the command their current label offers: eye for
// View only (unchecked, Off) and pointer for Remote control (checked, On), maximize for Fullscreen (unchecked,
// Off) and app-window for Window (checked, On); every other role draws one outline for both states, and the
// visible labels remain the primary cues
enum class RemoteAccessControl
{
	ViewOnlyToggle,
	SelectScreen,
	SendShortcut,
	Screenshot,
	FullscreenToggle,
	Exit
};

QIcon forRemoteAccessControl( RemoteAccessControl control, const QPalette& palette );

// semantic roles of the four Send Files and Collect Files transfer states (V3ax): Scheduled (queued),
// Transferring, Waiting and Finished; each state draws one outline for both Off and On, and the file and
// collection names and the numeric progress remain independent cues
enum class FileTransferState
{
	Scheduled,
	Transferring,
	Waiting,
	Finished
};

QIcon forFileTransferState( FileTransferState state, const QPalette& palette );

// semantic roles of the eleven first-party Administration (Configurator) navigation/page identities (V3bb); the
// Configurator maps each page root's non-translated nextspaceActionIconRole property to one role and renders it in the
// page selector's palette; every role draws one outline for both Off and On, and the page titles remain the primary
// cues. Pages without a known role keep their existing selector icon
enum class AdministrationPage
{
	General,
	StudentAgent,
	TeacherConsole,
	AccessControl,
	AuthenticationKeys,
	LocationsAndComputers,
	Broadcast,
	ApplicationsAndWebsites,
	FileTransfer,
	Windows,
	LdapDirectory
};

QIcon forAdministrationPage( AdministrationPage page, const QPalette& palette );

// semantic roles of the four Administration (Configurator) menu commands of V3AM-090 (V3bc), mapped separately from
// the Teacher view and secondary controls although all four reuse reviewed glyphs: Save settings to file, Load settings
// from file, About and Reset configuration; the Configurator renders each in its owning menu's palette, every role draws
// one outline for both Off and On, and the action labels remain the primary cues
enum class AdministrationCommand
{
	SaveSettings,
	LoadSettings,
	About,
	ResetConfiguration
};

QIcon forAdministrationCommand( AdministrationCommand command, const QPalette& palette );

// semantic roles of the nine Administration (Configurator) Access Control page commands of V3AM-091 (V3bd), mapped
// separately from the Teacher, file-transfer, page and menu-command roles although Test, RemoveRule and EditRule reuse
// reviewed glyphs: Test (both Test buttons), authorize the selected group, remove the selected authorized group, add a
// rule, remove a rule, edit the selected rule and move the selected rule up or down; the page renders each in the
// receiving button's palette, every role draws one outline for both Off and On, and the button labels and tooltips
// remain the primary cues
enum class AdministrationAccessControlCommand
{
	Test,
	AddAuthorizedGroup,
	RemoveAuthorizedGroup,
	AddRule,
	RemoveRule,
	EditRule,
	MoveRuleUp,
	MoveRuleDown
};

QIcon forAdministrationAccessControlCommand( AdministrationAccessControlCommand command, const QPalette& palette );

// semantic roles of the three Administration (Configurator) General page commands of V3AM-093 (V3be), mapped separately
// from the Teacher, file-transfer, page, menu-command and Access Control roles although all three reuse reviewed glyphs:
// test the authentication setup, open the log file directory browser and clear all log files; the page renders each in
// the receiving button's palette, every role draws one outline for both Off and On, and the button labels and the log
// file directory button's tooltip remain the primary cues
enum class AdministrationGeneralCommand
{
	TestAuthentication,
	OpenLogDirectory,
	ClearLogFiles
};

QIcon forAdministrationGeneralCommand( AdministrationGeneralCommand command, const QPalette& palette );

// semantic roles of the five Administration (Configurator) Teacher Console page commands of V3AM-094 (V3bf), mapped
// separately from the Teacher, file-transfer, page, menu-command, Access Control and General page roles although all
// five reuse reviewed glyphs: open the user configuration, screenshot and configuration templates directory browsers,
// disable the features selected in All features and enable the features selected in Disabled features; the page renders
// each in the receiving button's palette, every role draws one outline for both Off and On, and each icon-only button's
// tooltip and accessible name remain the primary cues
enum class AdministrationTeacherConsoleCommand
{
	OpenUserConfigurationDirectory,
	OpenScreenshotDirectory,
	OpenConfigurationTemplatesDirectory,
	DisableFeature,
	EnableFeature
};

QIcon forAdministrationTeacherConsoleCommand( AdministrationTeacherConsoleCommand command, const QPalette& palette );

// semantic roles of the two Administration (Configurator) Student Agent page service commands of V3AM-096 (V3bh),
// mapped separately from the Teacher, file-transfer, page, menu-command, Access Control, General and Teacher Console
// page roles: start and stop the Student Agent service; Start reuses the reviewed player-play and Stop draws the pinned
// player-stop; the page renders each in the receiving button's palette, both roles draw one outline for both Off and
// On, and the visible button labels remain the primary cues
enum class AdministrationStudentAgentCommand
{
	StartService,
	StopService
};

QIcon forAdministrationStudentAgentCommand( AdministrationStudentAgentCommand command, const QPalette& palette );

// semantic roles of the two Administration (Configurator) Authentication Keys page directory commands of V3AM-099
// (V3bj), mapped separately from the Teacher, file-transfer, page, menu-command, Access Control, General, Teacher
// Console and Student Agent page roles although both reuse the reviewed folder-open: open the public and the private
// key base directory browsers; the page renders each in the receiving button's palette, both roles draw one outline for
// both Off and On, and each icon-only button's tooltip and accessible name remain the primary cues
enum class AdministrationAuthenticationKeysCommand
{
	OpenPublicKeyDirectory,
	OpenPrivateKeyDirectory
};

QIcon forAdministrationAuthenticationKeysCommand( AdministrationAuthenticationKeysCommand command, const QPalette& palette );

// semantic roles of the eight Administration (Configurator) built-in directory Locations & computers page commands of
// V3AM-101 (V3bk), mapped separately from the Teacher, file-transfer, page, menu-command, Access Control, General,
// Teacher Console, Student Agent and Authentication Keys page roles although they reuse the reviewed plus, trash,
// arrow-up and arrow-down: add, remove, move up and move down a location, and the same four for a computer of the
// selected location; the location and computer roles stay distinct although each pair draws the same outline; the page
// renders each in the receiving button's palette, every role draws one outline for both Off and On, and each icon-only
// button's tooltip and accessible name remain the primary cues
enum class AdministrationBuiltinDirectoryCommand
{
	AddLocation,
	RemoveLocation,
	MoveLocationUp,
	MoveLocationDown,
	AddComputer,
	RemoveComputer,
	MoveComputerUp,
	MoveComputerDown
};

QIcon forAdministrationBuiltinDirectoryCommand( AdministrationBuiltinDirectoryCommand command, const QPalette& palette );

// semantic roles of the four Administration (Configurator) Desktop Services page commands of V3AM-104 (V3bl), shown as
// Applications & websites, mapped separately from the Teacher, file-transfer, page, menu-command, Access Control,
// General, Teacher Console, Student Agent, Authentication Keys and built-in directory page roles although they reuse
// the reviewed plus and trash: add and remove a predefined application, and add and remove a predefined website; the
// application and website roles stay distinct although each pair draws the same outline; the page renders each in the
// receiving button's palette, every role draws one outline for both Off and On, and each icon-only button's tooltip
// and accessible name remain the primary cues
enum class AdministrationDesktopServicesCommand
{
	AddApplication,
	RemoveApplication,
	AddWebsite,
	RemoveWebsite
};

QIcon forAdministrationDesktopServicesCommand( AdministrationDesktopServicesCommand command, const QPalette& palette );

// semantic roles of the four Administration (Configurator) File transfer page directory commands of V3AM-106 (V3bm),
// mapped separately from the Teacher, file-transfer state, page, menu-command, Access Control, General, Teacher Console,
// Student Agent, Authentication Keys, built-in directory and Desktop Services page roles although all four reuse the
// reviewed folder-open: browse for the default source directory, the file transfer destination directory, the files to
// collect source directory and the collected files destination directory; the four roles stay distinct although they
// draw the same outline; the page renders each in the receiving button's palette, every role draws one outline for
// both Off and On, and each icon-only button's tooltip and accessible name remain the primary cues
enum class AdministrationFileTransferCommand
{
	BrowseDefaultSourceDirectory,
	BrowseDestinationDirectory,
	BrowseFilesToCollectSourceDirectory,
	BrowseCollectedFilesDestinationDirectory
};

QIcon forAdministrationFileTransferCommand( AdministrationFileTransferCommand command, const QPalette& palette );

}
