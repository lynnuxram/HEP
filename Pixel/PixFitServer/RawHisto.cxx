/* @file RawHisto.cxx
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz
 */

#include <cstdlib> // for malloc/free
//#define NDEBUG
#include <cassert>
#include <stdint.h> // change to cstdint for C++11
#include <memory>

#include "RawHisto.h"

using namespace PixLib;

RawHisto::RawHisto(std::shared_ptr<const PixFitScanConfig> scanConfig) {
	m_rawData = nullptr;
	m_size = 0;
	m_scanConfig = scanConfig;

	/* Get information from PixFitScanConfig. */
	m_pixels = scanConfig->getNumOfPixels();
	m_wordsPerPixel = scanConfig->getWordsPerPixel();
	m_bytesPerPixel = m_wordsPerPixel * sizeof(histoWord_type);
	m_bins = scanConfig->getNumOfBins();
}

RawHisto::~RawHisto() {
	free(m_rawData);
}

int RawHisto::allocateMemory() {
	int size = 0;
	if (m_scanConfig->intermediate != PixFitScanConfig::intermediateType::INTERMEDIATE_NONE) {
		size = m_pixels * m_bytesPerPixel;
	}
	else {
		size = m_pixels * m_bytesPerPixel * m_bins;
	}
	return alloc(size);
}

std::shared_ptr<const PixFitScanConfig> RawHisto::getScanConfig() const {
	return m_scanConfig;
}

RawHisto::histoWord_type* RawHisto::getRawData() {
	return m_rawData;
}
int RawHisto::getSize() const {
	return m_size;
}

int RawHisto::getWords() const {
	return m_size/sizeof(histoWord_type);
}

RawHisto::histoWord_type* RawHisto::operator ()(int pixel) {
	assert(pixel >= 0 && pixel < m_pixels);
	return &m_rawData[pixel * m_bins];
}

RawHisto::histoWord_type* RawHisto::operator ()(int pixel, int bin) {
	assert(pixel >= 0 && pixel < m_pixels);
	assert(bin >= 0 && bin < m_bins);
	return &m_rawData[pixel * m_bins * m_wordsPerPixel + bin * m_wordsPerPixel];
}

RawHisto::histoWord_type& RawHisto::operator ()(int pixel, int bin, int word) {
	assert(pixel >= 0 && pixel < m_pixels);
	assert(bin >= 0 && bin < m_bins);
	assert(word >= 0 && word < m_wordsPerPixel);
	assert(pixel * m_bins * m_wordsPerPixel + bin * m_wordsPerPixel + word < m_size/static_cast<int>(sizeof(histoWord_type)));
	return m_rawData[pixel * m_bins * m_wordsPerPixel + bin * m_wordsPerPixel + word];
}

int RawHisto::alloc(int size) {
	/* Check if memory has already been allocated. */
	if (m_rawData != 0) {
		return 1;
	}
	else {
		/* Allocate memory and return 0 on success. */
		m_rawData = (histoWord_type *) malloc(size);
		if (m_rawData != 0) {
			this->m_size = size;
			return 0;
		}
		else {
			return 2;
		}
	return 0;
	}
}

PixFitScanConfig::ScanIdType RawHisto::getScanId() const {
	return m_scanConfig->scanId;
}
