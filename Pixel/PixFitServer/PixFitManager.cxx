/* @file PixFitManager.cxx
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz, marx
 */

#include <vector>
#include <iostream>
#include <string>
#include <bitset>
#include <map>
#include <cmath>
#include <queue> 
#include <memory>
#include <functional>
#include <utility> //std::pair

#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include <is/info.h>
#include <is/inforeceiver.h>
#include <ipc/core.h>
#include <ers/ers.h>

#include "RootDb/RootDb.h"
#include "PixModule/PixModule.h"
#include "PixFe/PixGeometry.h"

#include "PixFitManager.h"
#include "PixFitNetConfiguration.h"
#include "PixFitNet.h"
#include "PixFitPublisher.h"
#include "PixFitWorker.h"
#include "PixFitAssembler.h"
#include "PixFitInstanceConfig.h"
#include "PixFitResult.h"
#include "PixFitWorkQueue.h"

/** @todo: rewrite the locking mechanisms. */
/* Global lock for using anything remotely ROOTish. */
namespace PixLib {
boost::mutex root_m;
boost::condition_variable_any root_cond;

/* Queue to hold values of callback items, @todo: find another way to do this */
PixFitWorkQueue<std::pair<std::string, std::string>> RODqueue("RODqueue");
}

using namespace PixLib;

PixFitManager::PixFitManager(
		const char* server_name, const char* partition_name, const char* instance_name, bool slaveEmu) :
		fitQueue("FitQueue"),
		resultQueue("ResultQueue"),
		publishQueue("PublishQueue"),
		m_fitFarmCounter(0),
		instanceConfig(server_name, partition_name, instance_name, slaveEmu)
		{
	getMrs();

	/* Associate blacklist with the queues. */
	fitQueue.setBlackList(std::bind(&BlackList::investigateWorkObject, &instanceConfig.blacklist, std::placeholders::_1));
	resultQueue.setBlackList(std::bind(&BlackList::investigateWorkObject, &instanceConfig.blacklist, std::placeholders::_1));
	publishQueue.setBlackList(std::bind(&BlackList::investigateWorkObject, &instanceConfig.blacklist, std::placeholders::_1));
}

PixFitManager::~PixFitManager() {
}


