/* @file PixFitResult.h
 *
 *  Created on: Dec 3, 2013
 *      Author: mkretz, marx
 */

#ifndef PIXFITRESULT_H_
#define PIXFITRESULT_H_

#include <memory>

#include "TH1.h"
#include "TH2.h"

#include "PixFitWorkPackage.h"
#include "PixFitScanConfig.h"

namespace PixLib {

class RawHisto;

/** This is a container for processed histogram data (almost) ready for publishing.
 * The ROOT histograms are created by the PixFitAssembler and are empty otherwise. thresh_array and
 * rawHisto hold data that is being shipped between PixFitWorker/Fitter and PixFitAssembler. */
class PixFitResult : public PixFitWorkPackage {
public:
	/** @param scanConfig The associated PixFitScanConfig. */
	PixFitResult(std::shared_ptr<const PixFitScanConfig> scanConfig);

	virtual ~PixFitResult();

	/** Get a handle on the associated PixFitScanConfig object.
	 * @returns Pointer to associated PixFitScanConfig. */
	std::shared_ptr<const PixFitScanConfig> getScanConfig() const;

	/** The RawHisto is used for occupancy-only scans before the assembler. */
	std::shared_ptr<RawHisto> rawHisto;

	/** Holds the results from a threshold scan fit. */
	std::unique_ptr<double[]> thresh_array;

	/* Resulting ROOT histograms created by PixFitAssembler. */
	std::shared_ptr<TH2F> histo_occ;

	std::shared_ptr<TH1F> histo_thresh;
	std::shared_ptr<TH2F> histo_thresh2D;

	std::shared_ptr<TH1F> histo_noise;
	std::shared_ptr<TH2F> histo_noise2D;

	std::shared_ptr<TH1F> histo_chi2;
	std::shared_ptr<TH2F> histo_chi2_2D;

	std::shared_ptr<TH2F> histo_totmean;
	std::shared_ptr<TH2F> histo_totsum;
	std::shared_ptr<TH2F> histo_totsum2;
	std::shared_ptr<TH2F> histo_totsigma;

	/** Chip ID after reassembly: 0-7.
	 * @todo This is IBL-specific. */
	unsigned int chipId;

	/** Get the scan ID belonging to the work object. */
	virtual PixFitScanConfig::ScanIdType getScanId() const;

private:
	/** Handle for the corresponding PixFitScanConfig. */
	std::shared_ptr<const PixFitScanConfig> m_scanConfig;
};
} /* end of namespace PixLib */

#endif /* PIXFITRESULT_H_ */
