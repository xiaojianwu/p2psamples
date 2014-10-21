#include <QList>
#include <QString>
#include <QHostAddress>

struct SIPInfo {
	QString user_fragment;
	QString password;

	struct Candidate {
		QHostAddress address;
		quint16 port;
		QString type;
		int component;
		int priority;
		QString foundation;
	};
	QList<Candidate> candidates;
};