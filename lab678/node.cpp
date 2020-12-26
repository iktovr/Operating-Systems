#include <unistd.h>
#include <pthread.h>
#include <zmqpp/zmqpp.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include <signal.h>
#include <map>

#include "network.hpp"

/*
front_ping-><-\    /-><-back_ping
               \  /
    front_in->\ \/ /<-back_in
               node
   front_out-</    \>-back_out
*/

int id = -1, next_id = -1, prev_id = -1;
std::unique_ptr<zmqpp::socket> front_in(nullptr), front_out(nullptr), front_ping(nullptr),
                               back_in(nullptr), back_out(nullptr), back_ping(nullptr);
std::string back_in_port, back_out_port, back_ping_port, front_ping_port;
zmqpp::context context;

void* ping(void*) {
	int packet[2];
	size_t length = 2 * sizeof(int);
	while (*front_ping) {
		front_ping->receive_raw(reinterpret_cast<char *>(packet), length);
		// std::cout << "received ping " << packet[0] << " " << packet[1] << std::endl;
		if (packet[0] != id) {
			// std::cout << "sending forward" << std::endl;
			if (back_ping.get() != nullptr) {
				if (!back_ping->send_raw(reinterpret_cast<char *>(packet), length, zmqpp::socket::dont_wait)) {
					packet[1] = 0;
				} else if (!back_ping->receive_raw(reinterpret_cast<char *>(packet), length)) {
					packet[1] = 0;
					back_ping->close();
					back_ping.reset(new zmqpp::socket(context, zmqpp::socket_type::req));
					back_ping->set(zmqpp::socket_option::receive_timeout, 1000);
					back_ping->connect(host + back_ping_port);
				}
			} else {
				front_ping->close();
				front_ping.reset(new zmqpp::socket(context, zmqpp::socket_type::rep));
				front_ping->bind(host + front_ping_port);
				continue;
			}
		}
		// std::cout << "sending back" << std::endl;
		front_ping->send_raw(reinterpret_cast<char *>(packet), length);
	}
	return NULL;
}

void* back_to_front(void*) {
	zmqpp::message msg;
	while (back_in.get() == nullptr || *back_in) {
		if (back_in.get() == nullptr) {
			continue;
		}
		back_in->receive(msg);
		// std::cout << "sending back" << std::endl;
		int rid;
		msg >> rid;
		if (rid != id) {
			front_out->send(msg);
			continue;
		}

		int act;
		msg >> act;
		switch (static_cast<action>(act)) {
		case action::rebind_back: {
			msg >> next_id;
			back_out->disconnect(host + back_out_port);
			msg >> back_out_port;
			back_out->connect(host + back_out_port);

			back_in->unbind(host + back_in_port);
			msg >> back_in_port;
			back_in->bind(host + back_in_port);

			back_ping->disconnect(host + back_ping_port);
			msg >> back_ping_port;
			back_ping->connect(host + back_ping_port);
			break;
		}

		case action::unbind_back: {
			back_in.reset(nullptr);
			back_out.reset(nullptr);
			back_ping.reset(nullptr);
			next_id = -1;
			break;
		}

		default: {}
		}
	}
	return NULL;
}
	
