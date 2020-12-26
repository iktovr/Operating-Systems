#include <unistd.h>
#include <pthread.h>
#include <zmqpp/zmqpp.hpp>
#include <iostream>
#include <list>
#include <string>
#include <signal.h>
#include <sstream>

#include "network.hpp"

using line_t = std::pair<std::list<int>, std::pair<std::pair<zmqpp::socket, std::string>, std::pair<zmqpp::socket, std::string>>>;
std::list<line_t> network;

class node_coord {
private:
	std::pair<std::list<line_t>::iterator, std::list<int>::iterator> data;

public:
	node_coord(std::list<line_t>::iterator& i1, std::list<int>::iterator& i2) : data(i1, i2) {}
	node_coord(std::list<line_t>::iterator&& i1, std::list<int>::iterator&& i2) : data(i1, i2) {}
	node_coord(node_coord& other) : data(other.data) {}
	node_coord(node_coord&& other) : data(other.data) {}

	std::list<line_t>::iterator& line() {
		return data.first;
	}

	std::list<int>::iterator& node() {
		return data.second;
	}

	int& id() {
		return *(data.second);
	}

	int& parent() {
		// main parent
		return data.first->first.front();
	}

	zmqpp::socket& out_sock() {
		return data.first->second.first.first;
	}

	zmqpp::socket& ping_sock() {
		return data.first->second.second.first;
	}

	std::string& out_port() {
		return data.first->second.first.second;
	}

	std::string& ping_port() {
		return data.first->second.second.second;
	}
};

node_coord find_node(int id) {
	for (auto b_it = network.begin(); b_it != network.end(); ++b_it) {
		for (auto i_it = b_it->first.begin(); i_it != b_it->first.end(); ++i_it) {
			if (*i_it == id) {
				return {b_it, i_it};
			}
		}
	}
	return {network.end(), std::list<int>::iterator()};
}

void* result_waiter(void* arg) {
	zmqpp::socket *in_sock = reinterpret_cast<zmqpp::socket*>(arg);
	zmqpp::message msg;
	int rid, act;
	while (in_sock) {
		in_sock->receive(msg);
		msg >> rid >> act;

		switch (static_cast<action>(act)) {
		case action::fork: {
			int pid;
			msg >> pid;
			std::cout << "OK: " << pid << std::endl;
			break;
		}

		case action::exec_set: {
			std::cout << "OK:" << rid << std::endl;
			break;
		}

		case action::exec_get: {
			std::string name, value;
			msg >> name >> value;
			if (value == "not found") {
				std::cout << "OK:" << rid << ": '" << name << "' not found" << std::endl;
			} else {
				std::cout << "OK:" << rid << ": " << value << std::endl;
			}
			break;
		}

		case action::rebind_back: {
			node_coord node = find_node(rid);
			node.out_sock().disconnect(host + node.out_port());
			msg >> node.out_port();
			node.out_sock().connect(host + node.out_port());

			node.ping_sock().disconnect(host + node.ping_port());
			msg >> node.ping_port();
			node.ping_sock().connect(host + node.ping_port());
			break;
		}

		case action::exit: {
			node_coord node = find_node(rid);
			node.line()->first.erase(node.node());
			if (node.line()->first.empty()) {
				network.erase(node.line());
			}
		}

		default: {}

		}
	}
	return NULL;
}

zmqpp::context context;

bool ping(zmqpp::socket& ping_sock, std::string& ping_port, int id) {
	int packet[2] = {id, 1};
	size_t length = 2 * sizeof(int);
	if (!ping_sock.send_raw(reinterpret_cast<char *>(packet), length, zmqpp::socket::dont_wait)) {
		return false;
	}
	if (!ping_sock.receive_raw(reinterpret_cast<char *>(packet), length)) {
		ping_sock.close();
		ping_sock = zmqpp::socket(context, zmqpp::socket_type::req);
		ping_sock.set(zmqpp::socket_option::receive_timeout, 1000);
		ping_sock.connect(host + ping_port);
		return false;
	}
	return (packet[1] == 1) ? true : false;
}

