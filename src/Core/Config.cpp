#include "Config.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "CryptoNoteConfig.hpp"
#include "common/Base64.hpp"
#include "platform/PathTools.hpp"

static void parse_peer_and_add_to_container(const std::string &str, std::vector<cryptonote::NetworkAddress> &container) {
	cryptonote::NetworkAddress na{};
	if (!common::parse_ip_address_and_port(str, na.ip, na.port))
		throw std::runtime_error("Wrong address format " + str + ", should be ip:port");
	container.push_back(na);
}

using namespace common;
using namespace cryptonote;

const static UUID CRYPTONOTE_NETWORK{{0x81, 0xE7, 0xbA, 0x93, 0xd1, 0x28, 0x15, 0xD3, 0x10, 0x32, 0x00, 0x12, 0x09, 0x11,
    0x88, 0x77}};  // Bender's nightmare

Config::Config(common::CommandLine &cmd)
    : is_testnet(cmd.get_bool("--testnet"))
    , mempool_tx_live_time(parameters::CRYPTONOTE_MEMPOOL_TX_LIVETIME)
    , blocks_file_name(parameters::CRYPTONOTE_BLOCKS_FILENAME)
    , block_indexes_file_name(parameters::CRYPTONOTE_BLOCKINDEXES_FILENAME)
    , crypto_note_name(CRYPTONOTE_NAME)
    , network_id(CRYPTONOTE_NETWORK)
    , p2p_bind_port(P2P_DEFAULT_PORT)
    , p2p_external_port(P2P_DEFAULT_PORT)
    , p2p_bind_ip("0.0.0.0")
    , cryptonoted_bind_port(RPC_DEFAULT_PORT)
    , cryptonoted_bind_ip("127.0.0.1")  // Less attack vectors from outside for ordinary uses
    , cryptonoted_remote_port(0)
    , cryptonoted_remote_ip("127.0.0.1")
    , walletd_bind_port(WALLET_RPC_DEFAULT_PORT)
    , walletd_bind_ip("127.0.0.1")  // Connection to wallet allows spending
    , p2p_local_white_list_limit(P2P_LOCAL_WHITE_PEERLIST_LIMIT)
    , p2p_local_gray_list_limit(P2P_LOCAL_GRAY_PEERLIST_LIMIT)
    , p2p_default_peers_in_handshake(P2P_DEFAULT_PEERS_IN_HANDSHAKE)
    , p2p_default_connections_count(P2P_DEFAULT_CONNECTIONS_COUNT)
    , p2p_allow_local_ip(is_testnet)
    , p2p_whitelist_connections_percent(P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT)
    , p2p_block_ids_sync_default_count(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT)
    , p2p_blocks_sync_default_count(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)
    , rpc_get_blocks_fast_max_count(COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT) {
	common::pod_from_hex(P2P_STAT_TRUSTED_PUB_KEY, trusted_public_key);

	if (is_testnet) {
		network_id.data[0] += 1;
		p2p_bind_port += 1000;
		p2p_external_port += 1000;
		cryptonoted_bind_port += 1000;
		p2p_allow_local_ip = true;
	}
	if (const char *pa = cmd.get("--p2p-bind-address")) {
		if (!common::parse_ip_address_and_port(pa, p2p_bind_ip, p2p_bind_port))
			throw std::runtime_error("Wrong address format " + std::string(pa) + ", should be ip:port");
	}
	if (const char *pa = cmd.get("--p2p-external-port"))
		p2p_external_port = boost::lexical_cast<uint16_t>(pa);
	if (const char *pa = cmd.get("--walletd-bind-address")) {
		if (!common::parse_ip_address_and_port(pa, walletd_bind_ip, walletd_bind_port))
			throw std::runtime_error("Wrong address format " + std::string(pa) + ", should be ip:port");
	}
	if (const char *pa = cmd.get("--ssl-certificate-pem-file")) {
		ssl_certificate_pem_file = pa;
#if !platform_USE_SSL
		throw std::runtime_error(
		    "Setting --ssl-certificate-pem-file impossible - this binary is built without OpenSSL");
#endif
	}
	if (const char *pa = cmd.get("--ssl-certificate-password")) {
		ssl_certificate_password = pa;
#if !platform_USE_SSL
		throw std::runtime_error(
		    "Setting --ssl_certificate_password impossible - this binary is built without OpenSSL");
#endif
	}
	if (const char *pa = cmd.get("--cryptonoted-authorization")) {
		cryptonoted_authorization = common::base64::encode(BinaryArray(pa, pa + strlen(pa)));
	}
	if (const char *pa = cmd.get("--cryptonoted-bind-address")) {
		if (!common::parse_ip_address_and_port(pa, cryptonoted_bind_ip, cryptonoted_bind_port))
			throw std::runtime_error("Wrong address format " + std::string(pa) + ", should be ip:port");
	}
	if (const char *pa = cmd.get("--cryptonoted-remote-address")) {
		std::string addr         = pa;
		const std::string prefix = "https://";
		if (addr.find(prefix) == 0) {
#if !platform_USE_SSL
			throw std::runtime_error(
			    "Using https in --cryptonoted-remote-address impossible - this binary is built without OpenSSL");
#endif
			std::string sip;
			std::string sport;
			if (!split_string(addr.substr(prefix.size()), ":", sip, sport))
				throw std::runtime_error(
				    "Wrong address format " + addr + ", should be <ip>:<port> or https://<host>:<port>");
			cryptonoted_remote_port = boost::lexical_cast<uint16_t>(sport);
			cryptonoted_remote_ip   = prefix + sip;
		} else {
			const std::string prefix2 = "http://";
			if (addr.find(prefix2) == 0)
				addr = addr.substr(prefix2.size());
			if (!common::parse_ip_address_and_port(addr, cryptonoted_remote_ip, cryptonoted_remote_port))
				throw std::runtime_error("Wrong address format " + addr + ", should be ip:port");
		}
	}
	if (cmd.get_bool("--allow-local-ip"))
		p2p_allow_local_ip = true;
	for (auto &&pa : cmd.get_array("--seed-node-address"))
		parse_peer_and_add_to_container(pa, seed_nodes);
	for (auto &&pa : cmd.get_array("--seed-node", "Use --seed-node-address instead"))
		parse_peer_and_add_to_container(pa, seed_nodes);
	for (auto &&pa : cmd.get_array("--priority-node-address"))
		parse_peer_and_add_to_container(pa, priority_nodes);
	for (auto &&pa : cmd.get_array("--add-priority-node", "Use --priority-node-address instead"))
		parse_peer_and_add_to_container(pa, priority_nodes);
	for (auto &&pa : cmd.get_array("--exclusive-node-address"))
		parse_peer_and_add_to_container(pa, exclusive_nodes);
	for (auto &&pa : cmd.get_array("--add-exclusive-node", "Use --exclusive-node-address instead"))
		parse_peer_and_add_to_container(pa, exclusive_nodes);

	if (seed_nodes.empty() && !is_testnet)
		for (auto &&sn : cryptonote::SEED_NODES) {
			NetworkAddress addr;
			if (!common::parse_ip_address_and_port(sn, addr.ip, addr.port))
				continue;
			seed_nodes.push_back(addr);
		}

	std::sort(seed_nodes.begin(), seed_nodes.end());
	std::sort(exclusive_nodes.begin(), exclusive_nodes.end());
	std::sort(priority_nodes.begin(), priority_nodes.end());

	data_folder = platform::get_app_data_folder(crypto_note_name);
	if (is_testnet)
		data_folder += "_testnet";
	if (const char *pa = cmd.get("--data-folder")) {
		data_folder = pa;
		if (!platform::directory_exists(data_folder))
			throw std::runtime_error("Data folder must exist " + data_folder);
	} else {
		if (!platform::create_directories_if_necessary(data_folder))  // Create only in default place
			throw std::runtime_error("Failed to create data folder " + data_folder);
	}
	/*	std::string file_str;
	    if (common::load_file(coin_directory + "/" + "data_folder_path.txt", file_str)) {
	        if (file_str.find(std::string("\xEF\xBB\xBF")) == 0)  // BOM is written by Notepad on Windows
	            file_str = file_str.substr(3);
	        std::vector<std::string> strs;
	        boost::algorithm::split(strs, file_str, boost::algorithm::is_any_of("\r\n"));
	        for (auto &&str : strs) {
	            boost::algorithm::trim(str);
	            boost::algorithm::trim_right_if(str, boost::algorithm::is_any_of("\\/"));
	            if (str.empty() || str.find(std::string("#")) == 0)  // Comments
	                continue;
	            std::cout << "Found coin folder via data_folder_path.txt, path=" << str << std::endl;
	            coin_directory = str;
	            break;
	        }
	    } else {
	#ifdef _WIN32
	        const char content[] =
	            "\xEF\xBB\xBF# Edit this file to switch data folder\r\n"
	            "# Uncomment line below and point it to desired blockchain location. Only full path is supported\r\n"
	            "# You should manually move content of old data folder to new location after completely stopping "
	            "cryptonote\r\n\r\n"
	            "# D:\\BlockChains\\cryptonote\r\n";
	#else
	        const char content[] =
	            "# Edit this file to switch data folder\n"
	            "# Uncomment line below and point it to desired blockchain location. Note, ~ is unsupported, use full "
	            "path\n"
	            "# You should manually move content of old data folder to new location after completely stopping "
	            "cryptonote\n\n"
	            "# /some/far/away/folder\n";
	#endif
	        common::save_file(data_folder + "/" + "data_folder_path.txt", content);
	    }*/
}

std::string Config::get_data_folder(const std::string &subdir) const {
	std::string folder = data_folder;
	// This code is called just several times at startup, so no caching
	folder += "/" + subdir;
	if (!platform::create_directory_if_necessary(folder))
		throw std::runtime_error("Failed to create coin folder " + folder);
	return folder;
}
