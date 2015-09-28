/* @file PixFitScanConfig.cxx
 *
 *  Created on: Dec 17, 2013
 *      Author: mkretz, marx
 */

#include <map>
#include <memory>
#include <ers/ers.h>

#include "PixFitScanConfig.h"
#include "PixController/PixScan.h"

using namespace PixLib;

/** @todo Clean up globals, putting in unnamed namespace for now. */
/* These are only for use with slave emulator. */
namespace {
	int nchips = 8;
	int masknum = 8;
	int nbins = 101;
	int ntrigs = 100;
	int masksteps = 8;
	auto nscantype = PixFitScanConfig::scanType::THRESHOLD;
	auto nreadoutmode = PixFitScanConfig::readoutMode::ONLINE_OCCUPANCY;
}

PixFitScanConfig::PixFitScanConfig(bool slaveEmu = false) {
	intermediate = intermediateType::INTERMEDIATE_NONE;
	binNumber = -1;
	m_slaveEmu = slaveEmu;

	//ERS_DEBUG(0, "Created PixFitScanConfig at " << std::hex << this)
}

PixFitScanConfig::~PixFitScanConfig() {
	//ERS_DEBUG("DEBUG: Deleted PixFitScanConfig at " << std::hex << this)
}

/* Returns how many bytes per pixel are sent from the slave via ethernet. (e.g. 1, 4, 8) */
int PixFitScanConfig::getBytesPerPixel() const {
	return m_readoutModeBytes.at(getReadoutMode());
}

/** @todo Make generic. */
int PixFitScanConfig::getNumOfPixels() const {
  int ms;
  int tmpnchips;

  if (m_slaveEmu) {
    ms = masknum;
    tmpnchips = nchips;
  }
  else {
	  ms = PixLib::PixModuleGroup::translateMaskSteps(this->pixScanConfig->getMaskStageTotalSteps());
	  tmpnchips = this->getNumOfChips();
  }

  int pixels = 26880 * tmpnchips / ms;
  return pixels;
}

int PixFitScanConfig::getInjections() const {
  if (m_slaveEmu) {
	  return ntrigs;
  }
  else {
	  return this->pixScanConfig->getRepetitions();
  }
}

int PixFitScanConfig::getWordsPerPixel() const {
	return m_scanTypeWords.at(findScanType());
}

PixFitScanConfig::scanType PixFitScanConfig::findScanType() const {
  if (m_slaveEmu) {
    return nscantype;
  }
  else {
	return findScanType(this->pixScanConfig);
  }
}

PixFitScanConfig::scanType PixFitScanConfig::findScanType(std::shared_ptr<PixLib::PixScan> pixScan) {
	  // N.B. order is important here as OCCUPANCY is also filled even for THRESHOLD scans
	  if (pixScan->getHistogramFilled(PixLib::EnumHistogramType::SCURVE_MEAN)
			  || pixScan->getHistogramFilled(PixLib::EnumHistogramType::SCURVE_SIGMA)
			  || pixScan->getHistogramFilled(PixLib::EnumHistogramType::SCURVE_CHI2) )
	    return scanType::THRESHOLD;

	  else if (pixScan->getHistogramFilled(PixLib::EnumHistogramType::TOT_MEAN)
			  || pixScan->getHistogramFilled(PixLib::EnumHistogramType::TOT_SIGMA)
			  || pixScan->getHistogramFilled(PixLib::EnumHistogramType::TOTAVERAGE) )
	    return scanType::TOT; // return scanType::TOT_CALIB; don't use intermediate histos for now; complications with scan finish flag

	  else if (pixScan->getHistogramFilled(PixLib::EnumHistogramType::OCCUPANCY) ) {
	    return scanType::DIGITAL; // removed ANALOG as it's the same for all the FitFarm cares
	  }

	  else {
	    ERS_INFO("This scan type is not yet accommodated for, returning digital type as default.");
	    return scanType::DIGITAL;
	  }
}


