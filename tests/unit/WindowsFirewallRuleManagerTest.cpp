/*
 * WindowsFirewallRuleManagerTest.cpp - fake-store transaction tests
 *
 * This test never loads COM and never reads or mutates the host firewall.
 */

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include <functional>

#include "WindowsFirewallRuleManager.h"

Q_DECLARE_METATYPE( QList<WindowsFirewallRule> )

namespace
{
class FakeRuleStore final : public WindowsFirewallRuleStore
{
public:
	WindowsFirewallStoreResult enumerate( const QStringList& exactNames,
		QList<WindowsFirewallRule>* output ) override
	{
		operations.append( QStringLiteral("enumerate") );
		++callCount;
		if( beforeEnumerate )
		{
			beforeEnumerate( *this );
		}
		if( shouldFail() )
		{
			return failureResult();
		}
		output->clear();
		for( const auto& rule : rules )
		{
			for( const auto& name : exactNames )
			{
				if( rule.name.compare( name, Qt::CaseInsensitive ) == 0 )
				{
					output->append( rule );
					break;
				}
			}
		}
		return success();
	}

	WindowsFirewallStoreResult add( const WindowsFirewallRule& rule ) override
	{
		operations.append( QStringLiteral("add %1").arg( rule.name ) );
		++callCount;
		if( mutateBeforeFailure && callCount == failingCall )
		{
			rules.append( rule );
		}
		if( shouldFail() )
		{
			return failureResult();
		}
		rules.append( rule );
		return success();
	}

	WindowsFirewallStoreResult remove( const QString& exactName ) override
	{
		operations.append( QStringLiteral("remove %1").arg( exactName ) );
		++callCount;
		auto eraseOne = [&]() {
			for( auto it = rules.begin(); it != rules.end(); ++it )
			{
				if( it->name == exactName )
				{
					rules.erase( it );
					return;
				}
			}
		};
		if( mutateBeforeFailure && callCount == failingCall )
		{
			eraseOne();
		}
		if( shouldFail() )
		{
			return failureResult();
		}
		eraseOne();
		return success();
	}

	static WindowsFirewallStoreResult success()
	{
		return {WindowsFirewallStoreResult::Status::Succeeded, {}};
	}

	bool shouldFail() const
	{
		return (failingCall > 0 && (callCount == failingCall || (failAtOrAfter && callCount > failingCall))) ||
			(secondFailingCall > 0 && callCount == secondFailingCall);
	}

	WindowsFirewallStoreResult failureResult() const
	{
		return {ambiguousFailure ? WindowsFirewallStoreResult::Status::Ambiguous
								 : WindowsFirewallStoreResult::Status::Failed,
			QStringLiteral("injected store failure")};
	}

	QList<WindowsFirewallRule> rules;
	QStringList operations;
	int callCount{0};
	int failingCall{0};
	int secondFailingCall{0};
	bool ambiguousFailure{false};
	bool mutateBeforeFailure{false};
	bool failAtOrAfter{false};
	std::function<void( FakeRuleStore& )> beforeEnumerate;
};

QList<WindowsFirewallRuleSpec> specs()
{
	return {
		{WindowsFirewallRuleSpec::Role::Server,
			QStringLiteral("C:/Program Files/NextSpace Classroom Management/veyon-server.exe"),
			QStringLiteral("NextSpace Student Agent Server"), QStringLiteral("Veyon Server")},
		{WindowsFirewallRuleSpec::Role::Worker,
			QStringLiteral("C:/Program Files/NextSpace Classroom Management/veyon-worker.exe"),
			QStringLiteral("NextSpace Student Agent Worker"), QStringLiteral("Veyon Worker")}
	};
}

WindowsFirewallRule expected( int specIndex, bool legacy = false )
{
	const auto allSpecs = specs();
	const auto& spec = allSpecs.at( specIndex );
	return WindowsFirewallRuleManager::expectedRule( spec, legacy ? spec.legacyName : spec.name );
}

QStringList namesOf( const QList<WindowsFirewallRule>& rules )
{
	QStringList names;
	for( const auto& rule : rules )
	{
		names.append( rule.name );
	}
	names.sort();
	return names;
}

QList<WindowsFirewallRule> normalizedRules( QList<WindowsFirewallRule> rules )
{
	std::sort( rules.begin(), rules.end(),
		[]( const auto& first, const auto& second ) { return first.name < second.name; } );
	return rules;
}

QList<WindowsFirewallRule> allOwnedRules()
{
	return {expected( 0, true ), expected( 1, true ), expected( 0 ), expected( 1 )};
}
}

