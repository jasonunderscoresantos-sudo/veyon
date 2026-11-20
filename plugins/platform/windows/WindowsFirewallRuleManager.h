/*
 * WindowsFirewallRuleManager.h - pure transactional Windows Firewall model
 *
 * This file is part of Veyon - Virtual Eye On Networks.
 * Copyright (c) 2004-2026 Veyon Solutions.
 *
 * Veyon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WINDOWS_FIREWALL_RULE_MANAGER_H
#define WINDOWS_FIREWALL_RULE_MANAGER_H

#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <optional>

struct WindowsFirewallRule
{
	QString name;
	QString applicationPath;
	QString description;
	QString grouping;
	int action{1};
	bool enabled{true};
	int protocol{6};
	int profiles{0x7fffffff};
	int direction{1};
	std::optional<QString> serviceName;
	QString localPorts{QStringLiteral("*")};
	QString remotePorts{QStringLiteral("*")};
	QString localAddresses{QStringLiteral("*")};
	QString remoteAddresses{QStringLiteral("*")};
	std::optional<QString> icmpTypesAndCodes;
	std::optional<QStringList> interfaces;
	QString interfaceTypes{QStringLiteral("All")};
	bool edgeTraversal{false};
	int edgeTraversalOptions{0};
	std::optional<QString> localAppPackageId;
	std::optional<QString> localUserOwner;
	std::optional<QString> localUserAuthorizedList;
	std::optional<QString> remoteUserAuthorizedList;
	std::optional<QString> remoteMachineAuthorizedList;
	int secureFlags{0};

	bool operator==( const WindowsFirewallRule& other ) const;
	bool operator!=( const WindowsFirewallRule& other ) const
	{
		return !( *this == other );
	}
};

struct WindowsFirewallRuleSpec
{
	enum class Role { None, Server, Worker };

	Role role{Role::None};
	QString applicationPath;
	QString name;
	QString legacyName;
};

struct WindowsFirewallStoreResult
{
	enum class Status { Succeeded, Failed, Ambiguous };

	Status status{Status::Failed};
	QString diagnostic;

	bool succeeded() const
	{
		return status == Status::Succeeded;
	}
};

class WindowsFirewallRuleStore
{
public:
	virtual ~WindowsFirewallRuleStore() = default;

	virtual WindowsFirewallStoreResult enumerate( const QStringList& exactNames,
		QList<WindowsFirewallRule>* rules ) = 0;
	virtual WindowsFirewallStoreResult add( const WindowsFirewallRule& rule ) = 0;
	virtual WindowsFirewallStoreResult remove( const QString& exactName ) = 0;
};

struct WindowsFirewallTransactionResult
{
	enum class Status { Succeeded, Failed, RolledBack, RollbackFailed };

	Status status{Status::Failed};
	WindowsFirewallRuleSpec::Role failedRole{WindowsFirewallRuleSpec::Role::None};
	QString diagnostic;

	bool succeeded() const
	{
		return status == Status::Succeeded;
	}
};

class WindowsFirewallRuleManager
{
public:
	enum class Classification { Absent, Owned, ExternalOrUnknown };

	explicit WindowsFirewallRuleManager( WindowsFirewallRuleStore& store );

	WindowsFirewallTransactionResult configure( const QList<WindowsFirewallRuleSpec>& exceptions,
		bool enabled );
	WindowsFirewallTransactionResult captureState( const QList<WindowsFirewallRuleSpec>& exceptions,
		const QString& filePath );
	WindowsFirewallTransactionResult validateState( const QList<WindowsFirewallRuleSpec>& exceptions,
		const QString& filePath );
	WindowsFirewallTransactionResult restoreState( const QList<WindowsFirewallRuleSpec>& exceptions,
		const QString& filePath );

	static WindowsFirewallRule expectedRule( const WindowsFirewallRuleSpec& exception,
		const QString& name );
	static Classification classify( const WindowsFirewallRule& rule,
		const QList<WindowsFirewallRuleSpec>& exceptions );

private:
	WindowsFirewallRuleStore& m_store;
	QMutex m_mutex;
};

#endif // WINDOWS_FIREWALL_RULE_MANAGER_H
