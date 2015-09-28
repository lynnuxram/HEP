/* @file PixFitAbstractFitter.h
 *
 *  Created on: Oct 13, 2014
 *      Author: mkretz
 */

#ifndef PIXFITABSTRACTFITTER_H_
#define PIXFITABSTRACTFITTER_H_

#include <memory>

namespace PixLib {

class RawHisto;
class PixFitResult;

/** Abstract base class for fitter implementations. */
class PixFitAbstractFitter {
public:
	PixFitAbstractFitter() {};
	virtual ~PixFitAbstractFitter() {};

	/** Performs an S-Curve fit on the raw histogram and returns a PixFitResult.
	 * @param histo Raw histogram containing data for the S-curve fit.
	 * @returns PixFitResult containing the fitted parameters and errors. */
	virtual std::shared_ptr<PixFitResult> fit(std::shared_ptr<RawHisto> histo) = 0;
};

} /* namespace PixLib */

#endif /* PIXFITABSTRACTFITTER_H_ */
