#include "common/CommandLine.hpp"
#include "common/ConsoleTools.hpp"
#include "Core/Node.hpp"
#include "Core/WalletNode.hpp"
#include "Core/Config.hpp"
#include "platform/ExclusiveLock.hpp"
#include "logging/LoggerManager.hpp"
#include "platform/Network.hpp"
#include "version.hpp"
#include <future>
#include <boost/algorithm/string.hpp>

using namespace cryptonote;

static const char USAGE[] =
R"(walletd.

Usage:
  walletd [options] --wallet-file=<file> | --export-blocks=<directory>
  walletd --help | -h
  walletd --version | -v

Options:
  --wallet-file=<file>                 Path to wallet file to open.
  --wallet-password=<password>         DEPRECATED. Password to decrypt wallet file. If not specified, will prompt user.
  --generate-wallet                    Create wallet file with new random keys. Must be used with --wallet-file option.
  --set-password=<password>            DEPRECATED. Reencrypt wallet file with the new password.
  --set-password                       Prompt user for new password and reencrypt wallet file.

  --allow-local-ip                     Allow local ip add to peer list, mostly in debug purposes.
  --export-blocks=<directory>          Export blockchain into specified directory as blocks.bin and blockindexes.bin, then exit. This overwrites existing files.
  --export-view-only=<file>            Export view-only version of wallet file with the same password, then exit.
  --hide-my-port                       DEPRECATED. Do not announce yourself as peer list candidate. Use --p2p-external-port=0 instead.
  --testnet                            Configure for testnet.
  --p2p-bind-address=<ip:port>         Interface and port for P2P network protocol [default: 0.0.0.0:8080].
  --p2p-external-port=<port>           External port for P2P network protocol, if port forwarding used with NAT [default: 8080].
  --walletd-bind-address=<ip:port>     Interface and port for walletd RPC [default: 127.0.0.1:8070].
  --cryptonoted-bind-address=<ip:port>   Interface and port for cryptonoted RPC [default: 0.0.0.0:8081].
  --cryptonoted-remote-address=<ip:port> Connect to remote cryptonoted and suppress running built-in cryptonoted.
  --seed-node-address=<ip:port>        Specify list (one or more) of nodes to start connecting to.
  --priority-node-address=<ip:port>    Specify list (one or more) of nodes to connect to and attempt to keep the connection open.
  --exclusive-node-address=<ip:port>   Specify list (one or more) of nodes to connect to only. All other nodes including seed nodes will be ignored.

  --help, -h                           Show this screen.
  --version, -v                        Show version.
)";

static const bool separate_thread_for_cryptonoted = true;

