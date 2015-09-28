/** @file PixFitInstanceConfig.cxx
 *
 *  Created on: Jul 14, 2014
 *      Author: mkretz
 *      Author: K. Potamianos <karolos.potamianos@cern.ch>
 *      Update: * 2014-VII-30: adding loading of fitFarmInstance from connectivity
 */

#include <string>
#include <map>
#include <vector>
#include <algorithm> // for sorting blacklist
#include <cassert>
#include <memory>

/* Includes for getting RAM size and IP addresses. */
#include <sys/sysinfo.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include <boost/thread.hpp> // blacklist
#include <ers/ers.h>

#include "Config/Config.h"

#include "PixDbInterface/PixDbInterface.h"
#include "PixDbServer/PixDbServerInterface.h"
#include "PixUtilities/PixISManager.h"

#include "PixConnectivity/PixConnectivity.h"
#include "PixConnectivity/PartitionConnectivity.h"
#include "PixConnectivity/RodCrateConnectivity.h"
#include "PixConnectivity/RodBocConnectivity.h"


#include "PixFitInstanceConfig.h"
#include "PixFitScanConfig.h"

using namespace PixLib;

/* Initializing static const members of PixFitInstanceConfig class. */
const std::string PixFitInstanceConfig::slaveEmuRootFile =
		"/daq/db/scan-cfg/ANALOG_TEST/Base_I4/ANALOG_TEST_Base_I4_1398172938.root";
const std::string PixFitInstanceConfig::fitServerVersion = "0.2";

PixFitInstanceConfig::PixFitInstanceConfig(std::string serverName,
		std::string partitionName, std::string instanceID, bool slaveEmu)
{
	this->serverName = serverName;
	this->partitionName = partitionName;
	this->instanceID = instanceID;
	this->slaveEmu = slaveEmu;

	this->rodNetworkInterfaces.push_back("eth0"); //TODO: dynamically get the list of ROD interfaces
}

PixFitInstanceConfig::~PixFitInstanceConfig() {

}

int PixFitInstanceConfig::getThreadingSettings() {
	return boost::thread::hardware_concurrency();
}

std::map<std::string, std::string> PixFitInstanceConfig::getLocalIps() {
	std::map<std::string, std::string> Ips;

	ifaddrs *ifAddrs;
	ifaddrs *ifI;
	sockaddr_in *sockAddr;

	getifaddrs(&ifAddrs);
	/* Iterate over interfaces and collect interface names and IP addresses as strings. */
	for (ifI = ifAddrs; ifI != NULL; ifI = ifI->ifa_next) {
		if (ifI->ifa_addr->sa_family == AF_INET) {
			sockAddr = reinterpret_cast<sockaddr_in *>(ifI->ifa_addr);
			Ips[ifI->ifa_name] = inet_ntoa(sockAddr->sin_addr);
		}
	}

	freeifaddrs(ifAddrs);
	return Ips;
}

unsigned long PixFitInstanceConfig::getRam() {
	struct sysinfo info;
	sysinfo(&info);
	return (size_t) info.totalram * (size_t) info.mem_unit / (1024 * 1024);
}

