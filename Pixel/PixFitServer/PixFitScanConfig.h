/* @file PixFitScanConfig.h
 *
 *  Created on: Dec 17, 2013
 *      Author: mkretz, marx
 */

#ifndef PIXFITSCANCONFIG_H_
#define PIXFITSCANCONFIG_H_

#include <string>
#include <map>
#include <memory>

#include "PixController/PixScan.h"

#include "PixFitNetConfiguration.h"
#include "PixFitWorkPackage.h"

namespace PixLib {

/** Contains the relevant information for handling the work packages that are being passed
 * around in the FitFarm. Information about memory sizes for allocation by PixFitNet as 
 * well as fitting methods and publishing information should be stored here.
 * It has a pointer to the PixScan object associated with a whole scan. 
 * PixFitScanConfig objects are created by the PixFitManager as soon as a new scan request
 * has arrived.
 * @todo Check for invalid combinations of readoutMode and scanType? */
class PixFitScanConfig : public PixFitWorkPackage {
public:
	/** @param slaveEmu Indicates whether the slave emulator is being used. */
	PixFitScanConfig(bool slaveEmu);

	virtual ~PixFitScanConfig();

	/** Contains the four basic scantypes we are dealing with. */
	enum class scanType {THRESHOLD, ANALOG, DIGITAL, TOT, TOT_CALIB};

	/** Contains the four readout modes / data formats that the IBL histogrammer supports. */
	enum class readoutMode {ONLINE_OCCUPANCY = 0, OFFLINE_OCCUPANCY, SHORT_TOT, LONG_TOT};

	/** Typedef for the unique scan ID. */
	typedef int ScanIdType;

	/** @returns Number of pixels being histogrammed. */
	int getNumOfPixels() const;

	int getNumOfBins() const;

	/** Returns the mask step setting of the histogramming unit.
	 * @returns Returns either 1, 2, 4, or 8 for FE-I4.
	 * @todo Document the difference to getNumOfMaskSteps() */
	int getNumOfTotalMaskSteps() const;

	/** Returns the mask step setting of the histogramming unit.
	 * @returns Returns either 1, 2, 4, or 8 for FE-I4.
	 * Static function that investigates a PixScan object and returns relevant info even before
	 * a PixScanConfig is created. */
	static int getNumOfTotalMaskSteps(std::shared_ptr<PixLib::PixScan> pixScan, bool slaveEmu);

	/** Returns the number of steps the data is sent in, determines the number of RawHisto items needed
	 * in PixFitManager::setupScan() */
	int getNumOfMaskSteps() const;

	/** Returns the number of steps the data is sent in, determines the number of RawHisto items needed
	 * in PixFitManager::setupScan()
	 * Static function that investigates a PixScan object and returns relevant info even before
	 * a PixScanConfig is created. */
	static int getNumOfMaskSteps(std::shared_ptr<PixLib::PixScan> pixScan, bool slaveEmu);

	/** Returns how many chips the histogramming unit is configured to sample.
	 * @returns 1 or 8, depending on histogrammer configration. */
	int getNumOfChips() const;

	/** Number of charge injections per histogram step. Needed for fitting for example.
	 * @returns Charge injections, >0 */
	int getInjections() const;

	/** Returns the histogrammer readout mode. Relevant for re-formatting incoming data from the
	 * histogramming units via ethernet. */
	readoutMode getReadoutMode() const;

	/** Returns how many bytes are used by the histogramming unit per pixel when shipping
	 * histograms via Ethernet. */
	int getBytesPerPixel() const;

	/** Returns the type of scan. */
	scanType findScanType() const;

	/** Static version, returns scantype of a PixScan object. */
	static scanType findScanType(std::shared_ptr<PixScan> pixScan);

	/** Returns how many words are used per pixel in the RawHisto. (e.g. 1 for OCC or 3 for TOT)
	 * This is needed for allocating memory. */
	int getWordsPerPixel() const;

	/** Pointer to the associated PixScan object. */
	std::shared_ptr<PixLib::PixScan> pixScanConfig;

	/** The scan ID to which this particular object belongs. */
	ScanIdType scanId;

	/** The FitFarm internal scan ID to which this object belongs. */
	int fitFarmId;

	/** The module mask that determines which modules are involved/active. */
	unsigned int modMask;

	/** Used to identify the work packages for scans involving mask stepping. Starts at 0. */
	int maskId;

	/** Allows to identify the histogramming unit to which this config belongs. */
	HistoUnit histogrammer;

	/* Information solely relevant to PixFitPublisher. */
	std::string partitionName;
	std::string serverName;
	std::string providerName;

	/** Number of rows of a FE chip. Needed for formatting histograms.
	 * @todo Make more abstract. */
	int NumRow;

	/** Number of columns of a FE chip. Needed for formatting histograms.
	 * @todo Make more abstract. */
	int NumCol;

	/** Indicates whether intermediate histograms (for example occupancy histograms of every bin
	 * during a threshold scan) should be published to OH. binNumber to be used for naming the
	 * histograms and in the PixFitAssembler. */
	bool doIntermediateHistos() const;

	/** Different types of intermediate histograms available for publishing. */
	enum class intermediateType {INTERMEDIATE_NONE, INTERMEDIATE_ANALOG, INTERMEDIATE_TOT};

	/** Type of intermediate histograms. */
	intermediateType intermediate;

	/** The bin number of the histogram for more advanced scans such as threshold. This is used
	 * in case intermediate histograms need to be published and is then used for naming them.
	 * We also use this to keep track of the bin for a TOT_CALIBRATION. */
	int binNumber;

	/** Calculates Vcal value from meaningless bin value using min and max of Vcal range set in Console. */
	double getVcalfromBin(double i, bool isNoise=false) const;

	/** Get the scan ID belonging to the work object. */
	virtual ScanIdType getScanId() const;

private:
	/** Contains scan type sizes in words. */
	const std::map<scanType, int> m_scanTypeWords {
		{scanType::THRESHOLD, 1},
		{scanType::ANALOG, 1},
		{scanType::DIGITAL, 1},
		{scanType::TOT, 3},
		{scanType::TOT_CALIB, 3}
	};

	/** Contains readout mode sizes in bytes. */
	const std::map<readoutMode, int> m_readoutModeBytes {
		{readoutMode::OFFLINE_OCCUPANCY, 1},
		{readoutMode::ONLINE_OCCUPANCY, 4},
		{readoutMode::SHORT_TOT, 4},
		{readoutMode::LONG_TOT, 8}
	};

	/** Initializes the m_scanTypeWords and m_readoutModeBytes maps. */
	void initMaps();

	/** Indicates whether emulator is being used as a client. */
	bool m_slaveEmu;
};

} /* end of namespace PixLib */

#endif /* PIXFITSCANCONFIG_H_ */
