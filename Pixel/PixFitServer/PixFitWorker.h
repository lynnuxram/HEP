/* @file PixFitWorker.h
 *
 *  Created on: Dec 3, 2013
 *      Author: mkretz
 */

#ifndef PIXFITWORKER_H_
#define PIXFITWORKER_H_

#include <memory>

#include "PixFitWorkQueue.h"
#include "PixFitThread.h"

namespace PixLib {

class PixFitAbstractFitter;
class RawHisto;

/** Different fitting methods matching PixFitAbstractFitter derived fitters. */
enum class FitMethod : int {
	FIT_NONE,
	FIT_LMMIN,
	FIT_LEVMAR,
	FIT_DSP,
	FIT_DSP_LUT,
	FIT_ROOT,
	FIT_CUDA
};

/** Represents a worker thread that does fitting. It is created by PixFitManager and retrieves work
 * packages from the histoQueue and puts the results in the resultQueue.
 * One can configure the fitter being used (i.e. which class derived from PixFitAbstractFitter will
 * do the fit).*/
class PixFitWorker : public PixFitThread {
public:
  /** @param histoQueue Pointer to the input queue that is holding RawHistos from PixFitNet.
   * @param resultQueue Pointer to the output queue that holds processed histogram data. */
  PixFitWorker(PixFitWorkQueue<RawHisto> *histoQueue, PixFitWorkQueue<PixFitResult> *resultQueue, FitMethod fitterType = FitMethod::FIT_LMMIN);

  virtual ~PixFitWorker();

private:
	/** Main loop that waits for work, processes it, publishes results. */
  	void loop();

  	/** Pointer to the input queue. */
	PixFitWorkQueue<RawHisto> *m_histoQueue;

	/** Pointer to the output queue. */
	PixFitWorkQueue<PixFitResult> *m_resultQueue;

	/** Pointer to the Fitter instance. */
	std::unique_ptr<PixFitAbstractFitter> m_fitter;

	/** Creates a fitter instance. */
	std::unique_ptr<PixFitAbstractFitter> createFitter(FitMethod fitMethod);
};

} /* end of namespace PixLib */

#endif /* PIXFITWORKER_H_ */