PixFitScanConfig::readoutMode PixFitScanConfig::getReadoutMode() const {
  if (m_slaveEmu) {
	  return nreadoutmode;
  }
  else {
	  if (findScanType() == scanType::TOT || findScanType() == scanType::TOT_CALIB) {
		return readoutMode::LONG_TOT;
	  }
	  else {
		return readoutMode::ONLINE_OCCUPANCY;
	  }
	  /** @todo Accommodate for these two readout modes as well */
	  //return readoutMode::OFFLINE_OCCUPANCY;
	  //return readoutMode::SHORT_TOT;
  }
}

int PixFitScanConfig::getNumOfBins() const {
  if(m_slaveEmu) {
	  return nbins;
  }
  else {
	  int bins = 1; // ana and digi hardcoded to 1 as getLoopVarNSteps gives 0
	  if (this->intermediate != intermediateType::INTERMEDIATE_NONE) {
			/* For intermediate types we only use one bin. */
			bins = 1;
	  }
	  else if (this->findScanType() == scanType::THRESHOLD || this->findScanType() == scanType::TOT_CALIB) {
			bins = this->pixScanConfig->getLoopVarNSteps(0);
	  }
	  return bins;
  }
}

int PixFitScanConfig::getNumOfTotalMaskSteps() const {
  if (m_slaveEmu) {
	  return masknum;
  }
  else {
	  return PixLib::PixModuleGroup::translateMaskSteps(this->pixScanConfig->getMaskStageTotalSteps());
  }
}

int PixFitScanConfig::getNumOfTotalMaskSteps(std::shared_ptr<PixLib::PixScan> pixScan, bool slaveEmu) {
  if (slaveEmu) {
          return masknum;
  }
  else {
          return PixLib::PixModuleGroup::translateMaskSteps(pixScan->getMaskStageTotalSteps());
  }
}

int PixFitScanConfig::getNumOfMaskSteps(std::shared_ptr<PixLib::PixScan> pixScan, bool slaveEmu) {
  if (slaveEmu) {
          return masksteps;
  }
  else {
          return pixScan->getMaskStageSteps();
  }
}

/* This version is for PixFitPublisher */
int PixFitScanConfig::getNumOfMaskSteps() const{
  if (m_slaveEmu) {
	  return masksteps;
  }
  else {
	  return this->pixScanConfig->getMaskStageSteps();
  }
}

int PixFitScanConfig::getNumOfChips() const {
  if (m_slaveEmu) {
	  return nchips;
  }
  else {
	  unsigned int modmask = this->modMask;
	  bool powertwo = !(modmask == 0) && !(modmask & (modmask - 1));
	  if (powertwo) {
		  // return 1;
		  // doing 8 now for all cases as Rx will otherwise be wrong for single FE case...
		  // somebody would have to change this in the slave histogrammer if read-out should be optimized for this case
		  return 8;
	  }
	  else {
		  return 8;
	  }
  }
}

double PixFitScanConfig::getVcalfromBin(double i, bool isNoise) const{
  int n = this->pixScanConfig->getLoopVarNSteps(0);
  int min = this->pixScanConfig->getLoopVarMin(0);
  int max = this->pixScanConfig->getLoopVarMax(0);
  
  double vcal = min + ( (double)(max - min) / (n - 1) ) * i;

  // If this is the noise, then subtract off the min, so that the first step is not added to the noise.
  if(isNoise) vcal-=min;

  //ERS_LOG( "in getVcalfromBin = " << vcal << " min: " << min << " max: " << max << " n: "   << n )
  return vcal;
}

PixFitScanConfig::ScanIdType PixFitScanConfig::getScanId() const {
	return scanId;
}

bool PixFitScanConfig::doIntermediateHistos() const {
  /* Check if it is a threshold scan and occupancy histograms are needed on top as intermediate. */
  if (findScanType() == scanType::THRESHOLD
		  && this->pixScanConfig->getHistogramFilled(PixLib::EnumHistogramType::OCCUPANCY)) {
	  return true;
  }
  /* Currently TOT calibration scans are analyzed by PixAnalalysis and they therefore need the
   * intermediate histograms. */
  else if (findScanType() == scanType::TOT_CALIB) {
	  return true;
  }
  return false;
}
