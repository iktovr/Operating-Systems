#pragma once

#include <zmqpp/zmqpp.hpp>
#include <string>

#define check(VAL, BADVAL, MSG) if (VAL == BADVAL) { perror(MSG); exit(1); }

const std::string host = "tcp://127.0.0.1:";

int get_port() {
	static unsigned int port(49152);
	static zmqpp::context context;
	static zmqpp::socket socket(context, zmqpp::socket_type::pull);
	while (true) {
		std::cout << "trying " << port;
		try {
			socket.bind(host + std::to_string(port));
		} catch (zmqpp::zmq_internal_exception& ex) {
			++port;
			std::cout << std::endl;
			continue;
		}
		socket.unbind(host + std::to_string(port));
		std::cout << " ok" << std::endl;
		return port++;
	}
}

int try_bind(zmqpp::socket& socket) {
	static unsigned int port(49152);
	while (true) {
		try {
		// std::cout << "trying " << port;
			socket.bind(host + std::to_string(port));
		} catch (zmqpp::zmq_internal_exception& ex) {
			++port;
			// std::cout << std::endl;
			continue;
		}
		// std::cout << " ok" << std::endl;
		return port++;
	}
}

enum class action: int {
	fork, exit, unbind_front, unbind_back, rebind_front, rebind_back, exec_set, exec_get
};