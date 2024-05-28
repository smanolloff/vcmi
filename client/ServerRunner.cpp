/*
 * ServerRunner.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "ServerRunner.h"

#include "../lib/VCMIDirs.h"
#include "../lib/CThreadHelper.h"
#include "../server/CVCMIServer.h"

#ifndef VCMI_MOBILE
#include <boost/process/child.hpp>
#include <boost/process/io.hpp>
#endif

ServerThreadRunner::ServerThreadRunner() = default;
ServerThreadRunner::~ServerThreadRunner() = default;

uint16_t ServerThreadRunner::start(uint16_t port, bool connectToLobby, std::shared_ptr<StartInfo> startingInfo)
{
	server = std::make_unique<CVCMIServer>(port, true);

	if (startingInfo)
	{
		server->si = startingInfo; //Else use default
	}

	uint16_t srvport = port;

	threadRunLocalServer = boost::thread([this, connectToLobby, &srvport]{
		setThreadName("runServer");
		srvport = server->prepare(connectToLobby);
		server->run();
	});

	while(srvport == 0) {
		logNetwork->debug("Server port not available yet");
		boost::this_thread::sleep(boost::posix_time::milliseconds(50));
		logNetwork->debug("Server port: %d", srvport);
	}

	return srvport;
}

void ServerThreadRunner::shutdown()
{
	server->setState(EServerState::SHUTDOWN);
}

void ServerThreadRunner::wait()
{
	threadRunLocalServer.join();
}

int ServerThreadRunner::exitCode()
{
	return 0;
}

#ifndef VCMI_MOBILE

ServerProcessRunner::ServerProcessRunner() = default;
ServerProcessRunner::~ServerProcessRunner() = default;

void ServerProcessRunner::shutdown()
{
	child->terminate();
}

void ServerProcessRunner::wait()
{
	child->wait();
}

int ServerProcessRunner::exitCode()
{
	return child->exit_code();
}

uint16_t ServerProcessRunner::start(uint16_t port, bool connectToLobby, std::shared_ptr<StartInfo> startingInfo)
{
	boost::filesystem::path serverPath = VCMIDirs::get().serverPath();
	boost::filesystem::path logPath = VCMIDirs::get().userLogsPath() / "server_log.txt";
	std::vector<std::string> args;
	args.push_back("--port=" + std::to_string(port));
	args.push_back("--run-by-client");
	if(connectToLobby)
		args.push_back("--lobby");

	std::error_code ec;
	child = std::make_unique<boost::process::child>(serverPath, args, ec, boost::process::std_out > logPath);

	if (ec)
		throw std::runtime_error("Failed to start server! Reason: " + ec.message());

	return port;
}

#endif