int main(int argc, const char *argv[]) {
	common::console::UnicodeConsoleSetup console_setup;
	auto idea_start  = std::chrono::high_resolution_clock::now();
	common::CommandLine cmd(argc, argv);
	std::string wallet_file, password, new_password, export_view_only;
	bool set_password = false;
	bool password_in_args = true;
	bool new_password_in_args = true;
	bool generate_wallet = false;
	if (const char *pa = cmd.get("--wallet-file"))
		wallet_file = pa;
	if (const char *pa = cmd.get("--container-file", "Use --wallet-file instead"))
		wallet_file = pa;
	if (cmd.get_bool("--generate-wallet"))
		generate_wallet = true;
	if (cmd.get_bool("--generate-container", "Use --generate-wallet instead"))
		generate_wallet = true;
	if (const char *pa = cmd.get("--export-view-only"))
		export_view_only = pa;
	if (const char *pa = cmd.get("--wallet-password")) {
		password = pa;
		if (generate_wallet) {
			std::cout << "When generating wallet, use --set-password=<pass> argument for password" << std::endl;
			return api::WALLETD_WRONG_ARGS;
		}
	}
	else if (const char *pa = cmd.get("--container-password", "Use --wallet-password instead"))
		password = pa;
	else
		password_in_args = false;
	if (cmd.get_type("--set-password") == typeid(bool)) {
		cmd.get_bool("--set-password"); // mark option as used
		new_password_in_args = false;
		set_password = true;
	} else if (const char *pa = cmd.get("--set-password")) {
		new_password = pa;
		set_password = true;
	} else
		new_password_in_args = false;

	std::string export_blocks;
	if (const char *pa = cmd.get("--export-blocks"))
		export_blocks = pa;
	cryptonote::Config config(cmd);
	cryptonote::Currency currency(config.is_testnet);

	if (cmd.should_quit(USAGE, cryptonote::app_version()))
		return 0;
	logging::LoggerManager logManagerNode;
	logManagerNode.configure_default(config.get_coin_directory("logs"), "cryptonoted-");

	if (!export_blocks.empty()) {
		BlockChainState export_block_chain(logManagerNode, config, currency);
		if (!LegacyBlockChainWriter::export_blockchain2(export_blocks, export_block_chain))
			return 1;
		return 0;
	}

	if (wallet_file.empty()){
		std::cout << "--wallet-file=<file> argument is mandatory" << std::endl;
		return api::WALLETD_WRONG_ARGS;
	}
	if (!generate_wallet && !password_in_args) {
		std::cout << "Enter current wallet password: " << std::flush;
		std::getline(std::cin, password);
		boost::algorithm::trim(password);
	}
	if ((generate_wallet || set_password) && !new_password_in_args) {
		std::cout << "Enter new wallet password: " << std::flush;
		std::getline(std::cin, new_password);
		boost::algorithm::trim(new_password);
		std::cout << "Repeat new wallet password:" << std::flush;
		std::string new_password2;
		std::getline(std::cin, new_password2);
		boost::algorithm::trim(new_password2);
		if (new_password != new_password2) {
			std::cout << "New passwords do not match" << std::endl;
			return api::WALLETD_WRONG_ARGS;
		}
	}
	const std::string coinFolder = config.get_coin_directory();
//	if (wallet_file.empty() && !generate_wallet) // No args can be provided when debugging with MSVC
//		wallet_file = "C:\\Users\\user\\test.wallet";

	std::unique_ptr<platform::ExclusiveLock> blockchain_lock;
	std::unique_ptr<platform::ExclusiveLock> walletcache_lock;
	std::unique_ptr<Wallet> wallet;
	try {
		if (!config.cryptonoted_remote_port)
			blockchain_lock = std::make_unique<platform::ExclusiveLock>(coinFolder, "cryptonoted.lock");
	} catch (const platform::ExclusiveLock::FailedToLock & ex) {
		std::cout << ex.what() << std::endl;
		return api::CRYPTONOTED_ALREADY_RUNNING;
	}
	try {
		wallet = std::make_unique<Wallet>(wallet_file, generate_wallet ? new_password : password, generate_wallet);
		walletcache_lock = std::make_unique<platform::ExclusiveLock>(config.get_coin_directory("wallet_cache"), wallet->get_cache_name() + ".lock");
	} catch (const std::ios_base::failure & ex) {
		std::cout << ex.what() << std::endl;
		return api::WALLET_FILE_READ_ERROR;
	} catch (const platform::ExclusiveLock::FailedToLock & ex) {
		std::cout << ex.what() << std::endl;
		return api::WALLET_WITH_THE_SAME_VIEWKEY_IN_USE;
	} catch (const Wallet::Exception & ex) {
		std::cout << ex.what() << std::endl;
		return ex.return_code;
	}
	try {
		if (set_password)
			wallet->set_password(new_password);
		if (!export_view_only.empty()){
			wallet->export_view_only(export_view_only);
			return 0;
		}
	} catch (const std::ios_base::failure & ex) {
		std::cout << ex.what() << std::endl;
		return api::WALLET_FILE_WRITE_ERROR;
	} catch (const Wallet::Exception & ex) {
		std::cout << ex.what() << std::endl;
		return ex.return_code;
	}
	logging::LoggerManager logManagerWalletNode;
	logManagerWalletNode.configure_default(config.get_coin_directory("logs"), "walletd-");

	WalletState wallet_state(*wallet, logManagerWalletNode, config, currency);
	boost::asio::io_service io;
	platform::EventLoop run_loop(io);

	std::unique_ptr<BlockChainState> block_chain;
	std::unique_ptr<Node> node;

	std::promise<void> prm;
	std::thread cryptonoted_thread;
	if (!config.cryptonoted_remote_port) {
		try {
			if (separate_thread_for_cryptonoted) {
				cryptonoted_thread = std::thread([&prm, &logManagerNode, &config, &currency] {
					boost::asio::io_service io;
					platform::EventLoop separate_run_loop(io);

					std::unique_ptr<BlockChainState> separate_block_chain;
					std::unique_ptr<Node> separate_node;
					try {
						separate_block_chain = std::make_unique<BlockChainState>(logManagerNode, config, currency);
						separate_node = std::make_unique<Node>(logManagerNode, config, *separate_block_chain);
						prm.set_value();
					} catch (...) {
						prm.set_exception(std::current_exception());
						return;
					}
					while (!io.stopped()) {
						if (separate_node->on_idle()) // We load blockchain there
							io.poll();
						else
							io.run_one();
					}
				});
				std::future<void> fut = prm.get_future();
				fut.wait(); // propagates thread exception from here
			} else {
				block_chain = std::make_unique<BlockChainState>(logManagerNode, config, currency);
				node = std::make_unique<Node>(logManagerNode, config, *block_chain);
			}
		} catch (const boost::system::system_error & ex) {
			std::cout << ex.what() << std::endl;
			if (cryptonoted_thread.joinable())
				cryptonoted_thread.join(); // otherwise terminate will be called in ~thread
			return api::CRYPTONOTED_BIND_PORT_IN_USE;
		}
	}

	std::unique_ptr<WalletNode> wallet_node;
	try {
		wallet_node = std::make_unique<WalletNode>(nullptr, logManagerWalletNode, config, wallet_state);
	} catch (const boost::system::system_error & ex) {
		std::cout << ex.what() << std::endl;
		return api::WALLETD_BIND_PORT_IN_USE;
	}

	auto idea_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - idea_start);
	std::cout << "walletd started seconds=" << double(idea_ms.count()) / 1000 << std::endl;

	while (!io.stopped()) {
		if (node && node->on_idle()) // We load blockchain there
			io.poll();
		else
			io.run_one();
	}
	return 0;
}
