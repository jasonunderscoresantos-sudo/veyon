/*
 * WindowsFirewallRuleManager.cpp - pure transactional Windows Firewall model
 *
 * This file is part of Veyon - Virtual Eye On Networks.
 * Copyright (c) 2004-2026 Veyon Solutions.
 */

#include "WindowsFirewallRuleManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace
{
constexpr auto SnapshotSchema = "nextspace-firewall-state";
constexpr int SnapshotVersion = 1;

using Role = WindowsFirewallRuleSpec::Role;
using Result = WindowsFirewallTransactionResult;

QString canonicalPath( const QString& path )
{
	return QDir::cleanPath( QDir::fromNativeSeparators( path ) ).toCaseFolded();
}

QString roleName( Role role )
{
	switch( role )
	{
	case Role::Server: return QStringLiteral("server");
	case Role::Worker: return QStringLiteral("worker");
	case Role::None: break;
	}
	return QStringLiteral("none");
}

QStringList reservedNames( const QList<WindowsFirewallRuleSpec>& exceptions )
{
	QStringList names;
	for( const auto& exception : exceptions )
	{
		names << exception.name << exception.legacyName;
	}
	return names;
}

const WindowsFirewallRuleSpec* specForName( const QList<WindowsFirewallRuleSpec>& exceptions,
	const QString& name )
{
	for( const auto& exception : exceptions )
	{
		if( exception.name == name || exception.legacyName == name )
		{
			return &exception;
		}
	}
	return nullptr;
}

Role roleForName( const QList<WindowsFirewallRuleSpec>& exceptions, const QString& name )
{
	const auto* exception = specForName( exceptions, name );
	return exception ? exception->role : Role::None;
}

bool hasRule( const QList<WindowsFirewallRule>& rules, const QString& name )
{
	return std::any_of( rules.cbegin(), rules.cend(), [&]( const auto& rule ) { return rule.name == name; } );
}

bool sameTopology( QList<WindowsFirewallRule> left, QList<WindowsFirewallRule> right )
{
	auto byName = []( const auto& first, const auto& second ) { return first.name < second.name; };
	std::sort( left.begin(), left.end(), byName );
	std::sort( right.begin(), right.end(), byName );
	return left == right;
}

Result result( Result::Status status, Role role, const QString& diagnostic )
{
	return {status, role, diagnostic};
}

bool validateSpecs( const QList<WindowsFirewallRuleSpec>& exceptions, QString* diagnostic )
{
	if( exceptions.size() != 2 )
	{
		*diagnostic = QStringLiteral("exactly two firewall roles are required");
		return false;
	}
	QSet<int> roles;
	QSet<QString> names;
	for( const auto& exception : exceptions )
	{
		if( exception.role == Role::None || exception.applicationPath.isEmpty() || exception.name.isEmpty() ||
			exception.legacyName.isEmpty() )
		{
			*diagnostic = QStringLiteral("firewall role specification is incomplete");
			return false;
		}
		roles.insert( static_cast<int>( exception.role ) );
		for( const auto& name : {exception.name, exception.legacyName} )
		{
			const auto folded = name.toCaseFolded();
			if( names.contains( folded ) )
			{
				*diagnostic = QStringLiteral("reserved firewall names are not unique");
				return false;
			}
			names.insert( folded );
		}
	}
	if( roles.size() != 2 || !roles.contains( static_cast<int>( Role::Server ) ) ||
		!roles.contains( static_cast<int>( Role::Worker ) ) )
	{
		*diagnostic = QStringLiteral("server and worker firewall roles are required");
		return false;
	}
	return true;
}

bool enumerateOwned( WindowsFirewallRuleStore& store, const QList<WindowsFirewallRuleSpec>& exceptions,
	QList<WindowsFirewallRule>* rules, QString* diagnostic )
{
	rules->clear();
	const auto storeResult = store.enumerate( reservedNames( exceptions ), rules );
	if( !storeResult.succeeded() )
	{
		*diagnostic = QStringLiteral("firewall rule enumeration failed: %1").arg( storeResult.diagnostic );
		return false;
	}

	QSet<QString> observedNames;
	for( const auto& rule : std::as_const( *rules ) )
	{
		const auto folded = rule.name.toCaseFolded();
		if( observedNames.contains( folded ) )
		{
			*diagnostic = QStringLiteral("duplicate reserved firewall rule: %1").arg( rule.name );
			return false;
		}
		observedNames.insert( folded );
		if( WindowsFirewallRuleManager::classify( rule, exceptions ) !=
			WindowsFirewallRuleManager::Classification::Owned )
		{
			*diagnostic = QStringLiteral("reserved firewall rule is external or unknown: %1").arg( rule.name );
			return false;
		}
	}
	return true;
}

bool addAndVerify( WindowsFirewallRuleStore& store, const QList<WindowsFirewallRuleSpec>& exceptions,
	const WindowsFirewallRule& rule, QString* diagnostic )
{
	const auto addResult = store.add( rule );
	QList<WindowsFirewallRule> after;
	QString enumerateDiagnostic;
	if( !enumerateOwned( store, exceptions, &after, &enumerateDiagnostic ) )
	{
		*diagnostic = addResult.succeeded() ? enumerateDiagnostic :
			QStringLiteral("%1; %2").arg( addResult.diagnostic, enumerateDiagnostic );
		return false;
	}
	if( !hasRule( after, rule.name ) )
	{
		*diagnostic = QStringLiteral("firewall rule add was not observable: %1 (%2)")
			.arg( rule.name, addResult.diagnostic );
		return false;
	}
	if( !addResult.succeeded() && addResult.status != WindowsFirewallStoreResult::Status::Ambiguous )
	{
		*diagnostic = QStringLiteral("firewall rule add failed: %1").arg( addResult.diagnostic );
		return false;
	}
	return true;
}

bool removeAndVerify( WindowsFirewallRuleStore& store, const QList<WindowsFirewallRuleSpec>& exceptions,
	const QString& name, QString* diagnostic )
{
	QList<WindowsFirewallRule> before;
	if( !enumerateOwned( store, exceptions, &before, diagnostic ) )
	{
		return false;
	}
	if( !hasRule( before, name ) )
	{
		return true;
	}
	const auto removeResult = store.remove( name );
	QList<WindowsFirewallRule> after;
	QString enumerateDiagnostic;
	if( !enumerateOwned( store, exceptions, &after, &enumerateDiagnostic ) )
	{
		*diagnostic = removeResult.succeeded() ? enumerateDiagnostic :
			QStringLiteral("%1; %2").arg( removeResult.diagnostic, enumerateDiagnostic );
		return false;
	}
	if( hasRule( after, name ) )
	{
		*diagnostic = QStringLiteral("firewall rule removal was not observable: %1 (%2)")
			.arg( name, removeResult.diagnostic );
		return false;
	}
	if( !removeResult.succeeded() && removeResult.status != WindowsFirewallStoreResult::Status::Ambiguous )
	{
		*diagnostic = QStringLiteral("firewall rule removal failed: %1").arg( removeResult.diagnostic );
		return false;
	}
	return true;
}

bool restoreTopology( WindowsFirewallRuleStore& store, const QList<WindowsFirewallRuleSpec>& exceptions,
	const QList<WindowsFirewallRule>& target, QString* diagnostic )
{
	QList<WindowsFirewallRule> current;
	if( !enumerateOwned( store, exceptions, &current, diagnostic ) )
	{
		return false;
	}

	// Restore removed coverage before deleting replacements created by the failed operation.
	for( const auto& wanted : target )
	{
		if( !hasRule( current, wanted.name ) )
		{
			if( !addAndVerify( store, exceptions, wanted, diagnostic ) )
			{
				return false;
			}
			if( !enumerateOwned( store, exceptions, &current, diagnostic ) )
			{
				return false;
			}
		}
	}

	for( const auto& observed : current )
	{
		if( !hasRule( target, observed.name ) )
		{
			if( !removeAndVerify( store, exceptions, observed.name, diagnostic ) )
			{
				return false;
			}
		}
	}

	QList<WindowsFirewallRule> finalRules;
	if( !enumerateOwned( store, exceptions, &finalRules, diagnostic ) )
	{
		return false;
	}
	if( !sameTopology( finalRules, target ) )
	{
		*diagnostic = QStringLiteral("firewall rollback topology verification failed");
		return false;
	}
	return true;
}

Result rollbackResult( WindowsFirewallRuleStore& store, const QList<WindowsFirewallRuleSpec>& exceptions,
	const QList<WindowsFirewallRule>& original, Role failedRole, const QString& failure )
{
	QString rollbackDiagnostic;
	if( restoreTopology( store, exceptions, original, &rollbackDiagnostic ) )
	{
		return result( Result::Status::RolledBack, failedRole,
			QStringLiteral("%1; original firewall topology restored").arg( failure ) );
	}
	return result( Result::Status::RollbackFailed, failedRole,
		QStringLiteral("%1; rollback failed: %2").arg( failure, rollbackDiagnostic ) );
}

QJsonValue optionalString( const std::optional<QString>& value )
{
	return value ? QJsonValue{*value} : QJsonValue{QJsonValue::Null};
}

QJsonObject ruleToJson( const WindowsFirewallRule& rule )
{
	QJsonObject object{
		{QStringLiteral("name"), rule.name},
		{QStringLiteral("applicationPath"), rule.applicationPath},
		{QStringLiteral("description"), rule.description},
		{QStringLiteral("grouping"), rule.grouping},
		{QStringLiteral("action"), rule.action},
		{QStringLiteral("enabled"), rule.enabled},
		{QStringLiteral("protocol"), rule.protocol},
		{QStringLiteral("profiles"), rule.profiles},
		{QStringLiteral("direction"), rule.direction},
		{QStringLiteral("serviceName"), optionalString( rule.serviceName )},
		{QStringLiteral("localPorts"), rule.localPorts},
		{QStringLiteral("remotePorts"), rule.remotePorts},
		{QStringLiteral("localAddresses"), rule.localAddresses},
		{QStringLiteral("remoteAddresses"), rule.remoteAddresses},
		{QStringLiteral("icmpTypesAndCodes"), optionalString( rule.icmpTypesAndCodes )},
		{QStringLiteral("interfaceTypes"), rule.interfaceTypes},
		{QStringLiteral("edgeTraversal"), rule.edgeTraversal},
		{QStringLiteral("edgeTraversalOptions"), rule.edgeTraversalOptions},
		{QStringLiteral("localAppPackageId"), optionalString( rule.localAppPackageId )},
		{QStringLiteral("localUserOwner"), optionalString( rule.localUserOwner )},
		{QStringLiteral("localUserAuthorizedList"), optionalString( rule.localUserAuthorizedList )},
		{QStringLiteral("remoteUserAuthorizedList"), optionalString( rule.remoteUserAuthorizedList )},
		{QStringLiteral("remoteMachineAuthorizedList"), optionalString( rule.remoteMachineAuthorizedList )},
		{QStringLiteral("secureFlags"), rule.secureFlags}
	};
	if( rule.interfaces )
	{
		QJsonArray interfaces;
		for( const auto& interfaceName : *rule.interfaces ) interfaces.append( interfaceName );
		object.insert( QStringLiteral("interfaces"), interfaces );
	}
	else
	{
		object.insert( QStringLiteral("interfaces"), QJsonValue::Null );
	}
	return object;
}

bool onlyKeys( const QJsonObject& object, const QSet<QString>& expected, QString* diagnostic )
{
	QSet<QString> actual;
	for( auto it = object.constBegin(); it != object.constEnd(); ++it ) actual.insert( it.key() );
	if( actual != expected )
	{
		*diagnostic = QStringLiteral("snapshot contains missing or unknown fields");
		return false;
	}
	return true;
}

bool readString( const QJsonObject& object, const QString& key, QString* value )
{
	const auto item = object.value( key );
	if( !item.isString() ) return false;
	*value = item.toString();
	return true;
}

bool readInt( const QJsonObject& object, const QString& key, int* value )
{
	const auto item = object.value( key );
	if( !item.isDouble() ) return false;
	const auto number = item.toDouble();
	const auto integer = static_cast<int>( number );
	if( number != integer ) return false;
	*value = integer;
	return true;
}

bool readOptionalString( const QJsonObject& object, const QString& key, std::optional<QString>* value )
{
	const auto item = object.value( key );
	if( item.isNull() )
	{
		value->reset();
		return true;
	}
	if( !item.isString() ) return false;
	*value = item.toString();
	return true;
}

bool ruleFromJson( const QJsonValue& value, WindowsFirewallRule* rule, QString* diagnostic )
{
	if( !value.isObject() )
	{
		*diagnostic = QStringLiteral("snapshot rule is not an object");
		return false;
	}
	const auto object = value.toObject();
	const QSet<QString> keys{
		QStringLiteral("name"), QStringLiteral("applicationPath"), QStringLiteral("description"),
		QStringLiteral("grouping"), QStringLiteral("action"), QStringLiteral("enabled"),
		QStringLiteral("protocol"), QStringLiteral("profiles"), QStringLiteral("direction"),
		QStringLiteral("serviceName"), QStringLiteral("localPorts"), QStringLiteral("remotePorts"),
		QStringLiteral("localAddresses"), QStringLiteral("remoteAddresses"), QStringLiteral("icmpTypesAndCodes"),
		QStringLiteral("interfaces"), QStringLiteral("interfaceTypes"), QStringLiteral("edgeTraversal"),
		QStringLiteral("edgeTraversalOptions"), QStringLiteral("localAppPackageId"), QStringLiteral("localUserOwner"),
		QStringLiteral("localUserAuthorizedList"), QStringLiteral("remoteUserAuthorizedList"),
		QStringLiteral("remoteMachineAuthorizedList"), QStringLiteral("secureFlags")
	};
	if( !onlyKeys( object, keys, diagnostic ) ||
		!readString( object, QStringLiteral("name"), &rule->name ) ||
		!readString( object, QStringLiteral("applicationPath"), &rule->applicationPath ) ||
		!readString( object, QStringLiteral("description"), &rule->description ) ||
		!readString( object, QStringLiteral("grouping"), &rule->grouping ) ||
		!readInt( object, QStringLiteral("action"), &rule->action ) ||
		!object.value( QStringLiteral("enabled") ).isBool() ||
		!readInt( object, QStringLiteral("protocol"), &rule->protocol ) ||
		!readInt( object, QStringLiteral("profiles"), &rule->profiles ) ||
		!readInt( object, QStringLiteral("direction"), &rule->direction ) ||
		!readOptionalString( object, QStringLiteral("serviceName"), &rule->serviceName ) ||
		!readString( object, QStringLiteral("localPorts"), &rule->localPorts ) ||
		!readString( object, QStringLiteral("remotePorts"), &rule->remotePorts ) ||
		!readString( object, QStringLiteral("localAddresses"), &rule->localAddresses ) ||
		!readString( object, QStringLiteral("remoteAddresses"), &rule->remoteAddresses ) ||
		!readOptionalString( object, QStringLiteral("icmpTypesAndCodes"), &rule->icmpTypesAndCodes ) ||
		!readString( object, QStringLiteral("interfaceTypes"), &rule->interfaceTypes ) ||
		!object.value( QStringLiteral("edgeTraversal") ).isBool() ||
		!readInt( object, QStringLiteral("edgeTraversalOptions"), &rule->edgeTraversalOptions ) ||
		!readOptionalString( object, QStringLiteral("localAppPackageId"), &rule->localAppPackageId ) ||
		!readOptionalString( object, QStringLiteral("localUserOwner"), &rule->localUserOwner ) ||
		!readOptionalString( object, QStringLiteral("localUserAuthorizedList"), &rule->localUserAuthorizedList ) ||
		!readOptionalString( object, QStringLiteral("remoteUserAuthorizedList"), &rule->remoteUserAuthorizedList ) ||
		!readOptionalString( object, QStringLiteral("remoteMachineAuthorizedList"), &rule->remoteMachineAuthorizedList ) ||
		!readInt( object, QStringLiteral("secureFlags"), &rule->secureFlags ) )
	{
		if( diagnostic->isEmpty() ) *diagnostic = QStringLiteral("snapshot rule has an invalid property type");
		return false;
	}
	rule->enabled = object.value( QStringLiteral("enabled") ).toBool();
	rule->edgeTraversal = object.value( QStringLiteral("edgeTraversal") ).toBool();
	const auto interfacesValue = object.value( QStringLiteral("interfaces") );
	if( interfacesValue.isNull() )
	{
		rule->interfaces.reset();
	}
	else if( interfacesValue.isArray() )
	{
		QStringList interfaces;
		for( const auto& entry : interfacesValue.toArray() )
		{
			if( !entry.isString() )
			{
				*diagnostic = QStringLiteral("snapshot interfaces entry is invalid");
				return false;
			}
			interfaces.append( entry.toString() );
		}
		rule->interfaces = interfaces;
	}
	else
	{
		*diagnostic = QStringLiteral("snapshot interfaces property is invalid");
		return false;
	}
	return true;
}

QJsonObject snapshotObject( const QList<WindowsFirewallRuleSpec>& exceptions,
	const QList<WindowsFirewallRule>& rules )
{
	QJsonArray roles;
	for( const auto& exception : exceptions )
	{
		roles.append( QJsonObject{
			{QStringLiteral("role"), roleName( exception.role )},
			{QStringLiteral("applicationPath"), exception.applicationPath},
			{QStringLiteral("name"), exception.name},
			{QStringLiteral("legacyName"), exception.legacyName}
		} );
	}
	QJsonArray ruleArray;
	for( const auto& rule : rules ) ruleArray.append( ruleToJson( rule ) );
	return {
		{QStringLiteral("schema"), QString::fromLatin1( SnapshotSchema )},
		{QStringLiteral("version"), SnapshotVersion},
		{QStringLiteral("roles"), roles},
		{QStringLiteral("rules"), ruleArray}
	};
}

bool parseSnapshot( const QList<WindowsFirewallRuleSpec>& exceptions, const QString& filePath,
	QList<WindowsFirewallRule>* rules, QString* diagnostic )
{
	QFile file{filePath};
	if( !file.open( QIODevice::ReadOnly ) )
	{
		*diagnostic = QStringLiteral("could not open firewall snapshot: %1").arg( file.errorString() );
		return false;
	}
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson( file.readAll(), &parseError );
	if( parseError.error != QJsonParseError::NoError || !document.isObject() )
	{
		*diagnostic = QStringLiteral("firewall snapshot JSON is malformed");
		return false;
	}
	const auto object = document.object();
	if( !onlyKeys( object, {QStringLiteral("schema"), QStringLiteral("version"),
			QStringLiteral("roles"), QStringLiteral("rules")}, diagnostic ) ||
		object.value( QStringLiteral("schema") ).toString() != QString::fromLatin1( SnapshotSchema ) ||
		object.value( QStringLiteral("version") ).toInt( -1 ) != SnapshotVersion ||
		!object.value( QStringLiteral("roles") ).isArray() || !object.value( QStringLiteral("rules") ).isArray() )
	{
		if( diagnostic->isEmpty() ) *diagnostic = QStringLiteral("firewall snapshot schema or version is invalid");
		return false;
	}

	const auto roles = object.value( QStringLiteral("roles") ).toArray();
	if( roles.size() != exceptions.size() )
	{
		*diagnostic = QStringLiteral("firewall snapshot role count is invalid");
		return false;
	}
	QSet<int> seenRoles;
	for( const auto& roleValue : roles )
	{
		if( !roleValue.isObject() ) return false;
		const auto roleObject = roleValue.toObject();
		if( !onlyKeys( roleObject, {QStringLiteral("role"), QStringLiteral("applicationPath"),
				QStringLiteral("name"), QStringLiteral("legacyName")}, diagnostic ) ) return false;
		const auto roleText = roleObject.value( QStringLiteral("role") ).toString();
		const auto role = roleText == QStringLiteral("server") ? Role::Server :
			roleText == QStringLiteral("worker") ? Role::Worker : Role::None;
		const auto it = std::find_if( exceptions.cbegin(), exceptions.cend(),
			[&]( const auto& exception ) { return exception.role == role; } );
		if( role == Role::None || it == exceptions.cend() || seenRoles.contains( static_cast<int>( role ) ) ||
			canonicalPath( roleObject.value( QStringLiteral("applicationPath") ).toString() ) !=
				canonicalPath( it->applicationPath ) ||
			roleObject.value( QStringLiteral("name") ).toString() != it->name ||
			roleObject.value( QStringLiteral("legacyName") ).toString() != it->legacyName )
		{
			*diagnostic = QStringLiteral("firewall snapshot role/name/path contract is invalid");
			return false;
		}
		seenRoles.insert( static_cast<int>( role ) );
	}

	rules->clear();
	QSet<QString> names;
	for( const auto& ruleValue : object.value( QStringLiteral("rules") ).toArray() )
	{
		WindowsFirewallRule rule;
		if( !ruleFromJson( ruleValue, &rule, diagnostic ) ||
			WindowsFirewallRuleManager::classify( rule, exceptions ) !=
				WindowsFirewallRuleManager::Classification::Owned ||
			names.contains( rule.name.toCaseFolded() ) )
		{
			if( diagnostic->isEmpty() ) *diagnostic = QStringLiteral("firewall snapshot rule is invalid or duplicate");
			return false;
		}
		names.insert( rule.name.toCaseFolded() );
		rules->append( rule );
	}
	return true;
}
}

