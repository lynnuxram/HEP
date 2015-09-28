/** @file PixFitAssembler.h
 *
 *  Created on: Mar 18, 2014
 *      Author: mkretz
 */

#include <vector>
#include <map>
#include <memory>

#include <boost/thread.hpp>

#include "PixFitWorkQueue.h"
#include "PixFitThread.h"
#include "PixFitScanConfig.h"

#ifndef PIXFITASSEMBLER_H_
#define PIXFITASSEMBLER_H_

namespace PixLib {

class PixFitResult;
class PixFitInstanceConfig;

/** Class that re-assembles incomplete histograms caused by mask-stepping. It takes PixFitResults
 * and buffers them in an internal, dynamically sized hold, until all the necessary data segments
 * have been retrieved; then it re-assembles the segments. If necessary, it will also split the
 * results that contain data from more than one chip, before enqueuing them in the publishQueue.
 * Note: Input and output queue contain objects of the same type, which might be confusing...
 */
class PixFitAssembler : public PixFitThread {
	/* Some typedefs for better readability. */
	typedef std::vector<std::shared_ptr<PixFitResult> > ResultsVector;
	typedef std::pair<PixFitScanConfig::ScanIdType, int> OuterKey;
	typedef std::map<HistoUnit, ResultsVector> InnerMap;
	typedef std::map<OuterKey, InnerMap> OuterMap;

public:
	/** @param resultQueue Pointer to the input queue that is holding the results that (possibly)
	 *  need to be reassembled.
	 * @param publishQueue Pointer to the output queue that holds the assembled results to be published. */
	PixFitAssembler(PixFitWorkQueue<PixFitResult> *resultQueue,
			PixFitWorkQueue<PixFitResult> *publishQueue, PixFitInstanceConfig *instanceConfig);
	virtual ~PixFitAssembler();

	/** Prints out the contents of the hold up to different depths.
	 * @param depth Specifies up to which level the printout will happen. */
	void printMap(int depth);

	/** Tells the assembler to clean up aborted scans as soon as possible. Scan IDs in question
	 * are to be stored in the global BlackList object. */
	void setCleanFlag();

private:
	/** Pointer to the input queue. */
	PixFitWorkQueue<PixFitResult> *resultQueue;

	/** Pointer to the output queue. */
	PixFitWorkQueue<PixFitResult> *publishQueue;

	/** Main thread loop. */
	void loop();

	/** Hold the PixFitResults here. Two nested maps with scanId and HistoUnit keys contain vectors
	 * of results. */
	OuterMap m_results;

	/** Reassembles data.
	 * This function is called once all the necessary data has been gathered. It then combines the
	 * data fragments.
	 * @param resVec Vector containing the complete results for a HistoUnit.
	 * @returns Vector containing PixFitResults that are then enqueued for the publisher. */
	ResultsVector reassemble(ResultsVector &resVec);

	/** Calculates geographical pixel location from flat memory location.
	 * @param j Flat memory location of the pixel, starting from 0.
	 * @param scanConfig
	 * @returns Vector containing chip, column, row number. */
	std::vector<int> getRowColumn(unsigned int j, std::shared_ptr<const PixFitScanConfig> scanConfig);

	/** Cleans up m_results from InnerMap objects belonging to blacklisted scans.
	 * @returns True in case object(s) were removed. */
	bool cleanAssembler();

	/** Flag that indicates whether the cleanAssembler method should be run once the next item
	 * from the queue has been processed. */
	bool m_cleanFlag;

	/** Mutex for accessing the clean assembler flag. */
	boost::mutex m_cleanMutex;

	/** Set up histograms. Helper function.
	 * @param histo Histogram in the PixFitResult.
	 * @param ncol Number of columns.
	 * @param nrow Number of rows.
	 * @param name Name of the histogram.
	 * @param id Identifier for the histogram (aka ROD, crate, etc.)
	 * @param bin Optional bin number. */
	void prepareHisto(std::shared_ptr<TH2F> &histo, int ncol, int nrow, std::string name, TString id, int bin = -1); 
};

} /* end of namespace PixLib */
#endif /* PIXFITASSEMBLER_H_ */
