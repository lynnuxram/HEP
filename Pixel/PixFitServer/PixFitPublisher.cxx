/* @file PixFitPublisher.cxx
 *
 *  Created on: Dec 10, 2013
 *      Author: mkretz, marx
 */

#include <iostream>
#include <string>
#include <sstream>
#include <memory>

#include <ipc/core.h>
#include <oh/OHRootProvider.h>
#include <is/info.h>
#include <is/infoT.h>
#include <is/infodictionary.h>
#include <is/inforeceiver.h>
#include <cmdl/cmdargs.h>
#include <ers/ers.h>

#include <TF1.h>
#include <TH1.h>
#include <TH2.h>

#include "PixHistoServer/PixHistoServerInterface.h"

#include "PixFitPublisher.h"
#include "PixFitWorkQueue.h"
#include "PixFitResult.h"
#include "PixFitScanConfig.h"
#include "PixFitInstanceConfig.h"

using namespace PixLib;

/** @todo Remove "global". */
OWLSemaphore semaphore;

class PixFitPublisherCommandListener : public OHCommandListener{
 public:
  void command ( const std::string & name, const std::string & cmd )
  {
    std::cout << " Command " << cmd << " received for the " << name << " histogram" << std::endl;
  }
  
  void command ( const std::string & cmd )
  {
    std::cout << " Command " << cmd << " received for all histograms" << std::endl;
    if ( cmd == "exit" )
      {
	semaphore.post();
      }
  }
};

PixFitPublisher::PixFitPublisher(PixFitWorkQueue<PixFitResult> *publishQueue,
		PixFitInstanceConfig *instanceConfig) {
	this->m_publishQueue = publishQueue;
	this->m_threadName = "publisher";
	this->m_instanceConfig = instanceConfig;
}

PixFitPublisher::~PixFitPublisher() {
}

void PixFitPublisher::loop() {

  while (true) {

    /* Get work and gather other scanConfig information */
    std::shared_ptr<PixFitResult> result = m_publishQueue->getWork();
    std::shared_ptr<const PixFitScanConfig> scanConfig = result->getScanConfig();
    PixFitScanConfig::scanType scanType = scanConfig->findScanType();
    PixFitScanConfig::intermediateType intermediateType = scanConfig->intermediate;

    /* Build an OHRootProvider using scanConfig information */
    IPCPartition partition(scanConfig->partitionName);
    PixFitPublisherCommandListener lst;
    OHRootProvider provider(partition, scanConfig->serverName, scanConfig->providerName, &lst);
    ISInfoDictionary dict(partition);

    /* Decide a folder name */
    std::string ROD_String = "ROD_" +
    		scanConfig->histogrammer.crateletter +
    		std::to_string(scanConfig->histogrammer.crate) +
    		"_S" + std::to_string(scanConfig->histogrammer.rod);

    std::string folder_Name = "/" +
    		std::to_string(scanConfig->scanId) + "/" + ROD_String + "/Rx" +
    		std::to_string(RxConversion(scanConfig->histogrammer.slave, scanConfig->histogrammer.histo, result->chipId));

    /* Get some info on the histogram as strings. */
    std::string slave = std::to_string(scanConfig->histogrammer.slave);
    std::string histo = std::to_string(scanConfig->histogrammer.histo);
    std::string chipId = std::to_string(result->chipId);
    std::string binNumber = std::to_string(scanConfig->binNumber);

    /* Publish according to scantype. Check for intermediate types first! */
    if (intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_ANALOG) {
      provider.publish(*result->histo_occ, folder_Name + "/Occup_intermediate_" +
		       slave + "_" + histo  + "_" + chipId + "-" + binNumber);
    }
    else if (intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_TOT) {
      provider.publish(*result->histo_totmean, folder_Name + "/ToTMean_" + chipId + "-" + binNumber);
      provider.publish(*result->histo_totsum, folder_Name + "/ToTSum_" + chipId + "-" + binNumber);
      provider.publish(*result->histo_totsum2, folder_Name + "/ToTSum2_" + chipId + "-" + binNumber);
      provider.publish(*result->histo_totsigma, folder_Name + "/ToTSigma_" + chipId + "-" + binNumber);
      provider.publish(*result->histo_occ, folder_Name + "/Occ_" + chipId + "-" + binNumber);
    }
    else if (scanType == PixFitScanConfig::scanType::ANALOG || scanType == PixFitScanConfig::scanType::DIGITAL) {
      provider.publish(*result->histo_occ, folder_Name + "/Occup_0");  
    }
    else if (scanType == PixFitScanConfig::scanType::THRESHOLD) {
      provider.publish(*result->histo_thresh, folder_Name + "/1DTh_" + chipId);
      provider.publish(*result->histo_noise, folder_Name + "/1DNoi_" + chipId);
      provider.publish(*result->histo_chi2, folder_Name + "/1DChi2_" + chipId);
      provider.publish(*result->histo_thresh2D, folder_Name + "/Thr_" + chipId);
      provider.publish(*result->histo_noise2D, folder_Name + "/Noise_" + chipId);
      provider.publish(*result->histo_chi2_2D, folder_Name + "/Chi2_" + chipId);
    }
    else if (scanType == PixFitScanConfig::scanType::TOT) {
      provider.publish(*result->histo_totmean, folder_Name + "/ToTMean_" + chipId);
      provider.publish(*result->histo_totsum, folder_Name + "/ToTSum_" + chipId);
      provider.publish(*result->histo_totsum2, folder_Name + "/ToTSum2_" + chipId);
      provider.publish(*result->histo_totsigma, folder_Name + "/ToTSigma_" + chipId);
      provider.publish(*result->histo_occ, folder_Name + "/Occ_" + chipId);
    }
    else if (scanType == PixFitScanConfig::scanType::TOT_CALIB) {
    	/* We only have an empty result for TOT_CALIB at the moment, as the intermediate
    	 * histograms are published for external analysis. Do nothing here. */
    }

    ERS_DEBUG(0, result->getScanConfig()->histogrammer.makeHistoString() << " - " << chipId << ": Histogram was published to OH with internal ID " << result->getScanConfig()->fitFarmId)

    /* Check if publishing of results is complete for a particular ROD and signal to IS.
     * Make sure there is no race introduced here, in case non-intermediate histograms are published
     * before their corresponding intermediate histos. */
    if (intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_NONE
    		&& m_instanceConfig->scanlist.reduceScanCount(result->getScanConfig()->fitFarmId)) {
		ERS_LOG("Scan finished! All histograms for " << ROD_String << " are ready.")
		ISInfoBool isfinish(1);
		dict.checkin(scanConfig->serverName + "." + ROD_String + "_FinishScan", isfinish);
    }
  }
}


int PixFitPublisher::RxConversion(int slave, int histounit, int chip) {
  return 16*slave + 8*histounit + chip;
}