std::map<std::string, std::string> PixFitInstanceConfig::getConfiguration() {
	std::map<std::string, std::string> config;

#if 0
	std::string ipcPartitionName = "PixelInfr"; if(getenv("TDAQ_PARTITION_INFRASTRUCTURE")) ipcPartitionName = getenv("TDAQ_PARTITION_INFRASTRUCTURE");
	// Todo: get other values from environment
	std::string dbServerName     = "PixelDbServer";
	std::string isServerName     = "PixelRunParams";
	IPCPartition *ipcPartition;
	PixDbServerInterface *dbServer;
	PixISManager* isManager;

	//  Create IPCPartition constructors
	ERS_LOG("Connecting to partition " << ipcPartitionName)
	try {
		ipcPartition = new IPCPartition(ipcPartitionName);
	} catch(...) {
		ERS_INFO("PixDbServer: ERROR!! problems while connecting to the partition!")
	}
	bool ready=false;
	int nTry=0;
	ERS_LOG("Trying to connect to DbServer with name " << dbServerName << "  ..... ")
	do {
		sleep(1);
		dbServer=new  PixDbServerInterface(ipcPartition,dbServerName,PixDbServerInterface::CLIENT, ready);
		if(!ready) delete dbServer;
		else break;
		std::cout << " ..... attempt number: " << nTry << std::endl;
		nTry++;
	} while(nTry<10);
	if(!ready) {
		ERS_INFO("Impossible to connect to DbServer " << dbServerName)
		exit(-1);
	}
	ERS_LOG("Succesfully connected to DbServer " << dbServerName)

	dbServer->ready();

	try {
		isManager = new PixISManager(ipcPartition, isServerName);
	}
	catch(PixISManagerExc e) {
		ERS_INFO("Exception caught in " << __PRETTY_FUNCTION__ << ": " << e.getErrorLevel() <<
				"-" << e.getISmanName() << "-" << e.getExcName())
		return config;
	}
	// @todo: decide whether we want a catch all
	catch(...) {
		ERS_INFO("Unknown exception caught in " << __PRETTY_FUNCTION__ <<
				" while creating ISManager for IsServer " << isServerName)
		return config;
	}
	ERS_LOG("Succesfully connected to IsManager " << isServerName)

	// Todo: make sure these are properly made available in IS, or get them from another source!
#if 0
	std::string idTag = isManager->read<std::string>("DataTakingConfig-IdTag");
	std::string connTag = isManager->read<std::string>("DataTakingConfig-ConnTag");
#else
	std::string idTag = "PIXEL14";
	std::string connTag = "PIT-ALL-RUN2";
#endif
	PixConnectivity conn(dbServer, "Base", "Base", "Base", "Base", idTag);
	conn.clearConn();
	conn.setConnTag(connTag, connTag, connTag, idTag);
	conn.loadConn();

	std::string domainName = "Connectivity-" + idTag;

	std::map<std::string, PartitionConnectivity*>::iterator it_partConn;
	std::map<std::string, RodCrateConnectivity*>::iterator it_rodCrateConn;
	std::map<int, RodBocConnectivity*>::iterator it_rodBocConn;

	for(it_partConn = conn.parts.begin(); it_partConn != conn.parts.end(); ++it_partConn) {
		for(it_rodCrateConn = it_partConn->second->rodCrates().begin(); it_rodCrateConn != it_partConn->second->rodCrates().end(); ++it_rodCrateConn) {
			for(it_rodBocConn = it_rodCrateConn->second->rodBocs().begin(); it_rodBocConn != it_rodCrateConn->second->rodBocs().end(); ++it_rodBocConn) {
				std::string rodBocName = it_rodBocConn->second->name();
				std::string fitFarmInstance;
				PixLib::Config conf(rodBocName, "RODBOC");
				conf.addGroup("Config");
				conf["Config"].addString("fitFarmInstance", fitFarmInstance, "FitServer-NONE", "Fitting farm instance", true);
				conf.read(dbServer, domainName, connTag, rodBocName);
				std::cout << fitFarmInstance << std::endl;
			}
		}
	}
#endif

	/* TODO: get these values from IS! */
	config["ROD_C2_S9"] = "FitServer-1";
	config["ROD_C2_S16"] = "FitServer-1";
	//config["ROD_C0_S2"] = "FitServer-1";
	//config["ROD_C2_S0"] = "FitServer-1";
	//config["ROD_C2_S15"] = "FitServer-1";
	//config["ROD_C1_S0"] = "FitServer-2";
	config["ROD_I1_S6"] = "FitServer-1";
	config["ROD_I1_S7"] = "FitServer-1";
	config["ROD_I1_S8"] = "FitServer-1";
	config["ROD_I1_S9"] = "FitServer-1";
	config["ROD_I1_S10"] = "FitServer-1";
	config["ROD_I1_S11"] = "FitServer-1";
	config["ROD_I1_S12"] = "FitServer-1";
	config["ROD_I1_S14"] = "FitServer-1";
	config["ROD_I1_S15"] = "FitServer-1";
	config["ROD_I1_S16"] = "FitServer-1";
	config["ROD_I1_S17"] = "FitServer-1";
	config["ROD_I1_S18"] = "FitServer-1";
	config["ROD_I1_S19"] = "FitServer-1";
	config["ROD_I1_S20"] = "FitServer-1";
	config["ROD_I1_S21"] = "FitServer-1";

	return config;
}

bool PixFitInstanceConfig::usingSlaveEmu() const {
	return slaveEmu;
}

std::string PixFitInstanceConfig::getInstanceId() const {
	return instanceID;
}

const std::string& PixFitInstanceConfig::getPartitionName() const {
	return partitionName;
}

const std::vector<std::string>& PixFitInstanceConfig::getRodNetworkInterfaces() const {
	return rodNetworkInterfaces;
}

const std::string& PixFitInstanceConfig::getServerName() const {
	return serverName;
}

/* BlackList implementation. */

BlackList::BlackList() {
}

BlackList::~BlackList() {
}

bool BlackList::isScanIdListed(PixFitScanConfig::ScanIdType scanId) {
	/* Get read access to vector. */
	boost::shared_lock<boost::shared_mutex> lock(m_mutex);

	/* Search for scan ID. Assume that the vector is sorted! */
	return (std::binary_search(m_blackList.begin(), m_blackList.end(), scanId));
}

bool BlackList::investigateWorkObject(std::shared_ptr<const PixFitWorkPackage> workPackage) {
	return isScanIdListed(workPackage->getScanId());
}

void BlackList::addScanId(PixFitScanConfig::ScanIdType scanId) {
	/* Get write access to vector. */
	boost::unique_lock<boost::shared_mutex> lock(m_mutex);

	/* Insert scan ID. */
	m_blackList.push_back(scanId);

	/* Sort vector. */
	std::sort(m_blackList.begin(), m_blackList.end());

	ERS_LOG("Added scan with ID " << scanId << " to blacklist, now contains "
			<< m_blackList.size() << " items.")
}


/* ScanList implementation. */

ScanList::ScanList() {
}

ScanList::~ScanList() {
}

void ScanList::setScanCount(int fitFarmId, int scanCount) {
	/* Get lock on map. */
	boost::unique_lock<boost::shared_mutex> lock(m_mutex);

	/* Element should not exist. */
	assert(m_scanMap.count(fitFarmId) == 0);

	m_scanMap.insert(std::pair<int, int>(fitFarmId, scanCount));
}


int ScanList::reduceScanCount(int fitFarmId) {
	/* Get lock on map. */
	boost::unique_lock<boost::shared_mutex> lock(m_mutex);

	/* Element should exist! */
	assert(m_scanMap.count(fitFarmId) > 0);

	m_scanMap[fitFarmId]--;

	/* Remove element if count reached zero. */
	if (m_scanMap[fitFarmId] == 0) {
		m_scanMap.erase(fitFarmId);
		return 1;
	}
	else {
		return 0;
	}
}
