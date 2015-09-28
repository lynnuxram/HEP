/** @file PixFitAssembler.cxx
 *
 *  Created on: Mar 18, 2014
 *      Author: mkretz
 */

#include <cassert>
#include <map>
#include <memory>

#include <boost/thread.hpp>

#include <TF1.h>
#include <TH1.h>
#include <TH2.h>

#include <ers/ers.h>

#include "PixFitAssembler.h"
#include "PixFitWorkQueue.h"
#include "PixFitResult.h"
#include "RawHisto.h"
#include "PixFitManager.h" // for ROOT lock

using namespace PixLib;

PixFitAssembler::PixFitAssembler(PixFitWorkQueue<PixFitResult> *resultQueue,
		PixFitWorkQueue<PixFitResult> *publishQueue, PixFitInstanceConfig *instanceConfig) {
	this->resultQueue = resultQueue;
	this->publishQueue = publishQueue;
	this->m_threadName = "assembler";
	this->m_instanceConfig = instanceConfig;
	this->m_cleanFlag = false;
	instanceConfig->assembler = this; // register with instanceConfig
}

PixFitAssembler::~PixFitAssembler() {

}

void PixFitAssembler::loop() {

	while (true) {
		/* Get result object. */
		std::shared_ptr<PixFitResult> result = resultQueue->getWork();

		/* Put result in hold. */
		std::shared_ptr<const PixFitScanConfig> scanCfg = result->getScanConfig();
		OuterMap::iterator outerIt = m_results.find(std::make_pair(scanCfg->scanId, scanCfg->binNumber));
		OuterKey outerKey = std::make_pair(scanCfg->scanId, scanCfg->binNumber);
		HistoUnit histo = scanCfg->histogrammer;

		/* Check if scanId is already present. */
		if (outerIt == m_results.end()) {
			InnerMap iMap;
			iMap.insert(
					std::make_pair(histo, std::vector<std::shared_ptr<PixFitResult> >(scanCfg->getNumOfMaskSteps())));
			m_results.insert(std::make_pair(outerKey, iMap));
		}
		/* Check if histogramming unit is already present. */
		else if (outerIt->second.find(histo) == outerIt->second.end()) {
			outerIt->second.insert(
					std::make_pair(histo, std::vector<std::shared_ptr<PixFitResult> >(scanCfg->getNumOfMaskSteps())));
		}
		/* Check if maskID is already present in vector, which should never be the case. */
		else {
			/* Crosscheck vector size and maskId. */
			assert(outerIt->second[histo].size() == static_cast<size_t>(scanCfg->getNumOfMaskSteps()));
			assert(outerIt->second[histo].size() > static_cast<size_t>(scanCfg->maskId));

			/* Error: Result does already exist! */
			if (outerIt->second[histo].at(scanCfg->maskId) != nullptr) {
				ERS_INFO("ERROR: Trying to put already existing PixFitResult object in hold.")
				ERS_INFO(histo.makeHistoString() << ": Scan ID: " << scanCfg->scanId << " binNumber: " << scanCfg->binNumber)
				return;
			}
		}

		/* Insert result into vector. */
		outerIt = m_results.find(outerKey);
		outerIt->second[histo].at(scanCfg->maskId) = result;

		/* Check if vector is complete for reassembly. */
		unsigned int resultsCount = 0;
		for (auto& result : outerIt->second[histo]) {
			if (result != nullptr)
				resultsCount++;
		}

		/* Reassemble result if all data is there. */
		ResultsVector resVec;
		if (resultsCount == outerIt->second[histo].size()) {
			resVec = reassemble(outerIt->second[histo]);
			/* Put reassembled result(s) in queue. */
			for (auto& vecIt : resVec) {
			  publishQueue->addWork(vecIt);
			}
			
			/* Remove vector (and possibly clean up map). */
			outerIt->second.erase(histo);
			/* Check if other work packages belonging to this scanId exist. */
			if (outerIt->second.size() == 0) {
			  m_results.erase(outerKey);
			}
		}

		/* Check if we need to clean m_results. */
		boost::lock_guard<boost::mutex> lock(m_cleanMutex);
		if (m_cleanFlag) {
			cleanAssembler();
			m_cleanFlag = false;
		}
	}
}