int main() {
	// std::cout << getpid() << std::endl;


	zmqpp::socket in_sock(context, zmqpp::socket_type::pull);
	std::string in_port = std::to_string(try_bind(in_sock));

	zmqpp::socket bridge(context, zmqpp::socket_type::rep);
	std::string bridge_port = std::to_string(try_bind(bridge));

	pthread_t result_waiter_id;
	check(pthread_create(&result_waiter_id, NULL, result_waiter, (void*)&in_sock), -1, "pthread_create error");
	if (pthread_detach(result_waiter_id) != 0) {
		perror("detach error");
	}

	std::string command;
	std::cout << "> ";
	while (std::cin >> command && command != "exit") {
		if (command == "create") {
			int id, parent;
			std::cin >> id >> parent;

			if (id == -1 || find_node(id).line() != network.end()) {
				std::cerr << "Error: Already exists" << std::endl;
				continue;
			}

			if (parent == -1) {
				network.push_back({std::list<int>(), std::make_pair(std::make_pair(zmqpp::socket(context, zmqpp::socket_type::push), ""), std::make_pair(zmqpp::socket(context, zmqpp::socket_type::req), ""))});
				network.back().first.push_back(id);
				node_coord node(--network.end(), --network.back().first.end());

				int pid = fork();
				check(id, -1, "fork error");
				if (pid == 0) {
					check(execl("node", "node", std::to_string(id).c_str(), std::to_string(-1).c_str(),
								bridge_port.c_str(), NULL), -1, "execl error")
				}

				zmqpp::message ports;
				bridge.receive(ports);

				ports >> node.out_port();
				node.out_sock().connect(host + node.out_port());

				ports >> node.ping_port();
				node.ping_sock().connect(host + node.ping_port());
				node.ping_sock().set(zmqpp::socket_option::receive_timeout, 1000);
				
				ports.pop_back();
				ports.pop_back();
				ports << in_port;
				bridge.send(ports);
			}

			else {
				node_coord parent_node = find_node(parent);
				if (parent_node.line() == network.end()) {
					std::cerr << "Error: Parent not found" << std::endl;
					continue;
				}

				if (!ping(parent_node.ping_sock(), parent_node.ping_port(), parent)) {
					std::cerr << "Error: Parent is unavailable" << std::endl;
					continue;
				}

				parent_node.line()->first.insert(++parent_node.node(), id);

				zmqpp::message msg;
				msg << parent << static_cast<int>(action::fork) << id;
				parent_node.out_sock().send(msg);
			}
		}

		else if (command == "remove") {
			int id;
			std::cin >> id;

			node_coord node = find_node(id);

			if (node.line() == network.end()) {
				std::cerr << "Error: Not found" << std::endl;
				continue;
			}

			if (!ping(node.ping_sock(), node.ping_port(), id)) {
				std::cerr << "Error: Node is unavailable" << std::endl;
				continue;
			}

			zmqpp::message msg;
			msg << id << static_cast<int>(action::exit);
			node.out_sock().send(msg);
		}

		else if (command == "exec") {
			int id;
			std::cin >> id;

			node_coord node = find_node(id);

			if (node.line() == network.end()) {
				std::cerr << "Error: Not found" << std::endl;
				continue;
			}

			if (!ping(node.ping_sock(), node.ping_port(), id)) {
				std::cerr << "Error: Node is unavailable" << std::endl;
				continue;
			}

			std::string line;
			std::getline(std::cin, line);
			std::istringstream is(line);
			std::string name, value;
			is >> name >> value;

			zmqpp::message msg;

			msg << id;
			if (value != "") {
				msg << static_cast<int>(action::exec_set) << name << stoi(value);
			} else {
				msg << static_cast<int>(action::exec_get) << name;
			}

			node.out_sock().send(msg);
		}

		else if (command == "pingall") {
			std::list<int> unavailable;
			bool dead;

			for (auto& line: network) {
				zmqpp::socket &ping_node = line.second.second.first;
				std::string &ping_port = line.second.second.second;
				dead = false;
				for (int& node: line.first) {
					if (dead) {
						unavailable.push_back(node);
					} else if (!ping(ping_node, ping_port, node)) {
						dead = true;
						unavailable.push_back(node);
					}
				}
			}

			std::cout << "OK: ";
			if (unavailable.empty()) {
				std::cout << -1 << std::endl;
			} else {
				std::cout << *unavailable.begin();
				for (auto node = ++unavailable.begin(); node != unavailable.end(); ++node) {
					std::cout << "; " << *node;
				}
				std::cout << std::endl;
			}
		}


		std::cout << "> ";
	}

	// for (auto& line: network) {
	// 	zmqpp::socket &out_node = line.second.first.first;
	// 	std::string &out_port = line.second.first.second;
	// 	zmqpp::socket &ping_node = line.second.second.first;
	// 	std::string &ping_port = line.second.second.second;
	// 	for (int& node: line.first) {
	// 		if (ping(ping_node, ping_port, node)) {
	// 			zmqpp::message
	// 		}
	// 	}
	// }
}

// topology - 1 - lists
// commands - 2 - exec id name value (exec id name)
// ping     - 1 - pingall