bool WindowsFirewallRule::operator==( const WindowsFirewallRule& other ) const
{
	return name == other.name && canonicalPath( applicationPath ) == canonicalPath( other.applicationPath ) &&
		description == other.description && grouping == other.grouping && action == other.action &&
		enabled == other.enabled && protocol == other.protocol && profiles == other.profiles &&
		direction == other.direction && serviceName == other.serviceName && localPorts == other.localPorts &&
		remotePorts == other.remotePorts && localAddresses == other.localAddresses &&
		remoteAddresses == other.remoteAddresses && icmpTypesAndCodes == other.icmpTypesAndCodes &&
		interfaces == other.interfaces && interfaceTypes == other.interfaceTypes &&
		edgeTraversal == other.edgeTraversal && edgeTraversalOptions == other.edgeTraversalOptions &&
		localAppPackageId == other.localAppPackageId && localUserOwner == other.localUserOwner &&
		localUserAuthorizedList == other.localUserAuthorizedList &&
		remoteUserAuthorizedList == other.remoteUserAuthorizedList &&
		remoteMachineAuthorizedList == other.remoteMachineAuthorizedList && secureFlags == other.secureFlags;
}

WindowsFirewallRuleManager::WindowsFirewallRuleManager( WindowsFirewallRuleStore& store ) :
	m_store{store}
{
}

