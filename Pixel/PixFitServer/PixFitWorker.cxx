/* @file PixFitWorker.cxx
 *
 *  Created on: Dec 3, 2013
 *      Author: mkretz, marx
 */

#include <memory>

#include "PixFitAbstractFitter.h"
#include "PixFitFitter_lmfit.h"
#include "PixFitWorker.h"
#include "PixFitWorkQueue.h"
#include "PixFitResult.h"
#include "RawHisto.h"

using namespace PixLib;

PixFitWorker::PixFitWorker(PixFitWorkQueue<RawHisto> *histoQueue,
		PixFitWorkQueue<PixFitResult> *resultQueue, FitMethod fitterType) {
	this->m_histoQueue = histoQueue;
	this->m_resultQueue = resultQueue;
	this->m_threadName = "worker";
	this->m_fitter = createFitter(fitterType);
}

PixFitWorker::~PixFitWorker() {
}

std::unique_ptr<PixFitAbstractFitter> PixFitWorker::createFitter(FitMethod fitterType) {
switch (fitterType) {
        case FitMethod::FIT_LMMIN:
                return std::unique_ptr<PixFitAbstractFitter>(new PixFitFitter_lmfit);
                break;
        default:
                return std::unique_ptr<PixFitAbstractFitter>(new PixFitFitter_lmfit);
                break;
        }
}

void PixFitWorker::loop() {

	while (1) {
		/* Get work. */
		std::shared_ptr<RawHisto> histo = m_histoQueue->getWork();

		/* Instantiate PixFitFitter. */
		std::shared_ptr<PixFitResult> result;

		/* Make decision whether fit is needed or not.
		 * (so far only analog/digital/threshold scans supported) */
		PixFitScanConfig::scanType scanType = histo->getScanConfig()->findScanType();
		PixFitScanConfig::intermediateType intermediateType = histo->getScanConfig()->intermediate;

		/* ANALOG & DIGITAL & TOT & INTERMEDIATEs */
		if (scanType == PixFitScanConfig::scanType::ANALOG
				|| scanType == PixFitScanConfig::scanType::DIGITAL
				|| scanType == PixFitScanConfig::scanType::TOT
				|| intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_ANALOG
				|| intermediateType == PixFitScanConfig::intermediateType::INTERMEDIATE_TOT) {
			std::shared_ptr<PixFitResult> tmpResult = std::make_shared<PixFitResult>(histo->getScanConfig());
			tmpResult->rawHisto = histo;
			result = tmpResult;
		}
		/* THRESHOLD */
		else if (scanType == PixFitScanConfig::scanType::THRESHOLD) {
			result = m_fitter->fit(histo);
		}
		/* TOT_CALIB
		 * We currently don't process the data for TOT_CALIB type scans, as this is done
		 * externally with INTERMEDIATE histograms. We forward an empty PixFitResult anyways as a
		 * dummy work package to be able to signal the successful publishing of all intermediate
		 * histograms by the PixFitPublisher at the end of a scan. */
		else if (scanType == PixFitScanConfig::scanType::TOT_CALIB) {
			result = std::make_shared<PixFitResult>(histo->getScanConfig());
		}

		/* Enqueue PixFitResult object. */
		m_resultQueue->addWork(result);
	}
}
