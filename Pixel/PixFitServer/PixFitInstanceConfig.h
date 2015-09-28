/** @file PixFitInstanceConfig.h
 *
 *  Created on: Jul 14, 2014
 *      Author: mkretz
 */

#ifndef PIXFITINSTANCECONFIG_H_
#define PIXFITINSTANCECONFIG_H_

#include <string>
#include <map>
#include <memory>

#include <boost/thread.hpp>

#include "PixFitNetConfiguration.h"
#include "PixFitScanConfig.h"

namespace PixLib {

class PixFitAssembler;
class PixFitWorkPackage;

/** Class that manages aborted scan IDs.
 * BlackList holds a list of aborted scan IDs and manages concurrent read/write access. Clients include
 * the PixFitWorkQueue to check if a scan has possibly been aborted and a work item can be discarded. */
class BlackList {
public:
	BlackList();
	virtual ~BlackList();

	/** Checks if scanId of PixFitWorkPackage is blacklisted.
	 * @param workPackage Pointer to work object in question.
	 * @returns True if object's scanId is blacklisted, false otherwise. */
	bool investigateWorkObject(std::shared_ptr<const PixFitWorkPackage> workPackage);

	/** Checks if a scan is blacklisted.
	 * @param scanId The scan ID to be checked.
	 * @returns True in case the scan is blacklisted, false otherwise. */
	bool isScanIdListed(PixFitScanConfig::ScanIdType scanId);

	/** Adds a scan ID to the blacklist.
	 * @param scanId The scan ID to be added. */
	void addScanId(PixFitScanConfig::ScanIdType scanId);

private:
	/** Contains scan IDs that are blacklisted. */
	std::vector<PixFitScanConfig::ScanIdType> m_blackList;

	/** Controls r/w access to blackList. */
	boost::shared_mutex m_mutex;
};


/** Class that holds information of how many histogramming units are used on a ROD during a scan.
 * We set how many objects per ROD are supposed to be published by the PixFitPublisher. */
class ScanList{
public:
	ScanList();
	virtual ~ScanList();

	/** Set the initial count for a new ID.
	 * @param fitFarmId The internal ID identifying the scan and ROD.
	 * @param scanCount How many objects need to be published by the publisher before the finish
	 * scan flag can be set. This is usually something like #histo units x #chips per unit. */
	void setScanCount(int fitFarmId, int scanCount);

	/** Reduce the count belonging to an ID.
	 * If count reaches zero, removes the entry from the map.
	 * @param fitFarmId The internal ID identifying the scan and ROD.
	 * @returns 0 on success, 1 if count reached zero. */
	int reduceScanCount(int fitFarmId);

private:
	/** Contains IDs and their count. */
	std::map<int, int> m_scanMap;

	/** Controls r/w access to m_scanMap. */
	boost::shared_mutex m_mutex;
};


/** Holds the necessary configuration options that are associated with a
 * particular PixFitServer instance. It provides functions to discover machine spec details and
 * network configuration that are being used. There should be only one instance of this class per
 * PixFitServer. */
class PixFitInstanceConfig {
public:
	/** @param serverName The server name for histogram publishing.
	 * @param partitionName The name of the IPC partition to which the instance belongs.
	 * @param instanceID A unique identifier to differentiate between multiple PixFitServer instances.
	 * @param slaveEmu Indicates whether we are using a real client or the emulator. */
	PixFitInstanceConfig(
			std::string serverName,
			std::string partitionName,
			std::string instanceID,
			bool slaveEmu);

	virtual ~PixFitInstanceConfig();

	/** Decide on the number of worker and fitting threads.
	 * @returns Number of usable (virtual) cores for threading. */
	static int getThreadingSettings();

	/** Get machine's memory.
	 * @returns Total memory of the machine in megabytes. */
	static unsigned long getRam();

	/** Get the IP addresses assigned to all local network interfaces.
	 * @returns A map with interface names as keys and IP addresses. */
	static std::map<std::string, std::string> getLocalIps();

	/** Retrieves the configuration from the DB and returns a map of RODs and FitFarm instance IDs. */
	static std::map<std::string, std::string> getConfiguration();

	/** FitServer version number. */
	static const std::string fitServerVersion;

	/** ROOT file to be used in conjunction with slaveEmu. */
	static const std::string slaveEmuRootFile;

	/** Lowest local port number for ROD network listening threads. */
	static const int startPort = 6000;

	/** Flag to indicate whether network traffic should be dumped. */
	static const bool dumpNetwork = false;

	/* Getter functions. */
	bool usingSlaveEmu() const;
	std::string getInstanceId() const;
	const std::string& getPartitionName() const;
	const std::vector<std::string>& getRodNetworkInterfaces() const;
	const std::string& getServerName() const;

	/** Contains aborted scans. */
	BlackList blacklist;

	/** Contains started scans. */
	ScanList scanlist;

	/** Pointer to PixFitAssembler. */
	PixFitAssembler *assembler;

private:
	/** Server name. */
	std::string serverName;

	/** Partition name. */
	std::string partitionName;

	/** Flag to show if emulator is being used as a client. */
	bool slaveEmu;

	/** Unique identifier for this FitFarm instance. */
	std::string instanceID;

	/** Vector of interfaces that belong to the ROD-side network (eth0, eth1...). */
	std::vector<std::string> rodNetworkInterfaces;
};
} /* end of namespace PixLib */

#endif /* PIXFITINSTANCECONFIG_H_ */