WindowsFirewallRule WindowsFirewallRuleManager::expectedRule( const WindowsFirewallRuleSpec& exception,
	const QString& name )
{
	WindowsFirewallRule rule;
	rule.name = name;
	rule.applicationPath = exception.applicationPath;
	rule.description = name;
	rule.grouping = name;
	return rule;
}

WindowsFirewallRuleManager::Classification WindowsFirewallRuleManager::classify(
	const WindowsFirewallRule& rule, const QList<WindowsFirewallRuleSpec>& exceptions )
{
	const auto* exception = specForName( exceptions, rule.name );
	if( exception == nullptr )
	{
		return Classification::ExternalOrUnknown;
	}
	return rule == expectedRule( *exception, rule.name ) ? Classification::Owned :
		Classification::ExternalOrUnknown;
}

Result WindowsFirewallRuleManager::configure( const QList<WindowsFirewallRuleSpec>& exceptions, bool enabled )
{
	QMutexLocker locker{&m_mutex};
	QString diagnostic;
	if( !validateSpecs( exceptions, &diagnostic ) )
	{
		return result( Result::Status::Failed, Role::None, diagnostic );
	}
	QList<WindowsFirewallRule> original;
	if( !enumerateOwned( m_store, exceptions, &original, &diagnostic ) )
	{
		return result( Result::Status::Failed, Role::None, diagnostic );
	}

	if( enabled )
	{
		QList<WindowsFirewallRule> current = original;
		for( const auto& exception : exceptions )
		{
			if( !hasRule( current, exception.name ) )
			{
				const auto addResult = m_store.add( expectedRule( exception, exception.name ) );
				if( !addResult.succeeded() )
				{
					return rollbackResult( m_store, exceptions, original, exception.role,
						QStringLiteral("failed to add %1 rule: %2").arg( roleName( exception.role ),
							addResult.diagnostic ) );
				}
			}
		}
		if( !enumerateOwned( m_store, exceptions, &current, &diagnostic ) )
		{
			return rollbackResult( m_store, exceptions, original, Role::None, diagnostic );
		}
		for( const auto& exception : exceptions )
		{
			if( !hasRule( current, exception.name ) )
			{
				return rollbackResult( m_store, exceptions, original, exception.role,
					QStringLiteral("replacement firewall pair verification failed") );
			}
		}
		for( const auto& exception : exceptions )
		{
			if( hasRule( current, exception.legacyName ) )
			{
				const auto removeResult = m_store.remove( exception.legacyName );
				if( !removeResult.succeeded() )
				{
					return rollbackResult( m_store, exceptions, original, exception.role,
						QStringLiteral("failed to remove legacy %1 rule: %2").arg( roleName( exception.role ),
							removeResult.diagnostic ) );
				}
			}
		}
		QList<WindowsFirewallRule> finalRules;
		if( !enumerateOwned( m_store, exceptions, &finalRules, &diagnostic ) )
		{
			return rollbackResult( m_store, exceptions, original, Role::None, diagnostic );
		}
		QList<WindowsFirewallRule> target;
		for( const auto& exception : exceptions ) target.append( expectedRule( exception, exception.name ) );
		if( !sameTopology( finalRules, target ) )
		{
			return rollbackResult( m_store, exceptions, original, Role::None,
				QStringLiteral("enabled firewall postcondition verification failed") );
		}
		return result( Result::Status::Succeeded, Role::None, {} );
	}

	for( const auto& exception : exceptions )
	{
		for( const auto& name : {exception.name, exception.legacyName} )
		{
			if( hasRule( original, name ) )
			{
				const auto removeResult = m_store.remove( name );
				if( !removeResult.succeeded() )
				{
					return rollbackResult( m_store, exceptions, original, exception.role,
						QStringLiteral("failed to disable %1 rule: %2").arg( roleName( exception.role ),
							removeResult.diagnostic ) );
				}
			}
		}
	}
	QList<WindowsFirewallRule> finalRules;
	if( !enumerateOwned( m_store, exceptions, &finalRules, &diagnostic ) || !finalRules.isEmpty() )
	{
		if( diagnostic.isEmpty() ) diagnostic = QStringLiteral("disabled firewall postcondition verification failed");
		return rollbackResult( m_store, exceptions, original, Role::None, diagnostic );
	}
	return result( Result::Status::Succeeded, Role::None, {} );
}

