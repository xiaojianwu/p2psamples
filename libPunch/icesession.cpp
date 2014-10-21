#include "icesession.h"

#include <QDebug>

#include <QHostAddress>


const char* ICESession::kStunServer = "stunserver.org";
int ICESession::sComponentId = 0;
pj_caching_pool ICESession::sCachingPool;
pj_ice_strans_cfg ICESession::sIceConfig;

pj_pool_t* ICESession::sPool;
pj_thread_t* ICESession::sThread;

ICESession::ICESession(QObject* parent)
	: QIODevice(parent) {
}


bool ICESession::Init(Direction direction) {
	// Create instance.
	pj_ice_strans_cb ice_cb;
	pj_bzero(&ice_cb, sizeof(ice_cb));

	ice_cb.on_rx_data = &OnReceiveData;
	ice_cb.on_ice_complete = &OnICEComplete;

	component_id_ = ++sComponentId;

	pj_status_t status = pj_ice_strans_create(
		"clementine",
		&sIceConfig,
		component_id_,
		this,
		&ice_cb,
		&ice_instance_);
	if (status != PJ_SUCCESS) {
		qDebug() << "Failed to create ICE instance";
		return false;
	}

	// TODO
	pj_ice_sess_role role = direction == DirectionControlling
		? PJ_ICE_SESS_ROLE_CONTROLLING
		: PJ_ICE_SESS_ROLE_CONTROLLED;

	status = pj_ice_strans_init_ice(ice_instance_, role, NULL, NULL);

	qDebug() << "Init ice:" << status;

	return true;
}

QString CandidateTypeToString(pj_ice_cand_type type) {
	switch (type) {
	case PJ_ICE_CAND_TYPE_HOST:
		return "host";
	case PJ_ICE_CAND_TYPE_SRFLX:
		return "srflx";
	case PJ_ICE_CAND_TYPE_PRFLX:
		return "prflx";
	case PJ_ICE_CAND_TYPE_RELAYED:
		return "relayed";
	}
	return "unknown";
}

pj_ice_cand_type CandidateStringToType(const QString& type) {
	if (type == "host") {
		return PJ_ICE_CAND_TYPE_HOST;
	} else if (type == "srflx") {
		return PJ_ICE_CAND_TYPE_SRFLX;
	} else if (type == "prflx") {
		return PJ_ICE_CAND_TYPE_PRFLX;
	} else if (type == "relayed") {
		return PJ_ICE_CAND_TYPE_RELAYED;
	}
	return PJ_ICE_CAND_TYPE_HOST;
}

void ICESession::InitialisationComplete(pj_status_t status) {
	unsigned int candidates = pj_ice_strans_get_cands_count(ice_instance_, component_id_);

	pj_ice_sess_cand* cand = new pj_ice_sess_cand[candidates];

	pj_ice_strans_enum_cands(ice_instance_, component_id_, &candidates, &cand[0]);

	pj_str_t ufrag;
	pj_str_t pwd;
	pj_ice_strans_get_ufrag_pwd(ice_instance_, &ufrag, &pwd, NULL, NULL);

	candidates_.user_fragment = QString::fromLatin1(ufrag.ptr, ufrag.slen);
	candidates_.password = QString::fromLatin1(pwd.ptr, pwd.slen);

	for (int i = 0; i < candidates; ++i) {
		if (!pj_sockaddr_has_addr(&cand[i].addr)) {
			continue;
		}

		int port = pj_sockaddr_get_port(&cand[i].addr);
		char ipaddr[PJ_INET6_ADDRSTRLEN];
		pj_sockaddr_print(&cand[i].addr, ipaddr, sizeof(ipaddr), 0);

		QHostAddress address(QString::fromLatin1(ipaddr));

		SIPInfo::Candidate candidate;
		candidate.address = address;
		candidate.port = port;
		candidate.type = CandidateTypeToString(cand[i].type);
		candidate.component = component_id_;
		candidate.priority = cand[i].prio;
		candidate.foundation = QString::fromLatin1(
			cand[i].foundation.ptr, cand[i].foundation.slen);

		candidates_.candidates << candidate;
	}


	delete cand;
	emit CandidatesAvailable(candidates_);
}


void ICESession::StartNegotiation(const SIPInfo& session) {
	pj_str_t remote_ufrag;
	pj_str_t remote_password;

	pj_cstr(&remote_ufrag, strdup(session.user_fragment.toStdString().c_str()));
	pj_cstr(&remote_password, strdup(session.password.toStdString().c_str()));

	int cand_cnt = session.candidates.size();
	pj_ice_sess_cand* candidates = new pj_ice_sess_cand[cand_cnt];
	for (int i = 0; i < session.candidates.size(); ++i) {
		const SIPInfo::Candidate c = session.candidates[i];
		pj_ice_sess_cand* candidate = new pj_ice_sess_cand;
		pj_bzero(candidate, sizeof(*candidate));

		candidate->type = CandidateStringToType(c.type);
		candidate->comp_id = c.component;
		candidate->prio = c.priority;
		int af = c.address.protocol() == QAbstractSocket::IPv6Protocol
			? pj_AF_INET6()
			: pj_AF_INET();

		pj_sockaddr_init(af, &candidate->addr, NULL, 0);
		pj_str_t temp_addr;
		pj_cstr(&temp_addr, c.address.toString().toStdString().c_str());
		pj_sockaddr_set_str_addr(af, &candidate->addr, &temp_addr);
		pj_sockaddr_set_port(&candidate->addr, c.port);

		pj_cstr(&candidate->foundation, c.foundation.toStdString().c_str());
	}

	pj_status_t status = pj_ice_strans_start_ice(
		ice_instance_,
		&remote_ufrag,
		&remote_password,
		session.candidates.size(),
		candidates);

	if (status != PJ_SUCCESS) {
		qDebug() << "Start negotation failed";
	} else {
		qDebug() << "ICE negotiation started";
	}

	delete candidates;
}

