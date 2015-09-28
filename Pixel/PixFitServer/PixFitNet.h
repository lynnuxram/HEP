/** @file PixFitNet.h
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz
 */

#ifndef PIXFITNET_H_
#define PIXFITNET_H_

#include <fstream>
#include <memory>

#include <boost/thread.hpp>

#include "PixUtilities/PixMessages.h"

#include "iblSlaveNetCmds.h"

#include "PixFitThread.h"
#include "PixFitWorkQueue.h"
#include "RawHisto.h"

namespace PixLib {

class PixFitNetConfiguration;
class PixFitScanConfig;
class PixFitInstanceConfig;

/** This class represents a network socket for a ROD histo-unit to FitFarm connection. It is created
 * and controlled by PixFitManager. PixFitScanConfig objects are enqueued and the incoming data is
 * reformatted according to the scan configuration and put in RawHistos. These are subsequently
 * enqueued in the fitQueue for processing. */
class PixFitNet : public PixFitThread {
public:
	/** @param queue Pointer to the fitQueue where completely received histograms will be put.
	 * @param netConfig Configuration for this particular PixFitNet instance.
	 * @param instanceConfig Handle to the PixFitServer instance configuration. */
	PixFitNet(PixFitWorkQueue<RawHisto> *queue,
			PixFitNetConfiguration const *netConfig,
			PixFitInstanceConfig *instanceConfig);
	virtual ~PixFitNet();

	/** Enqueues a PixFitScanConfig object. */
	void putScanConfig(std::shared_ptr<const PixFitScanConfig> scanConfig);

	/** Get a pointer to the network configuration.
	 * @returns Pointer to PixFitNetConfiguration of the PixFitNeto object/thread. */
	const PixFitNetConfiguration* getConfig() const;

	/** Reset the networking thread.
	 * Triggers graceful closing of possibly open socket and discarding of current PixFitScanConfig
	 * and RawHisto objects.
	 * @param ifBlacklisted Decides whether action happens only if current scan object is blacklisted. */
	void resetNetwork(bool ifBlacklisted = false);

private:
    void loop();

    /** Reads command from socket.
     * @return -1 on socket error, 0 if socket was remotely closed. 1 for complete command. */
    int readCommand();

    /** Processes commands.
     * @return 0 If more data (bins) are needed for histogram. 1 in case of error. 2 if histogram
     * is completed. */
    int procRequest();

    /** Converts command struct from network byte order to host byte order.
     * @param cmd Input RodSlvTcpCmd in network byte order.
     * @param hostCmd Output RodSlvTcpCmd in host byte order. */
    void cmdToHost(const RodSlvTcpCmd &cmd, RodSlvTcpCmd &hostCmd);

    /** Configuration for this PixFitNet object. */
    PixFitNetConfiguration const *m_configuration;

    /** Pointer to fitQueue. */
    PixFitWorkQueue<RawHisto> *m_queue;

    /** Socket number of the active connection for this PixFitNet thread. */
    int m_rodSock;

    /** Stores the current bin while receiving a histogram. Used to determine correct memory location
     * while filling a RawHisto. */
    int m_currentBin;

    /** Receive-buffer pointer. */
    unsigned char *m_rxBuf;

    /** Receive-buffer size.
    * @TODO: Make IBL/Pixel agnostic. */
    static const int s_bufSize = 4000000; // 4M as receive buffer

    /** Histogram buffer. Maximum buffer for one bin is maxBytesPerPixel * chipSize * numOfChips.
     * @TODO: Make IBL/Pixel agnostic. */
    char m_histoBuf[2 * sizeof(RawHisto::histoWord_type) * 26880 * 8];

    /** Internal queue where PixFitScanConfig objects are stored. */
    PixFitWorkQueue<const PixFitScanConfig> m_scanConfigQueue;

    /** Current PixFitScanConfig being processed. */
    std::shared_ptr<const PixFitScanConfig> m_scanConfig;

    /** Current RawHisto being filled. */
    std::shared_ptr<RawHisto> m_rawHisto;

    /** File for dumping raw network data to disk. */
    std::ofstream m_dumpFile;

    /** Reset flag that triggers closing the possibly open socket and cleaning up member variables.
     * Value of 1 triggers action all the time, 2 only if current scan config being processed is blacklisted. */
    int m_resetFlag;

    /** Checks if reset flag has been set and does cleanup call if necessary.
     * @returns true if reset is performed. */
    bool resetCheck();

	/** Mutex for accessing the reset flag. */
	boost::mutex m_resetMutex;

	/** Clean up networking. Assumes lock on m_resetMutex for writing to m_resetFlag. */
	void cleanup();

	/** Milliseconds that poll waits until timeout before recv() calls. */
	static const int s_timeout = 1000;

    /* String containing histogram unit identifier for this PixFitNet. */
    std::string m_histoUnitString;

    /* PixMessages object to report in GUI */
    PixMessages *m_msg; 
    PixMessages &getMrs(std::shared_ptr<const PixFitScanConfig> scanConfig);

};
} /* end of namespace PixLib */

#endif /* PIXFITNET_H_ */