class WindowsFirewallRuleManagerTest : public QObject
{
	Q_OBJECT

private slots:
	void enabledConvergence_data();
	void enabledConvergence();
	void enabledLegacyOrder();
	void disabledConvergence_data();
	void disabledConvergence();
	void ownershipFingerprint_data();
	void ownershipFingerprint();
	void canonicalPathAndCloneAreOwned();
	void duplicateAndCaseCollisionFailWithoutMutation();
	void unreadableEnumerationFailsWithoutMutation();
	void synchronousFailureRollsBack_data();
	void synchronousFailureRollsBack();
	void disabledFailureRollsBack_data();
	void disabledFailureRollsBack();
	void compensationFailureIsReported_data();
	void compensationFailureIsReported();
	void rollbackFailureIsReported();
	void ambiguousMutationIsReenumerated();
	void duplicateRaceFailsClosed();
	void raceNeverDeletesUnknownReplacement();
	void snapshotRoundTrip_data();
	void snapshotRoundTrip();
	void malformedSnapshotRejected_data();
	void malformedSnapshotRejected();
	void snapshotContractTamperRejected_data();
	void snapshotContractTamperRejected();
	void snapshotRulePropertyTamperRejected_data();
	void snapshotRulePropertyTamperRejected();
	void snapshotRestoreBoundaryRollsBackAndRetries_data();
	void snapshotRestoreBoundaryRollsBackAndRetries();
	void interruptedSnapshotRestoreRollsBackAndRetries();
	void atomicSnapshotWriteFailurePreservesExistingFile();
};

void WindowsFirewallRuleManagerTest::enabledConvergence_data()
{
	QTest::addColumn<QList<WindowsFirewallRule>>( "initial" );
	QTest::addColumn<QStringList>( "finalNames" );

	const QStringList currentNames{QStringLiteral("NextSpace Student Agent Server"),
		QStringLiteral("NextSpace Student Agent Worker")};
	QTest::newRow( "absent" ) << QList<WindowsFirewallRule>{} << currentNames;
	QTest::newRow( "legacy-only" ) << QList<WindowsFirewallRule>{expected( 0, true ), expected( 1, true )}
		<< currentNames;
	QTest::newRow( "current-only" ) << QList<WindowsFirewallRule>{expected( 0 ), expected( 1 )}
		<< currentNames;
	QTest::newRow( "mixed-interrupted" )
		<< QList<WindowsFirewallRule>{expected( 0 ), expected( 1, true )} << currentNames;
}

void WindowsFirewallRuleManagerTest::enabledConvergence()
{
	QFETCH( QList<WindowsFirewallRule>, initial );
	QFETCH( QStringList, finalNames );
	FakeRuleStore store;
	store.rules = initial;
	WindowsFirewallRuleManager manager{store};

	const auto result = manager.configure( specs(), true );

	QVERIFY2( result.succeeded(), qPrintable( result.diagnostic ) );
	QCOMPARE( namesOf( store.rules ), finalNames );
}

void WindowsFirewallRuleManagerTest::enabledLegacyOrder()
{
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	WindowsFirewallRuleManager manager{store};

	QVERIFY( manager.configure( specs(), true ).succeeded() );
	const QStringList mutations = store.operations.filter( QRegularExpression{QStringLiteral("^(add|remove) ")} );
	QCOMPARE( mutations, QStringList({
		QStringLiteral("add NextSpace Student Agent Server"),
		QStringLiteral("add NextSpace Student Agent Worker"),
		QStringLiteral("remove Veyon Server"),
		QStringLiteral("remove Veyon Worker")}) );
}

