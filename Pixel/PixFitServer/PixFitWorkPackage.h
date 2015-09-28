/* @file PixFitWorkPackage.h
 *
 *  Created on: Aug 14, 2014
 *      Author: mkretz
 */

#ifndef PIXFITWORKPACKAGE_H_
#define PIXFITWORKPACKAGE_H_

namespace PixLib {

/** Abstract base class for classes that are enqueued in one of the PixFitWorkQueues. */
class PixFitWorkPackage {
public:
	PixFitWorkPackage() {};
	virtual ~PixFitWorkPackage() {};

	/** Typedef this once more, as including PixFitScanConfig.h generates loop.
	 * @todo Move this typedef to a central place. */
	typedef int ScanIdType;

	/** Get the scan ID belonging to the work object. */
	virtual ScanIdType getScanId() const = 0;
};

} /* end of namespace PixLib */

#endif /* PIXFITWORKPACKAGE_H_ */