qint64 ICESession::readData(char* data, qint64 max_size) {
	QByteArray ret = receive_buffer_.left(max_size);
	receive_buffer_ = receive_buffer_.mid(ret.size());
	memcpy(data, ret.constData(), ret.size());
	return ret.size();
}

qint64 ICESession::writeData(const char* data, qint64 max_size) {
	// This address should never actually be used.
	pj_sockaddr addr;
	pj_getdefaultipinterface(pj_AF_INET(), &addr);

	pj_status_t ret = pj_ice_strans_sendto(
		ice_instance_, component_id_, data, max_size, &addr, sizeof(addr));
	return ret == PJ_SUCCESS ? max_size : -1;
}

void ICESession::OnReceiveData(pj_ice_strans* ice_st,
	unsigned comp_id,
	void* pkt,
	pj_size_t size,
	const pj_sockaddr_t* src_addr,
	unsigned src_addr_len) {
		ICESession* me = reinterpret_cast<ICESession*>(pj_ice_strans_get_user_data(ice_st));
		QByteArray data((const char*)pkt, size);
		qDebug() << "Received data" << data;

		me->receive_buffer_.append(data);
		emit me->readyRead();
}

void ICESession::OnICEComplete(pj_ice_strans* ice_st,
	pj_ice_strans_op op,
	pj_status_t status) {
		ICESession* me = reinterpret_cast<ICESession*>(pj_ice_strans_get_user_data(ice_st));
		const char* op_name = NULL;
		switch (op) {
		case PJ_ICE_STRANS_OP_INIT:
			op_name = "initialisation";
			me->InitialisationComplete(status);
			break;
		case PJ_ICE_STRANS_OP_NEGOTIATION: {
			op_name = "negotation";
			const char* data = "Hello, World!";
			pj_sockaddr addr;
			pj_getdefaultipinterface(pj_AF_INET(), &addr);
			pj_ice_strans_sendto(ice_st, me->component_id_, data, strlen(data), &addr, sizeof(addr));
			emit me->Connected();
			break;
										   }
		default:
			op_name = "unknown";
		}

		qDebug() << op_name << (status == PJ_SUCCESS ? "succeeded" : "failed");

}

int ICESession::HandleEvents(unsigned max_msec, unsigned* p_count) {
	pj_time_val max_timeout = { 0, 0 };
	max_timeout.msec = max_msec;

	pj_time_val timeout = { 0, 0 };
	int c = pj_timer_heap_poll(sIceConfig.stun_cfg.timer_heap, &timeout);
	int count = 0;
	if (c > 0) {
		count += c;
	}

	if (timeout.msec >= 1000) {
		timeout.msec = 999;
	}

	if (PJ_TIME_VAL_GT(timeout, max_timeout)) {
		timeout = max_timeout;
	}

	static const int kMaxNetEvents = 1;
	int net_event_count = 0;
	do {
		c = pj_ioqueue_poll(sIceConfig.stun_cfg.ioqueue, &timeout);
		if (c < 0) {
			pj_status_t err = pj_get_netos_error();
			pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
			if (p_count) {
				*p_count = count;
			}
			return err;
		} else if (c == 0) {
			break;
		} else {
			net_event_count += c;
			timeout.sec = 0;
			timeout.msec = 0;
		}

	} while (c > 0 && net_event_count < kMaxNetEvents);

	count += net_event_count;
	if (p_count) {
		*p_count = count;
	}

	return PJ_SUCCESS;
}

int ICESession::WorkerThread(void*) {
	forever {
		HandleEvents(500, NULL);
	}
	return 0;
}

void ICESession::PJLog(int level, const char* data, int len) {
	//qLog(Debug) << QByteArray(data, len);
}

void ICESession::StaticInit() {
	//pj_log_set_log_func(&PJLog);

	pj_init();
	pjlib_util_init();
	pjnath_init();

	pj_caching_pool_init(&sCachingPool, NULL, 0);
	pj_ice_strans_cfg_default(&sIceConfig);

	sIceConfig.stun_cfg.pf = &sCachingPool.factory;

	sPool = pj_pool_create(&sCachingPool.factory, "clementine", 512, 512, NULL);

	pj_timer_heap_create(sPool, 100, &sIceConfig.stun_cfg.timer_heap);
	pj_ioqueue_create(sPool, 16, &sIceConfig.stun_cfg.ioqueue);

	pj_thread_create(sPool, "clementine", &WorkerThread, NULL, 0, 0, &sThread);

	sIceConfig.af = pj_AF_INET();

	sIceConfig.stun.server.ptr = strdup(kStunServer);
	sIceConfig.stun.server.slen = strlen(kStunServer);
	sIceConfig.stun.port = PJ_STUN_PORT;
}

QDebug operator<< (QDebug dbg, const SIPInfo& session) {
	dbg.nospace() << session.user_fragment << ":"
		<< session.password << ":";

	const SIPInfo::Candidate& c = session.candidates[0];
	dbg.nospace() << c.address.toString() << ":"
		<< c.port << ":"
		<< c.type << ":"
		<< c.component << ":"
		<< c.priority << ":"
		<< c.foundation;
	return dbg.space();
}