void WindowsFirewallRuleManagerTest::disabledConvergence_data()
{
	QTest::addColumn<QList<WindowsFirewallRule>>( "initial" );
	QTest::newRow( "absent" ) << QList<WindowsFirewallRule>{};
	QTest::newRow( "legacy" ) << QList<WindowsFirewallRule>{expected( 0, true ), expected( 1, true )};
	QTest::newRow( "current" ) << QList<WindowsFirewallRule>{expected( 0 ), expected( 1 )};
	QTest::newRow( "all-owned-mixed" ) << allOwnedRules();
}

void WindowsFirewallRuleManagerTest::disabledConvergence()
{
	QFETCH( QList<WindowsFirewallRule>, initial );
	FakeRuleStore store;
	store.rules = initial;
	WindowsFirewallRuleManager manager{store};
	const auto result = manager.configure( specs(), false );
	QVERIFY2( result.succeeded(), qPrintable( result.diagnostic ) );
	QVERIFY( store.rules.isEmpty() );
}

void WindowsFirewallRuleManagerTest::ownershipFingerprint_data()
{
	QTest::addColumn<QString>( "field" );
	for( const auto& field : {"path", "description", "grouping", "action", "enabled", "protocol", "profiles",
			 "direction", "service", "localPorts", "remotePorts", "localAddresses", "remoteAddresses", "icmp",
			 "interfaces", "interfaceTypes", "edgeTraversal", "edgeTraversalOptions", "package", "owner",
			 "localUsers", "remoteUsers", "remoteMachines", "secureFlags"} )
	{
		QTest::newRow( field ) << QString::fromLatin1( field );
	}
}

void WindowsFirewallRuleManagerTest::ownershipFingerprint()
{
	QFETCH( QString, field );
	auto foreign = expected( 0, true );
	if( field == QStringLiteral("path") ) foreign.applicationPath += QStringLiteral(".other");
	else if( field == QStringLiteral("description") ) foreign.description += QLatin1Char('x');
	else if( field == QStringLiteral("grouping") ) foreign.grouping += QLatin1Char('x');
	else if( field == QStringLiteral("action") ) foreign.action = 0;
	else if( field == QStringLiteral("enabled") ) foreign.enabled = false;
	else if( field == QStringLiteral("protocol") ) foreign.protocol = 17;
	else if( field == QStringLiteral("profiles") ) foreign.profiles = 1;
	else if( field == QStringLiteral("direction") ) foreign.direction = 2;
	else if( field == QStringLiteral("service") ) foreign.serviceName = QStringLiteral("service");
	else if( field == QStringLiteral("localPorts") ) foreign.localPorts = QStringLiteral("11100");
	else if( field == QStringLiteral("remotePorts") ) foreign.remotePorts = QStringLiteral("11100");
	else if( field == QStringLiteral("localAddresses") ) foreign.localAddresses = QStringLiteral("LocalSubnet");
	else if( field == QStringLiteral("remoteAddresses") ) foreign.remoteAddresses = QStringLiteral("LocalSubnet");
	else if( field == QStringLiteral("icmp") ) foreign.icmpTypesAndCodes = QStringLiteral("*");
	else if( field == QStringLiteral("interfaces") ) foreign.interfaces = QStringList{QStringLiteral("LAN")};
	else if( field == QStringLiteral("interfaceTypes") ) foreign.interfaceTypes = QStringLiteral("LAN");
	else if( field == QStringLiteral("edgeTraversal") ) foreign.edgeTraversal = true;
	else if( field == QStringLiteral("edgeTraversalOptions") ) foreign.edgeTraversalOptions = 1;
	else if( field == QStringLiteral("package") ) foreign.localAppPackageId = QStringLiteral("package");
	else if( field == QStringLiteral("owner") ) foreign.localUserOwner = QStringLiteral("owner");
	else if( field == QStringLiteral("localUsers") ) foreign.localUserAuthorizedList = QStringLiteral("users");
	else if( field == QStringLiteral("remoteUsers") ) foreign.remoteUserAuthorizedList = QStringLiteral("users");
	else if( field == QStringLiteral("remoteMachines") ) foreign.remoteMachineAuthorizedList = QStringLiteral("hosts");
	else if( field == QStringLiteral("secureFlags") ) foreign.secureFlags = 1;

	FakeRuleStore store;
	store.rules = {foreign};
	WindowsFirewallRuleManager manager{store};
	const auto before = store.rules;
	const auto result = manager.configure( specs(), true );
	QVERIFY( !result.succeeded() );
	QCOMPARE( store.rules, before );
	QVERIFY( store.operations.filter( QRegularExpression{QStringLiteral("^(add|remove) ")} ).isEmpty() );
}