Result WindowsFirewallRuleManager::captureState( const QList<WindowsFirewallRuleSpec>& exceptions,
	const QString& filePath )
{
	QMutexLocker locker{&m_mutex};
	QString diagnostic;
	if( !validateSpecs( exceptions, &diagnostic ) ) return result( Result::Status::Failed, Role::None, diagnostic );
	QList<WindowsFirewallRule> rules;
	if( !enumerateOwned( m_store, exceptions, &rules, &diagnostic ) )
		return result( Result::Status::Failed, Role::None, diagnostic );
	QSaveFile file{filePath};
	if( !file.open( QIODevice::WriteOnly ) )
		return result( Result::Status::Failed, Role::None,
			QStringLiteral("could not create firewall snapshot: %1").arg( file.errorString() ) );
	const auto bytes = QJsonDocument{snapshotObject( exceptions, rules )}.toJson( QJsonDocument::Indented );
	if( file.write( bytes ) != bytes.size() || !file.commit() )
		return result( Result::Status::Failed, Role::None,
			QStringLiteral("could not atomically commit firewall snapshot: %1").arg( file.errorString() ) );
	return result( Result::Status::Succeeded, Role::None, {} );
}

Result WindowsFirewallRuleManager::validateState( const QList<WindowsFirewallRuleSpec>& exceptions,
	const QString& filePath )
{
	QMutexLocker locker{&m_mutex};
	QString diagnostic;
	if( !validateSpecs( exceptions, &diagnostic ) ) return result( Result::Status::Failed, Role::None, diagnostic );
	QList<WindowsFirewallRule> rules;
	return parseSnapshot( exceptions, filePath, &rules, &diagnostic ) ?
		result( Result::Status::Succeeded, Role::None, {} ) : result( Result::Status::Failed, Role::None, diagnostic );
}