void PixFitAssembler::printMap(int depth) {
	std::cout << "*** Assembler hold ***" << std::endl;
	std::cout << "Buffered Scan IDs: ";
	for (auto& outer : m_results) {
		std::cout << outer.first.first << " ";
	}
	std::cout << std::endl;

	if (depth >= 1) {

		/* Print ScanIDs. */
		for (auto& outer : m_results) {
			std::cout << outer.first.first << " / " << outer.first.second << ":";

			/* Print HistoUnits. */
			for (auto& inner : outer.second) {
				inner.first.printHistoUnit();

				/* Print valid data packages. */
				if (depth >= 2) {
					for (auto vecIt = inner.second.begin(); vecIt != inner.second.end();
							vecIt++) {

						/* Check if data is valid at position and print position in vector. */
						if (vecIt->get() != nullptr) {
							std::cout << vecIt - inner.second.begin() << " ";
						}
					}
					std::cout << std::endl << "--------------------------------" << std::endl;
				}
			}
		}
	}
}

/* First creates the needed ROOT histogram(s) and then fills them with values from the RawHisto
 * or PixFitResult taking mask stepping into account. */
PixFitAssembler::ResultsVector PixFitAssembler::reassemble(ResultsVector& resVec) {

	std::shared_ptr<const PixFitScanConfig> pScanConfig = resVec.at(0)->getScanConfig();
	PixFitScanConfig::ScanIdType scanId = pScanConfig->scanId;
	int binNumber = pScanConfig->binNumber;
	int crate = pScanConfig->histogrammer.crate;
	int rod = pScanConfig->histogrammer.rod;
	int slave = pScanConfig->histogrammer.slave;
	int histo = pScanConfig->histogrammer.histo;

	ERS_DEBUG(0, pScanConfig->histogrammer.makeHistoString() << ": Reassembling result with scanId " << 
	    scanId << " and binNumber " << binNumber) 

	ERS_DEBUG(0, "Vector contains " << resVec.size() << " object(s) for "
			<< pScanConfig->getNumOfMaskSteps() << " mask step(s).")

	ResultsVector tmpResultVec;

	PixFitScanConfig::scanType scanType = pScanConfig->findScanType();
	PixFitScanConfig::intermediateType intermediateType = pScanConfig->intermediate;
	int maskSteps = pScanConfig->getNumOfMaskSteps();
	int numChips = pScanConfig->getNumOfChips();

	assert(resVec.size() == static_cast<size_t>(maskSteps));

	/* Locking ROOT for the next two loops. */
	boost::lock_guard<boost::mutex> lock(root_m);

	for (int chip = 0; chip < numChips; chip++) {
		std::shared_ptr<PixFitResult> tmpResult = std::make_shared<PixFitResult>(pScanConfig);
		tmpResult->chipId = chip;
		int ncol = tmpResult->getScanConfig()->NumCol;
		int nrow = tmpResult->getScanConfig()->NumRow;

		/* k is attached to name and title of histograms to make them unique. */
		TString k = Form("%d-%d-%d-%d-%d", crate, rod, slave, histo, chip);

		/* Order is important, check for INTERMEDIATE types first! */
		if (intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_ANALOG) {
			int bin = tmpResult->getScanConfig()->binNumber;
			prepareHisto(tmpResult->histo_occ, ncol, nrow, "occupancy-", k, bin);
		}
		else if (intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_TOT) {
			int bin = tmpResult->getScanConfig()->binNumber;

			prepareHisto(tmpResult->histo_totmean, ncol, nrow, "totmean-", k, bin);
			prepareHisto(tmpResult->histo_totsum, ncol, nrow, "totsum-", k, bin);
			prepareHisto(tmpResult->histo_totsum2, ncol, nrow, "totsum2-", k, bin);
			prepareHisto(tmpResult->histo_totsigma, ncol, nrow, "totsigma-", k, bin);
			prepareHisto(tmpResult->histo_occ, ncol, nrow, "occ", k, bin);
		}
		else if (scanType == PixFitScanConfig::scanType::ANALOG || scanType == PixFitScanConfig::scanType::DIGITAL) {
			prepareHisto(tmpResult->histo_occ, ncol, nrow, "occupancy-", k);
		}
		else if (scanType == PixFitScanConfig::scanType::THRESHOLD) {
			TString histoName;

			histoName= "threshold-" + k;
			tmpResult->histo_thresh = std::make_shared<TH1F>(histoName, histoName, 1000, 0., tmpResult->getScanConfig()->getNumOfBins());

			histoName = "noise-" + k;
			tmpResult->histo_noise = std::make_shared<TH1F>(histoName, histoName, 100, 0., 10.);

			histoName = "chi2-" + k;
			tmpResult->histo_chi2 = std::make_shared<TH1F>(histoName, histoName, 25, 0., 50.);

			histoName = "threshold2D-" + k;
			tmpResult->histo_thresh2D = std::make_shared<TH2F>(histoName, histoName, ncol, 0, ncol, nrow, 0, nrow);

			histoName = "noise2D-" + k;
			tmpResult->histo_noise2D = std::make_shared<TH2F>(histoName, histoName, ncol, 0, ncol, nrow, 0, nrow);

			histoName = "chi2_2D-" + k;
			tmpResult->histo_chi2_2D = std::make_shared<TH2F>(histoName, histoName, ncol, 0, ncol, nrow, 0, nrow);
		}
		else if (scanType == PixFitScanConfig::scanType::TOT) {
			prepareHisto(tmpResult->histo_totmean, ncol, nrow, "totmean-", k);
			prepareHisto(tmpResult->histo_totsum, ncol, nrow, "totsum-", k);
			prepareHisto(tmpResult->histo_totsum2, ncol, nrow, "totsum2-", k);
			prepareHisto(tmpResult->histo_totsigma, ncol, nrow, "totsigma-", k);
			prepareHisto(tmpResult->histo_occ, ncol, nrow, "occ", k);
		}
		else if (scanType == PixFitScanConfig::scanType::TOT_CALIB) {
			/* Do nothing for the moment. Fill in if processing TOT calib scans in PixFitServer. */
		}
		tmpResultVec.push_back(tmpResult);
	}

	/* Fill full chips in case of mask stepping (dependency on injection pattern!). */
	for (auto& result : resVec) {
	  if (scanType == PixFitScanConfig::scanType::ANALOG ||
				scanType == PixFitScanConfig::scanType::DIGITAL ||
				intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_ANALOG) {
			for (int j = 0; j < result->rawHisto->getWords(); j++) {
				std::vector<int> location = getRowColumn(j, result->getScanConfig());
				tmpResultVec[location[0]]->histo_occ->Fill(location[1], location[2], result->rawHisto->getRawData()[j]);
			}
	  }
		else if (scanType == PixFitScanConfig::scanType::THRESHOLD) {
		        for (int j = 0; j < 2 * (result->getScanConfig()->getNumOfPixels()); j+=2) {
				std::vector<int> location = getRowColumn(j/2, result->getScanConfig());
				tmpResultVec[location[0]]->histo_thresh->Fill(result->thresh_array[j]);
				tmpResultVec[location[0]]->histo_noise->Fill(result->thresh_array[j+1]);
				// only doing bin to vcal conversion for 2D histos for now
				tmpResultVec[location[0]]->histo_thresh2D->Fill(location[1], location[2], result->getScanConfig()->getVcalfromBin(result->thresh_array[j]));
				tmpResultVec[location[0]]->histo_noise2D->Fill(location[1], location[2], result->getScanConfig()->getVcalfromBin(result->thresh_array[j+1], true)); // make sure the scan start is not added to the noise
			}
		        /* Fill chi2 values. */
		        int numOfPixels =  (result->getScanConfig()->getNumOfPixels());
		        int offset = numOfPixels * 2;  // chi2 values are behind mu/sigma value pairs in the array
		        for (int j = 0; j < numOfPixels; j++) {
		        	std::vector<int> location = getRowColumn(j, result->getScanConfig());
					tmpResultVec[location[0]]->histo_chi2->Fill(result->thresh_array[offset + j]);
					tmpResultVec[location[0]]->histo_chi2_2D->Fill(location[1], location[2], result->thresh_array[offset + j]);
		        }
		}
		else if (scanType == PixFitScanConfig::scanType::TOT ||
				intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_TOT) {
		        for (int j = 0; j < result->rawHisto->getWords() / result->getScanConfig()->getWordsPerPixel(); j++) {
				std::vector<int> location = getRowColumn(j, result->getScanConfig());

				/* Compute totmean and totsigma. */
				double occ = (*(result)->rawHisto)(j,0,0);
				double tot = (*(result)->rawHisto)(j,0,1);
				double tot2 = (*(result)->rawHisto)(j,0,2);
				double totmean = 0;
				double totsigma = 0;
				if(occ == 1) {
				  totmean = tot;
				}
				else if (occ > 1) {
				  totmean = tot/occ;
				  if(tot2 > 0) totsigma = sqrt( ((tot2 / occ) - (totmean * totmean))/(occ - 1) );
				} 
				/** @todo: Put this into some bad pixel histo. */
				//else std::cout << "Occupancy is zero!" << std::endl;
				tmpResultVec[location[0]]->histo_totmean->Fill(location[1], location[2], totmean);
				tmpResultVec[location[0]]->histo_totsum->Fill(location[1], location[2], tot);
				tmpResultVec[location[0]]->histo_totsum2->Fill(location[1], location[2], tot2);
				tmpResultVec[location[0]]->histo_totsigma->Fill(location[1], location[2], totsigma);
				tmpResultVec[location[0]]->histo_occ->Fill(location[1], location[2], occ);
			}
		}
		else if (scanType == PixFitScanConfig::scanType::TOT_CALIB) {
			/* Do nothing for the moment. Fill in if processing TOT calib scans in PixFitServer. */
		}
	}

	return tmpResultVec;
}