void WindowsFirewallRuleManagerTest::canonicalPathAndCloneAreOwned()
{
	auto canonicalCaseVariant = expected( 0, true );
	canonicalCaseVariant.applicationPath = canonicalCaseVariant.applicationPath.toUpper().replace( '/', '\\' );
	QCOMPARE( WindowsFirewallRuleManager::classify( canonicalCaseVariant, specs() ),
		WindowsFirewallRuleManager::Classification::Owned );
	const auto bitIdenticalManualClone = expected( 1, true );
	QCOMPARE( WindowsFirewallRuleManager::classify( bitIdenticalManualClone, specs() ),
		WindowsFirewallRuleManager::Classification::Owned );
}

void WindowsFirewallRuleManagerTest::duplicateAndCaseCollisionFailWithoutMutation()
{
	for( auto initial : {QList<WindowsFirewallRule>{expected( 0, true ), expected( 0, true )},
			 QList<WindowsFirewallRule>{expected( 0, true )}} )
	{
		if( initial.size() == 1 ) initial[0].name = QStringLiteral("veyon server");
		FakeRuleStore store;
		store.rules = initial;
		WindowsFirewallRuleManager manager{store};
		const auto before = store.rules;
		QVERIFY( !manager.configure( specs(), true ).succeeded() );
		QCOMPARE( normalizedRules( store.rules ), normalizedRules( before ) );
		QVERIFY( store.operations.filter( QRegularExpression{QStringLiteral("^(add|remove) ")} ).isEmpty() );
	}
}

void WindowsFirewallRuleManagerTest::unreadableEnumerationFailsWithoutMutation()
{
	FakeRuleStore store;
	store.failingCall = 1;
	WindowsFirewallRuleManager manager{store};
	QVERIFY( !manager.configure( specs(), true ).succeeded() );
	QVERIFY( store.rules.isEmpty() );
}

void WindowsFirewallRuleManagerTest::synchronousFailureRollsBack_data()
{
	QTest::addColumn<int>( "failingCall" );
	QTest::addColumn<int>( "failedRole" );
	const QList<QPair<int, WindowsFirewallRuleSpec::Role>> rows{
		{2, WindowsFirewallRuleSpec::Role::Server}, {3, WindowsFirewallRuleSpec::Role::Worker},
		{4, WindowsFirewallRuleSpec::Role::None}, {5, WindowsFirewallRuleSpec::Role::Server},
		{6, WindowsFirewallRuleSpec::Role::Worker}, {7, WindowsFirewallRuleSpec::Role::None}
	};
	for( const auto& row : rows )
		QTest::newRow( qPrintable( QStringLiteral("boundary-%1").arg( row.first ) ) )
			<< row.first << static_cast<int>( row.second );
}

void WindowsFirewallRuleManagerTest::synchronousFailureRollsBack()
{
	QFETCH( int, failingCall );
	QFETCH( int, failedRole );
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	const auto before = store.rules;
	store.failingCall = failingCall;
	WindowsFirewallRuleManager manager{store};
	const auto result = manager.configure( specs(), true );
	QVERIFY( !result.succeeded() );
	QCOMPARE( result.failedRole, static_cast<WindowsFirewallRuleSpec::Role>( failedRole ) );
	QVERIFY( !result.diagnostic.isEmpty() );
	QVERIFY( result.status == WindowsFirewallTransactionResult::Status::RolledBack ||
		result.status == WindowsFirewallTransactionResult::Status::Failed ||
		result.status == WindowsFirewallTransactionResult::Status::RollbackFailed );
	if( result.status != WindowsFirewallTransactionResult::Status::RollbackFailed )
	{
		QCOMPARE( normalizedRules( store.rules ), normalizedRules( before ) );
	}
}

