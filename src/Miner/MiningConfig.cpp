// Copyright (c) 2012-2018, The CryptoNote developers, The Bytecoin developers, [ ] developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "MiningConfig.hpp"
#include "common/CommandLine.hpp"
#include "common/Ipv4Address.hpp"

#include <iostream>
#include <thread>

#include "CryptoNoteConfig.hpp"
#include "logging/ILogger.hpp"

using namespace cryptonote;

MiningConfig::MiningConfig(common::CommandLine & cmd) :
		cryptonoted_ip("127.0.0.1"), cryptonoted_port(RPC_DEFAULT_PORT),
		thread_count(std::thread::hardware_concurrency()) {
	if (const char *pa = cmd.get("--address"))
		mining_address = pa;
	if (const char *pa = cmd.get("--cryptonoted-address")){
		if (!common::parse_ip_address_and_port(cryptonoted_ip, cryptonoted_port, pa))
			throw std::runtime_error("Wrong address format " + std::string(pa) + ", should be ip:port");
	}
	if (const char *pa = cmd.get("--daemon-address", "Use --cryptonoted-address instead")){
		if (!common::parse_ip_address_and_port(cryptonoted_ip, cryptonoted_port, pa))
			throw std::runtime_error("Wrong address format " + std::string(pa) + ", should be ip:port");
	}
	if (const char *pa = cmd.get("--daemon-host", "Use --cryptonoted-address instead"))
		cryptonoted_ip = pa;
	if (const char *pa = cmd.get("--daemon-rpc-port", "Use --cryptonoted-address instead"))
		cryptonoted_port = boost::lexical_cast<uint16_t>(pa);
	if (const char *pa = cmd.get("--threads"))
		thread_count = boost::lexical_cast<size_t>(pa);
	if(const char *pa = cmd.get("--limit"))
		blocksLimit = boost::lexical_cast<size_t>(pa);
}

