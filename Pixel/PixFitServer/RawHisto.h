/* @file RawHisto.h
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz
 */

#ifndef RAWHISTO_H_
#define RAWHISTO_H_

#include <stdint.h> // change to cstdint for C++11
#include <memory>

#include "PixFitWorkPackage.h"
#include "PixFitScanConfig.h"

namespace PixLib {

/** Stores the reformatted (partial) histograms from the ROD. It is filled by a networking thread and
 * then put into the Fit-Queue for processing. The memory layout is not hidden and could be a
 * starting point for further optimization. */
class RawHisto : public PixFitWorkPackage {
public:
        /** Type of a word for a histogram. */
	typedef uint32_t histoWord_type;

        /** @param scanConfig Pointer to associated PixFitScanConfig. */
	RawHisto(std::shared_ptr<const PixFitScanConfig> scanConfig);

	virtual ~RawHisto();

        /** Allocates the amount of memory that is needed to store the histogram. 
         * @returns 0 on success, 1 if memory has already been allocated, 2 on failure. */
	int allocateMemory();

        /** Getter for PixFitScanConfig. */
	std::shared_ptr<const PixFitScanConfig> getScanConfig() const;

        /** Getter for raw memory.
         * @returns Pointer to raw memory. */
	histoWord_type* getRawData();

        /** @returns Number of bytes that are allocated for the histogram. */
	int getSize() const;

        /** @returns Number of words in the histogram. */
	int getWords() const;

	/** Overloading () for accessing histogram subset. 
         * @param pixel The pixel in question (flat address).
         * @returns Pointer to a pixel. */
	histoWord_type* operator() (int pixel);

	/** Overloading () for accessing histogram subset. 
         * @param pixel The pixel in question (flat address).
         * @param bin The bin of the histogram.
         * @returns Pointer to a particular bin of a pixel. */
	histoWord_type* operator() (int pixel, int bin);

	/** Overloading () for accessing histogram data (for example pixel 2110, bin 5, word 2). 
         * @param pixel The pixel in question (flat address).
         * @param bin The bin of the histogram.
         * @param word The data word of the particular pixel/bin combination.
         * @returns Data word. */
	histoWord_type& operator() (int pixel, int bin, int word);

	/** Get the scan ID belonging to the work object. */
	virtual PixFitScanConfig::ScanIdType getScanId() const;

	/** @todo Add convenience functions to access pixel data */

private:
        /** Helper function that allocates memory for the histogram data.
         * @param size Size of the histogram in bytes.
         * @returns 0 on success, 1 if memory has already been allocated, 2 on failure. */
	int alloc(int size);

        /** Pointer to the raw memory for the histogram data. */
	histoWord_type *m_rawData;

        /** Size of the raw memory in bytes. */
	int m_size;

        /** Pointer to the associated PixFitScanConfig. */
	std::shared_ptr<const PixFitScanConfig> m_scanConfig;

	/* We could look up these values from the scanConfig, but rather use them here for caching. */
        /** Number of pixels in the histogram. */
	int m_pixels;

        /** Number of data words used per pixel. */
	int m_wordsPerPixel;

        /** Number of bytes used per pixel. */
	int m_bytesPerPixel;

        /** Number of bins in the histogram. */
	int m_bins;
};
} /* end of namespace PixLib */

#endif /* RAWHISTO_H_ */