void PixFitManager::run() {
	printBanner();
	setupPixFitServer();

	/* Setup IPC and IS */
	IPCPartition partition(instanceConfig.getPartitionName());
	ISInfoDictionary dict(partition);

	/* Spawn network threads. */
	ERS_LOG("Starting networking threads.")
	boost::thread_group networkThreads;

	for (auto& netConfig : networkConfigs) {
		if (netConfig->isActive()) {
		  /* @todo: !! The first variant seems to create SEGFAULTs when the FitServer is started by the PMG, while
		   * running fine standalone. Using the "new" works correct in all cases. Misuse of shared_ptr here?! */
		  //std::shared_ptr<PixFitNet> fitNet = std::make_shared<PixFitNet>(&fitQueue, it->get());
		  std::shared_ptr<PixFitNet> fitNet(new PixFitNet(&fitQueue, netConfig.get(), &instanceConfig));
		  m_netObjects.push_back(fitNet);
		  fitNet->start();

		  /* Add thread to the thread group. */
		  networkThreads.add_thread(fitNet->getThread());
		}
	}

	/* Spawn worker thread. */
	ERS_LOG("Starting fitting threads.")
	PixFitWorker worker(&fitQueue, &resultQueue);
	worker.start();

	/* Spawn assembler thread. */
	ERS_LOG("Starting assembler thread.")
	PixFitAssembler assembler(&resultQueue, &publishQueue, &instanceConfig);
	assembler.start();

	/* Spawn publisher thread. */
	ERS_LOG("Starting publishing thread.")
	PixFitPublisher publisher(&publishQueue, &instanceConfig);
	publisher.start();

	/* Subscribe to IS. */
	ISInfoReceiver rec(partition); 
	ISInfoBool scnfinish(0);
	ISInfoString decName;
	ISInfoInt scnid;
	ISInfoInt modmask;
	ISCriteria criteria(".*Scan");
	std::string serverName = instanceConfig.getServerName();

	if (!instanceConfig.usingSlaveEmu()) {

	  /* Get scan configuration information details from IS */

	    /* Get scan start from IS (with lock).
	     * Subscribe here now uses criteria so will callback whenever a controller of the RODs in 
	     * this FitServer instance gets updated. */
	    rec.subscribe(serverName, criteria, PixFitManager::callback);

	    while (true) {

	    /* Blocking call to get ROD string from queue. */
	    std::shared_ptr<std::pair<std::string, std::string>> rodWork = RODqueue.getWork();
	    std::string scnstart = rodWork->second;
	    if (rodWork->first == "start") {
	      dict.checkin( serverName + "." + scnstart + "_FinishScan", scnfinish);

	    /* Fill crate and rod based on string in start scan flag. */
	    int currentcrate = convertISstring(scnstart).second.first;
	    int currentrod = convertISstring(scnstart).second.second;
	    
	    /* Get decName, scanId and module mask from IS (put a catch here since there is no lock and timing might be off). */
	    try {
	      dict.getValue(serverName + ".DecName", decName);
	      dict.getValue(serverName + ".ScanId", scnid);
	      dict.getValue(serverName + "." + scnstart + "_ScanModuleMask", modmask);
	    }
	    catch(...) {
	      ERS_INFO("Failed to get decname, scanid or modmask from IS")
	    }

	    ERS_LOG("Started scan " << static_cast<int>(scnid) << " for crate/ROD " << currentcrate << "/" << currentrod <<
		", FE mask in binary = " << std::bitset<32>(modmask) << std::dec << " with internal ID " << m_fitFarmCounter);

	    /* Build PixScan object and fill PixFitScanConfig object with all this information */
	    setupScan(decName, scnid, currentcrate, currentrod, modmask);
	    }

	    /* Aborting scan otherwise. */
	    else if (rodWork->first == "abort") {
		/* Fill crate and rod based on string in start scan flag. */
		int currentcrate = convertISstring(scnstart).second.first;
		int currentrod = convertISstring(scnstart).second.second;
		try {
		  dict.getValue(serverName + ".ScanId", scnid);
		}
		catch(...) {
		  ERS_INFO("Failed to get scanid from IS")
		}
		cancelScan(scnid, currentcrate, currentrod);
	    }
	  }
	  }
	  else {
	    setupScan(instanceConfig.slaveEmuRootFile, 33, 2, 16, 8);
	    /* Dirty hack to wait for emulator. */
	    std::cin.ignore().get();
	}

	  //PixScanTask::saveResults to write outcome somewhere, TODO: tell the other code that it doesn't need to this anymore

	/* Unsubscribe from IS. */
	rec.unsubscribe(serverName, criteria);

	networkThreads.join_all();
	worker.join();
	assembler.join();
	publisher.join();
}