void WindowsFirewallRuleManagerTest::disabledFailureRollsBack_data()
{
	QTest::addColumn<int>( "failingCall" );
	QTest::addColumn<int>( "failedRole" );
	const QList<QPair<int, WindowsFirewallRuleSpec::Role>> rows{
		{2, WindowsFirewallRuleSpec::Role::Server}, {3, WindowsFirewallRuleSpec::Role::Server},
		{4, WindowsFirewallRuleSpec::Role::Worker}, {5, WindowsFirewallRuleSpec::Role::Worker},
		{6, WindowsFirewallRuleSpec::Role::None}
	};
	for( const auto& row : rows )
		QTest::newRow( qPrintable( QStringLiteral("boundary-%1").arg( row.first ) ) )
			<< row.first << static_cast<int>( row.second );
}

void WindowsFirewallRuleManagerTest::disabledFailureRollsBack()
{
	QFETCH( int, failingCall );
	QFETCH( int, failedRole );
	FakeRuleStore store;
	store.rules = allOwnedRules();
	const auto before = store.rules;
	store.failingCall = failingCall;
	WindowsFirewallRuleManager manager{store};
	const auto result = manager.configure( specs(), false );
	QVERIFY( !result.succeeded() );
	QCOMPARE( result.failedRole, static_cast<WindowsFirewallRuleSpec::Role>( failedRole ) );
	QVERIFY( !result.diagnostic.isEmpty() );
	QVERIFY( result.status != WindowsFirewallTransactionResult::Status::RollbackFailed );
	QCOMPARE( normalizedRules( store.rules ), normalizedRules( before ) );
}

void WindowsFirewallRuleManagerTest::compensationFailureIsReported_data()
{
	QTest::addColumn<int>( "compensationCall" );
	for( int call = 7; call <= 17; ++call )
		QTest::newRow( qPrintable( QStringLiteral("rollback-boundary-%1").arg( call ) ) ) << call;
}

void WindowsFirewallRuleManagerTest::compensationFailureIsReported()
{
	QFETCH( int, compensationCall );
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	store.failingCall = 6;
	store.secondFailingCall = compensationCall;
	WindowsFirewallRuleManager manager{store};
	const auto result = manager.configure( specs(), true );
	QCOMPARE( result.status, WindowsFirewallTransactionResult::Status::RollbackFailed );
	QCOMPARE( result.failedRole, WindowsFirewallRuleSpec::Role::Worker );
	QVERIFY( result.diagnostic.contains( QStringLiteral("rollback failed") ) );
}

void WindowsFirewallRuleManagerTest::rollbackFailureIsReported()
{
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	store.failingCall = 2;
	store.failAtOrAfter = true;
	WindowsFirewallRuleManager manager{store};
	const auto result = manager.configure( specs(), true );
	QCOMPARE( result.status, WindowsFirewallTransactionResult::Status::RollbackFailed );
	QVERIFY( result.diagnostic.contains( QStringLiteral("rollback failed") ) );
}

void WindowsFirewallRuleManagerTest::ambiguousMutationIsReenumerated()
{
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	store.failingCall = 2;
	store.ambiguousFailure = true;
	store.mutateBeforeFailure = true;
	WindowsFirewallRuleManager manager{store};
	QVERIFY( !manager.configure( specs(), true ).succeeded() );
	QVERIFY( store.operations.count( QStringLiteral("enumerate") ) >= 2 );
}

void WindowsFirewallRuleManagerTest::duplicateRaceFailsClosed()
{
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	bool injected = false;
	store.beforeEnumerate = [&]( FakeRuleStore& fake ) {
		if( !injected && fake.operations.contains( QStringLiteral("add NextSpace Student Agent Server") ) )
		{
			fake.rules.append( expected( 0 ) );
			injected = true;
		}
	};
	WindowsFirewallRuleManager manager{store};
	const auto result = manager.configure( specs(), true );
	QCOMPARE( result.status, WindowsFirewallTransactionResult::Status::RollbackFailed );
	QVERIFY( !store.operations.contains( QStringLiteral("remove NextSpace Student Agent Server") ) );
	QCOMPARE( std::count_if( store.rules.cbegin(), store.rules.cend(), []( const auto& rule ) {
		return rule.name == QStringLiteral("NextSpace Student Agent Server");
	} ), 2 );
}