int main(int argc, char* argv[]) {
	assert(argc >= 4);
	id = strtoul(argv[1], nullptr, 10);
	prev_id = strtoul(argv[2], nullptr, 10);
	std::string bridge_port(argv[3]);
	// std::cout << getpid() << " " << id << std::endl;

	zmqpp::message msg;

	zmqpp::socket bridge(context, zmqpp::socket_type::req);
	bridge.connect(host + bridge_port);

	front_in.reset(new zmqpp::socket(context, zmqpp::socket_type::pull));
	std::string front_in_port = std::to_string(try_bind(*front_in));

	front_ping.reset(new zmqpp::socket(context, zmqpp::socket_type::rep));
	front_ping_port = std::to_string(try_bind(*front_ping));

	msg << front_in_port << front_ping_port;
	bridge.send(msg);

	bridge.receive(msg);

	std::string front_out_port;
	msg >> front_out_port;
	front_out.reset(new zmqpp::socket(context, zmqpp::socket_type::push));
	front_out->connect(host + front_out_port);

	bridge.disconnect(host + bridge_port);
	bridge.close();
	bridge = zmqpp::socket(context, zmqpp::socket_type::rep);
	bridge_port = std::to_string(try_bind(bridge));

	{
		zmqpp::message ans;
		ans << id << static_cast<int>(action::fork) << getpid();
		front_out->send(ans);
	}

	pthread_t ping_id;
	check(pthread_create(&ping_id, NULL, ping, NULL), -1, "pthread_create error");
	if (pthread_detach(ping_id) != 0) {
		perror("detach error");
	}

	pthread_t back_to_front_id;
	check(pthread_create(&back_to_front_id, NULL, back_to_front, NULL), -1, "pthread_create error");
	if (pthread_detach(back_to_front_id) != 0) {
		perror("detach error");
	}

	std::map<std::string, int> dictionary;

	int act;
	int tid;

	while (true) {
		if (!front_in->receive(msg)) {
			perror("");
		}

		msg >> tid;

		if (tid != id) {
			// std::cout << "sending forward" << std::endl;
			back_out->send(msg);
			continue;
		}

		msg >> act;

		switch (static_cast<action>(act)) {
		case action::fork: {
			int cid;
			msg >> cid;

			if (next_id == -1) {
				next_id = cid;

				int pid = fork();
				check(pid, -1, "fork error");
				if (pid == 0) {
					check(execl("node", "node", std::to_string(cid).c_str(), std::to_string(id).c_str(),
								bridge_port.c_str(), NULL), -1, "execl error");
				}

				back_in.reset(new zmqpp::socket(context, zmqpp::socket_type::pull));
				back_in_port = std::to_string(try_bind(*back_in));

				zmqpp::message ports;
				bridge.receive(ports);

				ports >> back_out_port >> back_ping_port;
				ports.pop_back();
				ports.pop_back();

				back_out.reset(new zmqpp::socket(context, zmqpp::socket_type::push));
				back_ping.reset(new zmqpp::socket(context, zmqpp::socket_type::req));
				back_ping->set(zmqpp::socket_option::receive_timeout, 1000);

				back_out->connect(host + back_out_port);
				back_ping->connect(host + back_ping_port);

				ports << back_in_port;
				bridge.send(ports);
			}
			break;
		}

		case action::exec_set: {
			std::string name;
			int value;
			msg >> name >> value;
			dictionary[name] = value;

			zmqpp::message ans;
			ans << id << static_cast<int>(action::exec_set);
			front_out->send(ans);
			break;
		}

		case action::exec_get: {
			std::string name;
			msg >> name;

			zmqpp::message ans;
			ans << id << static_cast<int>(action::exec_get) << name;
			auto it = dictionary.find(name);
			if (it != dictionary.end()) {
				ans << std::to_string(it->second);
			} else {
				ans << "not found";
			}
			front_out->send(ans);
			break;
		}

		case action::exit: {
			back_in->unbind(host + back_in_port);


			if (prev_id == -1) {
				if (next_id != -1) {
					zmqpp::message msg2;
					msg2 << id << static_cast<int>(action::rebind_back) << back_in_port << back_ping_port;
					front_out->send(msg2);

					zmqpp::message msg3;
					msg3 << next_id << static_cast<int>(action::rebind_front) << front_out_port;
					back_out->send(msg3);
				} else {	
					zmqpp::message msg;
					msg << id << static_cast<int>(action::exit);
					front_out->send(msg);
				}


			} else {
				zmqpp::message msg;
				msg << id << static_cast<int>(action::exit);
				front_out->send(msg);
				
				if (next_id == -1) {
					zmqpp::message msg2;
					msg2 << prev_id << static_cast<int>(action::unbind_back);
					front_out->send(msg2);
				} else {
					zmqpp::message msg2;
					msg2 << prev_id << static_cast<int>(action::rebind_back) << next_id << back_in_port << back_out_port << back_ping_port;
					front_out->send(msg2);

					zmqpp::message msg3;
					msg3 << next_id << static_cast<int>(action::rebind_front) << front_out_port;
					back_out->send(msg3);
				}
			}

			return 0;
		}

		case action::rebind_front: {
			std::string tmp;
			msg << tmp;
			front_out->connect(host + tmp);
			front_out->disconnect(host + front_out_port);
			front_out_port = tmp;
		}

		default: {}
		}
	}

}