/** @todo optimize for performance */
std::vector<int> PixFitAssembler::getRowColumn(unsigned int j, std::shared_ptr<const PixFitScanConfig> scanConfig) {
  std::vector<int> location;
  int maskStep = scanConfig->getNumOfTotalMaskSteps();
  //int readoutSteps = scanConfig->getNumOfMaskSteps();
  int maskId = scanConfig->maskId;
  int nrow = scanConfig->NumRow;
  int ncol = scanConfig->NumCol;
  int chip = j / ((ncol * nrow) / maskStep); //  was j / ((ncol * nrow) / readoutSteps); TODO: CHECK THIS!
  int row = trunc((j - chip * 26880 / maskStep) / double(ncol)) * maskStep;
  int col = (j % ncol);
  
  if (j % 2 == 0) {
    row = row + maskId;
  }
  else {
    row = row + maskStep - 1 - maskId;
  }

  //std::cout << "getRowColumn: chip = " << chip << ", maskId = " << maskId << ", maskStep = " << maskStep << ", readoutSteps = " << readoutSteps << std::endl;
  //std::cout << "              j = " << j << ", row = " << row << ", col = " << col << std::endl;
  assert(col < ncol);
  assert(row < nrow);
  assert(chip < 8);

  location.push_back(chip);
  location.push_back(col);
  location.push_back(row);
  return location;
}

void PixLib::PixFitAssembler::setCleanFlag() {
	boost::lock_guard<boost::mutex> lock(m_cleanMutex);
	m_cleanFlag = true;
}

bool PixFitAssembler::cleanAssembler() {
	bool erasedSomething = false;

	/* Iterate over map and remove inner maps matching scan IDs. */
	OuterMap::iterator outerIt = m_results.begin();
	while (outerIt != m_results.end()) {
		if (m_instanceConfig->blacklist.isScanIdListed(outerIt->first.first)) {
			m_results.erase(outerIt++);
			erasedSomething = true;
		}
		else {
			outerIt++;
		}
	}
	return erasedSomething;
}


void PixFitAssembler::prepareHisto(std::shared_ptr<TH2F> &histo, int ncol, int nrow, std::string name, TString id, int bin) {
    TString histoName;
    if (bin >= 0) {
	histoName = name + id + "-" + std::to_string(bin);
    }
    else {
	histoName = name + id;
    }
    histo = std::make_shared<TH2F>(histoName, histoName, ncol, 0, ncol, nrow, 0, nrow);
    histo->GetXaxis()->SetTitle("Column");
    histo->GetYaxis()->SetTitle("Row");
}