void WindowsFirewallRuleManagerTest::raceNeverDeletesUnknownReplacement()
{
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	bool injected = false;
	store.beforeEnumerate = [&]( FakeRuleStore& fake ) {
		if( !injected && fake.operations.contains( QStringLiteral("add NextSpace Student Agent Server") ) )
		{
			auto unknown = expected( 0 );
			unknown.description = QStringLiteral("administrator rule");
			for( auto& rule : fake.rules )
			{
				if( rule.name == unknown.name )
				{
					rule = unknown;
					break;
				}
			}
			injected = true;
		}
	};
	WindowsFirewallRuleManager manager{store};
	QVERIFY( !manager.configure( specs(), true ).succeeded() );
	QVERIFY( store.rules.contains( [&]() { auto r = expected( 0 ); r.description = QStringLiteral("administrator rule"); return r; }() ) );
	QVERIFY( !store.operations.contains( QStringLiteral("remove NextSpace Student Agent Server") ) );
}

void WindowsFirewallRuleManagerTest::snapshotRoundTrip_data()
{
	QTest::addColumn<QList<WindowsFirewallRule>>( "initial" );
	QTest::newRow( "empty" ) << QList<WindowsFirewallRule>{};
	QTest::newRow( "legacy" ) << QList<WindowsFirewallRule>{expected( 0, true ), expected( 1, true )};
	QTest::newRow( "current" ) << QList<WindowsFirewallRule>{expected( 0 ), expected( 1 )};
	QTest::newRow( "mixed" ) << allOwnedRules();
}

void WindowsFirewallRuleManagerTest::snapshotRoundTrip()
{
	QFETCH( QList<WindowsFirewallRule>, initial );
	QTemporaryDir directory;
	QVERIFY( directory.isValid() );
	const auto path = directory.filePath( QStringLiteral("firewall-rules.json") );
	FakeRuleStore store;
	store.rules = initial;
	WindowsFirewallRuleManager manager{store};
	QVERIFY2( manager.captureState( specs(), path ).succeeded(), qPrintable( path ) );
	QVERIFY( manager.validateState( specs(), path ).succeeded() );
	store.rules.clear();
	QVERIFY( manager.restoreState( specs(), path ).succeeded() );
	QCOMPARE( store.rules, initial );
}

void WindowsFirewallRuleManagerTest::malformedSnapshotRejected_data()
{
	QTest::addColumn<QByteArray>( "content" );
	QTest::newRow( "truncated" ) << QByteArray{"{"};
	QTest::newRow( "wrong-version" ) << QByteArray{"{\"schema\":\"nextspace-firewall-state\",\"version\":2}"};
	QTest::newRow( "unknown-field" ) << QByteArray{"{\"schema\":\"nextspace-firewall-state\",\"version\":1,\"unknown\":true}"};
	QTest::newRow( "missing-rules" ) << QByteArray{"{\"schema\":\"nextspace-firewall-state\",\"version\":1}"};
	QTest::newRow( "duplicate-record" ) << QByteArray{"{\"schema\":\"nextspace-firewall-state\",\"version\":1,\"rules\":[{},{}]}"};
}

void WindowsFirewallRuleManagerTest::malformedSnapshotRejected()
{
	QFETCH( QByteArray, content );
	QTemporaryDir directory;
	const auto path = directory.filePath( QStringLiteral("invalid.json") );
	QFile file{path};
	QVERIFY( file.open( QIODevice::WriteOnly ) );
	QCOMPARE( file.write( content ), content.size() );
	file.close();
	FakeRuleStore store;
	WindowsFirewallRuleManager manager{store};
	QVERIFY( !manager.validateState( specs(), path ).succeeded() );
	QVERIFY( !manager.restoreState( specs(), path ).succeeded() );
	QVERIFY( store.operations.filter( QRegularExpression{QStringLiteral("^(add|remove) ")} ).isEmpty() );
}

