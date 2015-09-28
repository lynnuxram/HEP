/* @file PixFitResult.cxx
 *
 *  Created on: Dec 3, 2013
 *      Author: mkretz, marx
 */

#include <memory>

#include "PixFitResult.h"
#include "PixFitScanConfig.h"

using namespace PixLib;

PixFitResult::PixFitResult(std::shared_ptr<const PixFitScanConfig> scanConfig) {
	this->m_scanConfig = scanConfig;
}

PixFitResult::~PixFitResult() {
}

std::shared_ptr<const PixFitScanConfig> PixFitResult::getScanConfig() const {
	return m_scanConfig;
};

PixFitScanConfig::ScanIdType PixFitResult::getScanId() const {
	return m_scanConfig->scanId;
}