int PixFitManager::findFreePort(const char *ipAddress, int startPort, int numOfPorts){
	const int maxPortsToScan = 1000;
	int localSock = -1;
	int availablePorts = 0;

	// ERS_DEBUG(0, Looking for " << numOfPorts << " available ports on IP " << ipAddress)

	for (int i = startPort; i <= startPort + maxPortsToScan; i++) {
		localSock = socket(AF_INET, SOCK_STREAM, 0);

		if (-1 == localSock) {
			ERS_INFO("Opening socket failed.")
			perror("Error:");
			return -1;
		}

		int yes = 1;
		if (setsockopt(localSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			ERS_INFO("Setting socket option failed")
			perror("Error:");
			close(localSock);
			return -1;
		}

		sockaddr_in my_addr;
		my_addr.sin_family = AF_INET;
		inet_aton(ipAddress, &my_addr.sin_addr);
		my_addr.sin_port = htons(i);
		if (bind(localSock, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) {
			//ERS_INFO(ipAddress << ":" << i << " is busy.")
			//perror("Error:");
			availablePorts = 0;
		}
		else {
			close(localSock);
			//ERS_INFO(ipAddress << ":" << i <<  " is OK.")
			availablePorts++;
		}
		if (availablePorts == numOfPorts) {
			int startingPort = i - numOfPorts + 1;
			ERS_LOG(numOfPorts << " ports available on IP " << ipAddress << " starting from " << startingPort)
			return startingPort;
		}
	}
	std::string info = "PixFitManager::findFreePort()";
	std::string mess = "Could not find any free ports for IP " + (std::string)ipAddress;
	m_msg->publishMessage(PixMessages::FATAL, info , mess);

  return -1;
}

void PixFitManager::callback(ISCallbackInfo * isc) {
	std::string type = "";
	ISInfoString isi;
	isc->value(isi);
	ERS_DEBUG(0, "CALLBACK : " << isc->name() << " with content " << isi)
	
	/* Nasty. Use some other queue type instead to distinguish between abort and start scan. */
	if (boost::algorithm::ends_with(static_cast<std::string>(isc->name()), "_StartScan")) {
	    type = "start";
	}
	else {
	    type = "abort";
	}
	RODqueue.addWork(std::make_shared<std::pair<std::string, std::string>>(std::make_pair(type, static_cast<std::string>(isi))));
}

void PixLib::PixFitManager::setupScan(std::string decName,
		PixFitScanConfig::ScanIdType scanId,
	        int crate, int rod,
		int modMask, PixActions::SyncType) {

  std::shared_ptr<PixScan> pixScanFitServer;

  int rown = 0;
  int coln = 0;
  std::string fileName;
  int pos = decName.find_first_of(":");
  fileName = decName.substr(0, pos);
  std::string flavour = "_#FLAV";
  std::string FEflav;
  std::size_t start_pos = 0;
  //if(m_feFlav == PixModule::PM_FE_I2) FEflav = "_I3";  // T
  //else if(m_feFlav == PixModule::PM_FE_I4A || m_feFlav == PixModule::PM_FE_I4B) FEflav = "_I4"; // T
  FEflav = "_I4";

  while ((start_pos = fileName.find(flavour, start_pos)) != std::string::npos) {
    fileName.replace(start_pos, flavour.length(), FEflav);
    start_pos += FEflav.length();
  }
  decName = fileName;
  decName += ":/rootRecord;1";
  

  try {
    /* Making PixScan object and also filling some info from PixModule. */

    boost::lock_guard<boost::mutex> lock(root_m);

    RootDb root(fileName, "READ");
    DbRecord* rec = root.DbFindRecordByName(decName);

    PixModule pm(rec, 0, "M990001");
    PixGeometry geo = pm.geometry();
    geo = PixGeometry(PixGeometry::FEI4_CHIP);
    rown = geo.nRow();
    coln = geo.nCol();

    /** @todo How is the PixScan being constructed? Is it still valid after root has been deleted? */
    pixScanFitServer = std::shared_ptr<PixScan>(new PixScan(rec));
  } catch (...) {
    ERS_INFO("Exception caught while creating new RootDb and calling RootDb::DbFindRecordByName.")
  }

  /* The number of work packages for this instance can be calculated as follows:
   * #active histo units * #mask steps */
  int numOfMaskSteps = PixFitScanConfig::getNumOfMaskSteps(pixScanFitServer, instanceConfig.usingSlaveEmu());
  int numOfTotalMaskSteps = PixFitScanConfig::getNumOfTotalMaskSteps(pixScanFitServer, instanceConfig.usingSlaveEmu());
  /* sanity check to make sure user selected sensible values for scan in Console */
  if(numOfMaskSteps > numOfTotalMaskSteps){
    std::string info = "PixFitManager::setupScan())";
    std::string mess = "Number of mask steps is larger than number of total mask steps, please correct this in your Console parameter selection";
    m_msg->publishMessage(PixMessages::ERROR, info , mess);
  }
  
  /* Get the involved units without correct crate and rod fields from the module mask. */
  std::vector<HistoUnit> units = translateModuleMask(modMask);

  /* Count how many PixFitScanConfig objects are created (ignoring extra ones for mask stepping.) */
  int objCount = 0;

  /* Build and enqueue PixFitScanConfig objects. We iterate through the active histo units and
   * look for a match in the networking threads. When a match is found the correct number of
   * PixFitScanConfigs is created and put in the network thread's queue. */
  for (auto &unit : units) {
	  /* Fill with correct values for crate and ROD. */
	  unit.crate = crate;
	  unit.rod = rod;


	  for (std::vector<std::shared_ptr<PixFitNet> >::iterator net_it = m_netObjects.begin();
			  net_it != m_netObjects.end(); net_it++) {

	  /* Only create and enqueue object(s) if network thread serves the particular histo unit. */
		  if (*((*net_it)->getConfig()->getHistogrammer()) == unit) {
			  for (int j = 0; j < numOfMaskSteps; j++) {
				  auto pixFitScanConfig = std::make_shared<PixFitScanConfig>(instanceConfig.usingSlaveEmu());

					pixFitScanConfig->partitionName = instanceConfig.getPartitionName();
					pixFitScanConfig->serverName = instanceConfig.getServerName();
					pixFitScanConfig->providerName = instanceConfig.getInstanceId();
					pixFitScanConfig->pixScanConfig = pixScanFitServer;
					pixFitScanConfig->histogrammer = *((*net_it)->getConfig()->getHistogrammer());
					pixFitScanConfig->maskId = j;
					pixFitScanConfig->NumRow = rown;
					pixFitScanConfig->NumCol = coln;
					pixFitScanConfig->scanId = scanId;
					pixFitScanConfig->modMask = modMask;
					pixFitScanConfig->fitFarmId = m_fitFarmCounter;

				/* Enqueue pixFitScanConfig into network thread queue. */
				(*net_it)->putScanConfig(pixFitScanConfig);
				}
			  /* Increment created object count (ignoring mask steps) for this fitFarmId.
			   * This works for the usual case with 8 chips per histounit. For cases where one ROD
			   * might use strange combinations of active FEs per histo unit this will break! */
			  objCount += 8;
		  }
    }
  }
  /* Put ID/objCount in ScanList. */
  instanceConfig.scanlist.setScanCount(m_fitFarmCounter, objCount);

  /* Increment internal counter after each setupScan for a ROD. */
  m_fitFarmCounter++;

  /** @todo Signal readiness to entity that is steering the scan. Set IS variable back to 0? */
}

/** For configuring the FitFarm PixFitServer processes, we need to get some general settings
 * that are valid for all PixFitServer instances (such as partition name), as well as
 * configuration options specific to a particular PixFitServer process.
 * We need to know which histogramming units are associated to a certain PixFitServer process -
 * or rather to which PixFitNet thread - as well as the network configuration of those network
 * threads.
 * We assume that each FitFarm instance is assigned a unique ID directly after startup. It will
 * then use this ID to look up its configuration which consists of a list of assigned RODs. It will
 * then assign IP/port combination for the histogramming units on these RODs.
 * Afterwards it will publish a list of CRATE/ROD/SLAVE/HISTOUNIT, IP:port to IS.
 */
void PixLib::PixFitManager::setupPixFitServer() {
	/* Setup IPC and IS */
	IPCPartition partition(instanceConfig.getPartitionName());
	ISInfoDictionary dict(partition);

	/* Get configuration of networking threads. */
	std::vector<std::shared_ptr<NetworkThreadConfig> > netThreadConfigs =
			createNetThreadConfigs(instanceConfig.getConfiguration());

	/* Create the network configuration. */
	ERS_LOG("Creating FitFarm instance " << instanceConfig.getInstanceId() << " with configuration:")
	for (auto netThreadConfig : netThreadConfigs) {
		std::shared_ptr<PixFitNetConfiguration> config = std::make_shared<PixFitNetConfiguration>();

		config->setLocalIpAddress(netThreadConfig->localIp);
		config->setLocalPort(netThreadConfig->localPort);
		config->setHistogrammer(netThreadConfig->histogrammer);
		config->enable();
		networkConfigs.push_back(config);

		ERS_LOG(netThreadConfig->localIp << ":" << netThreadConfig->localPort << " for "
				<< netThreadConfig->histogrammer.makeHistoString())

		/* Publish IP and port number to IS for Controller/PPC */
		HistoUnit h = netThreadConfig->histogrammer;
		std::string ISvariable = ".ROD_" + h.crateletter +
				std::to_string(h.crate)
		+ "_S" + std::to_string(h.rod)
		+ "_" + std::to_string(h.slave)
		+ "_" + std::to_string(h.histo);
		std::string ISvalue = netThreadConfig->localIp + ":" +
				std::to_string(netThreadConfig->localPort);
		ISInfoString isstring(ISvalue);
		dict.checkin( instanceConfig.getServerName() + ISvariable, isstring );
	}

	ERS_LOG("Network configuration contains " << networkConfigs.size() << " objects.")
}

void PixFitManager::printBanner() {
	std::cout << "===========================" << std::endl;
	std::cout << "Starting FitServer v"	<< PixFitInstanceConfig::fitServerVersion << std::endl;
	std::cout << "===========================" << std::endl;
	std::cout << "Partition: " << instanceConfig.getPartitionName()	<< std::endl;
	std::cout << "Server name: " << instanceConfig.getServerName() << std::endl;
	std::cout << std::endl;

	std::cout << "Installed RAM: " << instanceConfig.getRam() << " MB" << std::endl;
	std::cout << std::endl;

	std::cout << "Local IP address(es):" << std::endl;
	std::map<std::string, std::string> Ips = instanceConfig.getLocalIps();
	for (std::map<std::string, std::string>::iterator it = Ips.begin();
			it != Ips.end(); it++) {
		std::cout << it->first << ": " << it->second << std::endl;
	}
	std::cout << std::endl;
}


std::vector<std::shared_ptr<PixFitManager::NetworkThreadConfig> > PixLib::PixFitManager::createNetThreadConfigs(
		std::map<std::string, std::string> config) {

	std::vector<std::shared_ptr<PixFitManager::NetworkThreadConfig> > netThreadConfig;
	std::vector<std::string> rodNetworkIps;
	std::vector<std::string> rods;

	std::map<std::string, std::string> localIps = instanceConfig.getLocalIps();
	for (auto& networkInterface : instanceConfig.getRodNetworkInterfaces()) {
		if (localIps.find(networkInterface) != localIps.end()) {
			rodNetworkIps.push_back(localIps.find(networkInterface)->second);
		}
	}
	/* Preliminary: Listen on all available interfaces if no interfaces are specifically selected. */
	if (instanceConfig.getRodNetworkInterfaces().empty()) {
		ERS_LOG("Fallback: Listening on all available interfaces as rodNetworkInterfaces not configured.")
		rodNetworkIps.push_back("0.0.0.0");
	}

	for (std::map<std::string, std::string>::iterator it = config.begin(); it != config.end(); it++) {
		if (it->second == instanceConfig.getInstanceId()) {
			rods.push_back(it->first);
		}
	}

	if (rodNetworkIps.empty()) {
		ERS_INFO("Error: No IPs available on the ROD network. Aborting setup.")
		return std::vector<std::shared_ptr<PixFitManager::NetworkThreadConfig> >();
	}

	ERS_LOG("Server has " << rodNetworkIps.size() << " IP(s) on the ROD network and is serving " << rods.size() << " ROD(s).")

	/* Now we create 4 histounits out of each ROD string and assign those to one IP/port combination. */

	for (unsigned int i = 0; i < rods.size(); i++) {
		HistoUnit histoUnit;

		/* Match the configuration string. */
		if (convertISstring(rods[i]).first) {

		        histoUnit.crateletter = rods[i].at(4);
			histoUnit.crate = convertISstring(rods[i]).second.first;
			histoUnit.rod = convertISstring(rods[i]).second.second;

			/* Create 4 histogramming units per ROD. */
			for (unsigned int j = 0; j < 4; j++) {
				std::shared_ptr<PixFitManager::NetworkThreadConfig> config = \
						std::make_shared<PixFitManager::NetworkThreadConfig>();
				config->histogrammer = histoUnit;
				config->histogrammer.slave = j / 2;
				config->histogrammer.histo = j % 2;
				netThreadConfig.push_back(config);
			}
		}
	}

	/** Distribute equally across all available interfaces.
	 * @todo This is actually sub-optimal. Improve algorithm if necessary. */
	int numOfUnits = netThreadConfig.size();
	int numOfNics = rodNetworkIps.size();
	int unitsPerNic = (numOfUnits + numOfNics - 1) / numOfNics;

	int portNum = 0;
	std::string ipAddress;

	for (int i = 0; i < numOfUnits; i++) {
		/* For each new IP find valid range for ports. */
		if (i % unitsPerNic == 0){
			ipAddress = rodNetworkIps[i / unitsPerNic];
			portNum = findFreePort(ipAddress.c_str(), instanceConfig.startPort, unitsPerNic);
		}
		netThreadConfig[i]->localIp = ipAddress;
		netThreadConfig[i]->localPort = portNum + (i % unitsPerNic);
	}

	return netThreadConfig;
}

void PixFitManager::cancelScan(PixFitScanConfig::ScanIdType scanId, int crate, int rod) {
	ERS_LOG("Cancelling scan with ID " << scanId << " for crate/ROD " << crate << "/" << rod)

	/* Add scan ID to the global blacklist. Do this before the next step to make sure queue
	 * blacklists are set up already. */
	instanceConfig.blacklist.addScanId(scanId);

	/* Iterate through network threads to reset them. */
	for (auto& netObject : m_netObjects) {
		auto hist = netObject->getConfig()->getHistogrammer();
		if (hist->crate == crate && hist->rod == rod) {
		    netObject->resetNetwork(false);
		}
	}

	/* Tell assembler to clean up the hold to remove orphaned objects. */
	instanceConfig.assembler->setCleanFlag();
}


std::vector<HistoUnit> PixLib::PixFitManager::translateModuleMask(int modMask) {
	std::vector<HistoUnit> unitVec;
	HistoUnit histo;

	/* Initialize with invalid entries for crate and rod field. */
	histo.crate = -1;
	histo.rod = -1;

	if ((0x000000FF & modMask) > 0) {
		histo.slave = 0;
		histo.histo = 0;
		unitVec.push_back(histo);
	}
	if ((0x0000FF00 & modMask) > 0) {
		histo.slave = 0;
		histo.histo = 1;
		unitVec.push_back(histo);
	}
	if ((0x00FF0000 & modMask) > 0) {
		histo.slave = 1;
		histo.histo = 0;
		unitVec.push_back(histo);
	}
	if ((0xFF000000 & modMask) > 0) {
		histo.slave = 1;
		histo.histo = 1;
		unitVec.push_back(histo);
	}

	return unitVec;
}

PixMessages& PixFitManager::getMrs()
{
  m_msg = new PixMessages();
  IPCPartition partition(instanceConfig.getPartitionName());
  m_msg->pushPartition(instanceConfig.getPartitionName(), &partition);
  m_msg->pushStream("cout", std::cout);
  return *m_msg;
}

std::pair <bool, std::pair <int,int> >  PixFitManager::convertISstring(std::string ISstring){

  boost::regex regEx("ROD_[CIL]([0-9]{1,2})_S([0-9]{1,2})");
  boost::smatch match;
  bool success = boost::regex_match(ISstring, match, regEx);
  int crate = 0;
  int rod = 0;
  if (success) {
    std::string scrate(match[1].first, match[1].second);
    std::string srod(match[2].first, match[2].second);
    crate = atoi(scrate.c_str());
    rod = atoi(srod.c_str());
  }

  return std::make_pair(success, make_pair(crate, rod) );
}