void WindowsFirewallRuleManagerTest::snapshotContractTamperRejected_data()
{
	QTest::addColumn<QString>( "tamper" );
	QTest::newRow( "wrong-path" ) << QStringLiteral("path");
	QTest::newRow( "wrong-name" ) << QStringLiteral("name");
	QTest::newRow( "duplicate-role" ) << QStringLiteral("duplicate-role");
	QTest::newRow( "duplicate-rule" ) << QStringLiteral("duplicate-rule");
	QTest::newRow( "unknown-top-level" ) << QStringLiteral("unknown-top-level");
}

void WindowsFirewallRuleManagerTest::snapshotContractTamperRejected()
{
	QFETCH( QString, tamper );
	QTemporaryDir directory;
	const auto path = directory.filePath( QStringLiteral("tampered.json") );
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	WindowsFirewallRuleManager manager{store};
	QVERIFY( manager.captureState( specs(), path ).succeeded() );
	QFile file{path};
	QVERIFY( file.open( QIODevice::ReadOnly ) );
	auto object = QJsonDocument::fromJson( file.readAll() ).object();
	file.close();
	if( tamper == QStringLiteral("unknown-top-level") )
	{
		object.insert( QStringLiteral("unknown"), true );
	}
	else if( tamper == QStringLiteral("duplicate-rule") )
	{
		auto rules = object.value( QStringLiteral("rules") ).toArray();
		rules.append( rules.first() );
		object.insert( QStringLiteral("rules"), rules );
	}
	else
	{
		auto roles = object.value( QStringLiteral("roles") ).toArray();
		auto first = roles.first().toObject();
		if( tamper == QStringLiteral("path") ) first.insert( QStringLiteral("applicationPath"), QStringLiteral("C:/wrong.exe") );
		else if( tamper == QStringLiteral("name") ) first.insert( QStringLiteral("name"), QStringLiteral("Wrong Name") );
		else roles[1] = roles.first();
		roles[0] = first;
		object.insert( QStringLiteral("roles"), roles );
	}
	QVERIFY( file.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
	file.write( QJsonDocument{object}.toJson() );
	file.close();
	store.operations.clear();
	QVERIFY( !manager.validateState( specs(), path ).succeeded() );
	QVERIFY( !manager.restoreState( specs(), path ).succeeded() );
	QVERIFY( store.operations.filter( QRegularExpression{QStringLiteral("^(add|remove) ")} ).isEmpty() );
}

void WindowsFirewallRuleManagerTest::snapshotRulePropertyTamperRejected_data()
{
	QTest::addColumn<QString>( "property" );
	for( const auto& property : {"name", "applicationPath", "description", "grouping", "action", "enabled",
			 "protocol", "profiles", "direction", "serviceName", "localPorts", "remotePorts", "localAddresses",
			 "remoteAddresses", "icmpTypesAndCodes", "interfaces", "interfaceTypes", "edgeTraversal",
			 "edgeTraversalOptions", "localAppPackageId", "localUserOwner", "localUserAuthorizedList",
			 "remoteUserAuthorizedList", "remoteMachineAuthorizedList", "secureFlags"} )
		QTest::newRow( property ) << QString::fromLatin1( property );
}

void WindowsFirewallRuleManagerTest::snapshotRulePropertyTamperRejected()
{
	QFETCH( QString, property );
	QTemporaryDir directory;
	const auto path = directory.filePath( QStringLiteral("tampered-property.json") );
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	WindowsFirewallRuleManager manager{store};
	QVERIFY( manager.captureState( specs(), path ).succeeded() );
	QFile file{path};
	QVERIFY( file.open( QIODevice::ReadOnly ) );
	auto object = QJsonDocument::fromJson( file.readAll() ).object();
	file.close();
	auto rules = object.value( QStringLiteral("rules") ).toArray();
	auto rule = rules.first().toObject();
	const auto oldValue = rule.value( property );
	if( oldValue.isString() ) rule.insert( property, oldValue.toString() + QStringLiteral("-tampered") );
	else if( oldValue.isBool() ) rule.insert( property, !oldValue.toBool() );
	else if( oldValue.isDouble() ) rule.insert( property, oldValue.toInt() + 1 );
	else if( oldValue.isNull() ) rule.insert( property, QStringLiteral("tampered") );
	else if( oldValue.isArray() ) { auto values = oldValue.toArray(); values.append( QStringLiteral("tampered") ); rule.insert( property, values ); }
	rules[0] = rule;
	object.insert( QStringLiteral("rules"), rules );
	QVERIFY( file.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
	file.write( QJsonDocument{object}.toJson() );
	file.close();
	store.operations.clear();
	QVERIFY( !manager.validateState( specs(), path ).succeeded() );
	QVERIFY( !manager.restoreState( specs(), path ).succeeded() );
	QVERIFY( store.operations.filter( QRegularExpression{QStringLiteral("^(add|remove) ")} ).isEmpty() );
}

void WindowsFirewallRuleManagerTest::snapshotRestoreBoundaryRollsBackAndRetries_data()
{
	QTest::addColumn<int>( "failingCall" );
	for( int call = 2; call <= 15; ++call )
		QTest::newRow( qPrintable( QStringLiteral("restore-boundary-%1").arg( call ) ) ) << call;
}

void WindowsFirewallRuleManagerTest::snapshotRestoreBoundaryRollsBackAndRetries()
{
	QFETCH( int, failingCall );
	QTemporaryDir directory;
	const auto path = directory.filePath( QStringLiteral("firewall-rules.json") );
	FakeRuleStore store;
	store.rules = allOwnedRules();
	WindowsFirewallRuleManager manager{store};
	QVERIFY( manager.captureState( specs(), path ).succeeded() );
	store.rules.clear();
	store.callCount = 0;
	store.failingCall = failingCall;
	const auto interrupted = manager.restoreState( specs(), path );
	QVERIFY( !interrupted.succeeded() );
	QVERIFY( interrupted.status != WindowsFirewallTransactionResult::Status::RollbackFailed );
	QVERIFY( !interrupted.diagnostic.isEmpty() );
	QVERIFY( store.rules.isEmpty() );
	store.callCount = 0;
	store.failingCall = 0;
	QVERIFY( manager.restoreState( specs(), path ).succeeded() );
	QCOMPARE( normalizedRules( store.rules ), normalizedRules( allOwnedRules() ) );
}

void WindowsFirewallRuleManagerTest::interruptedSnapshotRestoreRollsBackAndRetries()
{
	QTemporaryDir directory;
	const auto path = directory.filePath( QStringLiteral("firewall-rules.json") );
	FakeRuleStore store;
	store.rules = {expected( 0, true ), expected( 1, true )};
	WindowsFirewallRuleManager manager{store};
	QVERIFY( manager.captureState( specs(), path ).succeeded() );
	store.rules.clear();
	store.callCount = 0;
	store.failingCall = 3;
	const auto interrupted = manager.restoreState( specs(), path );
	QCOMPARE( interrupted.status, WindowsFirewallTransactionResult::Status::RolledBack );
	QVERIFY( store.rules.isEmpty() );
	store.callCount = 0;
	store.failingCall = 0;
	QVERIFY( manager.restoreState( specs(), path ).succeeded() );
	QCOMPARE( normalizedRules( store.rules ),
		normalizedRules( QList<WindowsFirewallRule>{expected( 0, true ), expected( 1, true )} ) );
}

void WindowsFirewallRuleManagerTest::atomicSnapshotWriteFailurePreservesExistingFile()
{
	QTemporaryDir directory;
	const auto existing = directory.filePath( QStringLiteral("firewall-rules.json") );
	QFile sentinel{existing};
	QVERIFY( sentinel.open( QIODevice::WriteOnly ) );
	sentinel.write( "sentinel" );
	sentinel.close();
	FakeRuleStore store;
	store.rules = allOwnedRules();
	WindowsFirewallRuleManager manager{store};
	const auto impossible = existing + QStringLiteral("/child.json");
	QVERIFY( !manager.captureState( specs(), impossible ).succeeded() );
	QVERIFY( sentinel.open( QIODevice::ReadOnly ) );
	QCOMPARE( sentinel.readAll(), QByteArray{"sentinel"} );
}

QTEST_GUILESS_MAIN( WindowsFirewallRuleManagerTest )

#include "WindowsFirewallRuleManagerTest.moc"
