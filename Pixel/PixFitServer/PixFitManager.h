/** @file PixFitManager.h
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz, marx
 */

#ifndef PIXFITMANAGER_H_
#define PIXFITMANAGER_H_

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <boost/thread.hpp>

#include <ipc/partition.h>
#include <ipc/object.h>
#include <is/callbackinfo.h>

#include "PixController/PixScan.h"
#include "PixUtilities/PixLock.h"
#include "PixActions/PixActions.h"
#include "PixUtilities/PixMessages.h"

#include "PixFitWorkQueue.h"
#include "PixFitNetConfiguration.h"
#include "PixFitScanConfig.h"
#include "PixFitInstanceConfig.h"

namespace PixLib {

class PixFitResult;
class RawHisto;

/** @todo: rewrite the locking mechanism for ROOT stuff */
extern boost::mutex root_m;

/** Manages one FitFarm process (Spawns and coordinates threads/scans). Should be
 * instantiated only once - not using singleton, though.*/
class PixFitManager {
 public:
  /** @param server_name Name of the histogramming server.
   * @param partition_name Name of the TDAQ partition.
   * @param instance_name Name to uniquely identify this FitServer instance.
   * @param slaveEmu Flag to determine whether data is coming from emulator. */
  PixFitManager(const char* server_name, const char* partition_name, const char* instance_name, bool slaveEmu);

  virtual ~PixFitManager();
  
  /** Starts the manager loop. */
  void run();

  /* Work queues */
  /** Queue between PixFitNet and PixFitWorker/Fitter
   * Contains RawHistos that might need fitting/calculating. */
  PixFitWorkQueue<RawHisto> fitQueue;

  /** Queue between PixFitWorker/Fitter and PixFitAssembler.
   * Contains processed results that still need to be assembled. */
  PixFitWorkQueue<PixFitResult> resultQueue;

  /** Queue between PixFitAssembler and PixFitPublisher.
   * Contains completely assembled results that are ready for publishing to OH */
  PixFitWorkQueue<PixFitResult> publishQueue;

  /** Looks for free ports by trying to bind to them. This is a poor-man's-workaround for running
   * multiple PixFitServers of different partitions on the same machine.
   * @param ipAddress The IPv4 address that we are interested in.
   * @param startPort First port to give a try.
   * @param numOfPorts Number of ports that is needed.
   * @returns First free port number of a range of consecutive ports. -1 otherwise. */
  int findFreePort(const char *ipAddress, int startPort, int numOfPorts);
  
  /** IS callback function to signal start or abort scan flagged by Controller. */
  static void callback(ISCallbackInfo * isc);


 private:
  /** Configuration struct for networking threads. Determines which network endpoint is associated
   * with a certain HistoUnit. */
  struct NetworkThreadConfig {
	  /** The associated IPv4 address. */
	std::string localIp;

	/** The associated TCP port number. */
	int localPort;

	/** The HistoUnit belonging to the networking thread. */
	HistoUnit histogrammer;
  };

  /** Internal counter to uniquely identify an ongoing scan.
   * As ScanID is reused for scans belonging to a tuning, we need a way to internally identify scans
   * in order to signal (partial) completion via IS. */
  int m_fitFarmCounter;

  /** Configuration of this PixFitServer instance. */
  PixFitInstanceConfig instanceConfig;

  /** Creates the instance configuration out of the information from IS available via getConfiguration().
   * @param config Map with the FitFarm configuration (association FitServer instance - ROD/slave).
   * @returns Vector with NetworkThreadConfigs for the current instance. */
  std::vector<std::shared_ptr<NetworkThreadConfig> > createNetThreadConfigs(std::map<std::string, std::string> config);

  /** Contains the configuration for this particular instance of PixFitServer. */
  std::vector<std::shared_ptr<PixFitNetConfiguration> > networkConfigs;

  /** Contains PixFitNet objects. */
  std::vector<std::shared_ptr<PixFitNet> > m_netObjects;

  /** Analyzes the PixScan object and creates, initializes and enqueues all the objects that are
   * necessary to succesfully publish the results of the scan.
   * @param decName Used to identify the ROOT file that contains
   * @param scanId Unique identifier for this scan.
   * @param crate Which crate is involved.
   * @param rod Which ROD is involved.
   * @param modMask Bit mask that identifies the involved FE modules.
   * @param sync Type of IS synchronization. Defaults to PixActions::Asynchronous. */
  void setupScan(std::string decName, PixFitScanConfig::ScanIdType scanId, int crate, int rod,
		  int modMask, PixActions::SyncType sync=PixActions::Asynchronous);

  /** Cancels an ongoing scan.
   * Eliminates PixFitScanConfig objects that fit the scanId as soon as they are popped from queues. Also signals PixFitNetwork threads
   * to discard a possibly ongoing receive operation and PixFitAssembler in case of buffered incomplete histograms.
   * *DISCLAIMER*: We only put scan IDs in the global blacklist irrespective of the ROD in question (as it is only possible to cancel
   * a scan globally anyways). However we need to make sure that each networking thread is reset after the corresponding slave was reset.
   * @param scanId The scan ID in question. 
   * @param crate Crate identifier.
   * @param rod ROD identifier. */
  void cancelScan(PixFitScanConfig::ScanIdType scanId, int crate, int rod);

  /** Sets up and configures the current PixFitServer process. */
  void setupPixFitServer();

  /** Translate a module mask as supplied by the PixController via IS to semi-filled HistoUnit structs.
   * @param modMask Module mask as supplied to FitFarm via IS for a scan.
   * @returns Vector containing HistoUnits that are only valid in their slave and histo field. */
  std::vector<HistoUnit> translateModuleMask(int modMask);

  /** Prints some information after startup of the PixFitServer. */
  void printBanner();

  /* PixMessages object to report in GUI */
  PixMessages *m_msg; 
  PixMessages &getMrs();

  /** Transform the string from IS into crate and rod numbers. */
  std::pair <bool, std::pair <int,int> > convertISstring(std::string ISstring);
};
} /* end of namespace PixLib */

#endif /* PIXFITMANAGER_H_ */
