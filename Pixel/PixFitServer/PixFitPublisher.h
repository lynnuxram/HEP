/* @file PixFitResult.h
 *
 *  Created on: Dec 10, 2013
 *      Author: mkretz, marx
 */

#ifndef PIXFITPBUBLISHER_H_
#define PIXFITPUBLISHER_H_

#include <oh/OHRootProvider.h>

#include "PixHistoServer/PixHistoServerInterface.h"

#include "PixFitWorkQueue.h"
#include "PixFitThread.h"

namespace PixLib {

class PixFitResult;
class PixFitScanConfig;
class PixFitInstanceConfig;

/** Handles the publishing of processed histograms to OH. PixFitPublisher gets histograms via the
 * publishQueue, decides on the naming scheme, and publishes them. */
class PixFitPublisher : public PixFitThread {
 public:
  /** @param publishQueue The publishQueue that supplies the results to the publisher. */
  PixFitPublisher(PixFitWorkQueue<PixFitResult> *publishQueue, PixFitInstanceConfig *instanceConfig);

  virtual ~PixFitPublisher();

 protected:
  std::shared_ptr<PixHistoServerInterface> m_histoInt;

 private:
  void loop();

  /** Converts FE connection details to an Rx channel.
   * @param slave ROD slave, either 0 or 1.
   * @param histounit Unit on the FPGA, either 0 or 1.
   * @param chip The FE chip that is being referred to, 0 to 7 (for IBL).
   * @returns The RX channel ranging from 0 to 31. */
  int RxConversion(int slave, int histounit, int chip);

  /** Pointer to input queue. */
  PixFitWorkQueue<PixFitResult> *m_publishQueue;
};

} /* end of namespace PixLib */

#endif /* PIXFITPUBLISHER_H_ */