Result WindowsFirewallRuleManager::restoreState( const QList<WindowsFirewallRuleSpec>& exceptions,
	const QString& filePath )
{
	QMutexLocker locker{&m_mutex};
	QString diagnostic;
	if( !validateSpecs( exceptions, &diagnostic ) ) return result( Result::Status::Failed, Role::None, diagnostic );
	QList<WindowsFirewallRule> target;
	if( !parseSnapshot( exceptions, filePath, &target, &diagnostic ) )
		return result( Result::Status::Failed, Role::None, diagnostic );
	QList<WindowsFirewallRule> original;
	if( !enumerateOwned( m_store, exceptions, &original, &diagnostic ) )
		return result( Result::Status::Failed, Role::None, diagnostic );
	if( restoreTopology( m_store, exceptions, target, &diagnostic ) )
		return result( Result::Status::Succeeded, Role::None, {} );
	QString rollbackDiagnostic;
	if( restoreTopology( m_store, exceptions, original, &rollbackDiagnostic ) )
		return result( Result::Status::RolledBack, Role::None,
			QStringLiteral("snapshot restore failed; original topology restored: %1").arg( diagnostic ) );
	return result( Result::Status::RollbackFailed, Role::None,
		QStringLiteral("snapshot restore failed: %1; rollback failed: %2").arg( diagnostic, rollbackDiagnostic ) );
}
