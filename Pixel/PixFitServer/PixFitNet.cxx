/** @file PixFitNet.cxx
 *
 *  Created on: Nov 28, 2013
 *      Author: mkretz
 */

#include <iostream>
#include <fstream>	// for dumping raw network data
#include <stdio.h>	// TODO: used for printfs
#include <cstdlib> // for malloc
#include <memory>
#include <string>
#include <functional>
#include <cassert>

/* Networking via POSIX sockets */
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <boost/thread.hpp>
#include <ers/ers.h>

#include "iblSlaveNetCmds.h"
#include "rodHisto.hxx"

#include "PixFitNet.h"
#include "PixFitNetConfiguration.h"
#include "PixFitInstanceConfig.h"

using namespace PixLib;

PixFitNet::PixFitNet(PixFitWorkQueue<RawHisto> *queue,
		PixFitNetConfiguration const *netConfig,
		PixFitInstanceConfig *instanceConfig ) :
		m_scanConfigQueue("ScanConfigQueue"), m_msg() {
	this->m_queue = queue;
	this->m_configuration = netConfig;
	this->m_rodSock = 0;
	this->m_rxBuf = 0;
	this->m_currentBin = 0;
	this->m_threadName = "network";
	this->m_instanceConfig = instanceConfig;
	this->m_resetFlag = 0;
	this->m_scanConfigQueue.setBlackList(std::bind(&BlackList::investigateWorkObject, &m_instanceConfig->blacklist, std::placeholders::_1));
	this->m_histoUnitString = netConfig->getHistogrammer()->makeHistoString();
}

PixFitNet::~PixFitNet() {
	/* TODO Gracefully close open connections */
	free(m_rxBuf);
}

/* @todo: error handling for socket -> exit thread? */
void PixFitNet::loop() {
	std::ostringstream buf; // for buffering lines of output
	ERS_LOG(m_histoUnitString << ": Opening socket.")

	sockaddr_in my_addr;
	sockaddr_in their_addr; // connector's address information
	socklen_t sin_size;
	int rc;
	int localSock = -1;

	/* Create socket. */
	localSock = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == localSock) {
		ERS_LOG(m_histoUnitString << ": Opening socket failed.")
		return;
	}

	/* Allow port number to be reused. */
	int yes = 1;
	if (setsockopt(localSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		ERS_LOG(m_histoUnitString << ": Setting socket option failed.")
		close(localSock);
		return;
	}

	/* Configure local part. */
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = m_configuration->getLocalPort();
	my_addr.sin_addr.s_addr = m_configuration->getLocalIpAddress().s_addr;
	memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));

	rc = bind(localSock, (struct sockaddr *) &my_addr, sizeof(my_addr));
	if (0 != rc) {
		ERS_LOG(m_histoUnitString << ": Bind failed.")
		close(localSock);
		return;
	}

	rc = listen(localSock, 10); // @todo backlog
	if (0 != rc) {
		ERS_LOG(m_histoUnitString << ": Listen failed.")
		close(localSock);
		return;
	}

	/* Get buffer for histogram data. */
	if (0 == (m_rxBuf = (unsigned char*) malloc(s_bufSize))) {
		ERS_LOG(m_histoUnitString << ": Buffer allocation failed.")
		close(localSock);
		return;
	}

	/* Main loop.
	 * @todo Communication to/from PixFitManager.
	 * @todo Signal readiness (to PixFitManager). */
	while (1) {
		ERS_DEBUG(1, m_histoUnitString << ": Begin of loop, localsock " << localSock)

		sin_size = sizeof their_addr;
		/* Blocking here. */
		m_rodSock = accept(localSock, (struct sockaddr *) &their_addr, &sin_size);
		if (-1 == m_rodSock) {
			ERS_LOG(m_histoUnitString << ": Accept failed.")
			close(localSock);
			break;
		}
		ERS_LOG(m_histoUnitString << ": FitServer connection successful!")

		/* Set socket to be non-blocking. */
		fcntl(m_rodSock, F_SETFL, O_NONBLOCK);

		/* Discard reset requests from before a connection has actually been established. */
		m_resetMutex.lock();
		m_resetFlag = 0;
		m_resetMutex.unlock();

		/* Read and process commands. */
		while (1) {
			/* Get scan config from queue if no scan is ongoing. Blocks if no items in queue. */
			if (m_scanConfig == 0) {
				m_dumpFile.close(); /* Close potentially open stream, not throwing by default. */
				m_scanConfig = m_scanConfigQueue.getWork();

				/* Initialize RawHisto object. */
				m_rawHisto = std::make_shared<RawHisto>(m_scanConfig);
				if (!(m_rawHisto->allocateMemory())) {
					ERS_DEBUG(0, m_histoUnitString <<
							": Allocated " << m_rawHisto->getSize() << " bytes for histogram.")
					m_currentBin = 0;
				}
				else {
					return;
				}

				/* Optionally open file for dumping network data. */
				if (m_instanceConfig->dumpNetwork) {
					std::string crate = std::to_string(m_configuration->getHistogrammer()->crate);
					std::string rod = std::to_string(m_configuration->getHistogrammer()->rod);
					std::string slave = std::to_string(m_configuration->getHistogrammer()->slave);
					std::string histo = std::to_string(m_configuration->getHistogrammer()->histo);
					std::string scanId = std::to_string(m_scanConfig->scanId);
					std::string maskId = std::to_string(m_scanConfig->maskId);
					std::string fileName = "FitServer-" + scanId + "-" + "m" + maskId + "-" + crate + "_"
							+ rod + "_" + slave + "_" + histo + ".dump";
					m_dumpFile.open(fileName.c_str(), ios::out | ios::binary | ios::ate);
					/* @todo: fail check */
				}
			}

			rc = readCommand();
			if (1 != rc) {
				break;
			}

			/* Process complete commands. */
			rc = procRequest();


			/* Publish RawHisto to queue. */
			if (2 == rc) {
				m_queue->addWork(m_rawHisto);
				m_rawHisto.reset();
				m_scanConfig.reset();
			}
			else if (0 == rc) {
				;
			}
			/* Reset connection in case of error */
			else {
				break;
			}
		}

		int rc;
		rc = shutdown(m_rodSock, SHUT_WR);
		if (rc != 0) {
		    ERS_LOG(m_histoUnitString << ": Error on shutdown socket")
		}
		rc = close(m_rodSock);
		if (rc != 0) {
		    ERS_LOG(m_histoUnitString << ": Error on close socket")
		}
		m_rodSock = 0;
		sleep(1);
		m_scanConfig = 0;
	}

	/** @todo Only reached when accept() fails. Free memory (when thread is terminated?) */
	free(m_rxBuf);
	ERS_LOG(m_histoUnitString << ": Done!")
	return;
}

int PixFitNet::readCommand() {
	unsigned int rxLen = 0;
	int rc = 0;
	int poll_rc;
	pollfd ufds[1];

	/* Fill struct for poll() call. */
	ufds[0].fd = m_rodSock;
	ufds[0].events = POLLIN;

	while (rxLen < sizeof(RodSlvTcpCmd)) {
		poll_rc = poll(ufds, 1, s_timeout);

		/* Timeout. */
		if (poll_rc == 0) {
			/* Check if reset was requested. */
			if (resetCheck()) return -2;
		}
		/* Error on poll. */
		else if (poll_rc == -1) {
			return -2;
		}
		/* Activity on socket. */
		else {
			rc = recv(m_rodSock, m_rxBuf + rxLen, sizeof(RodSlvTcpCmd) - rxLen, 0);
			//printf("Received %d bytes\n",rc);
			if (-1 == rc) {
				ERS_LOG(m_histoUnitString << ": Socket error.")
				return rc;
			}
			else if (0 == rc) {
				ERS_LOG(m_histoUnitString << ": Socket closed from remote.")
				return rc;
			}
			rxLen += rc; // update len
		}
	}

	/* Optionally dump data to file. */
	if (m_dumpFile.is_open()) {
		m_dumpFile.write(reinterpret_cast<char *>(m_rxBuf), rxLen);
	}
	return 1;
}

void PixFitNet::putScanConfig(std::shared_ptr<const PixFitScanConfig> scanConfig) {
	m_scanConfigQueue.addWork(scanConfig);
}

int PixFitNet::procRequest() {
	// Initialising the Error Message Service
	getMrs(m_rawHisto->getScanConfig());

	RodSlvTcpCmd cmdBuf; // temp buffer is data size too small
	RodSlvTcpCmd cmd;
	unsigned int i, j;
	unsigned int rxLen = 0;
	int rc;
	void *buf = (void *) m_rxBuf;

	int poll_rc;
	pollfd ufds[1];

	/* Fill struct for poll() call. */
	ufds[0].fd = m_rodSock;
	ufds[0].events = POLLIN;

	const int numberBins = m_rawHisto->getScanConfig()->getNumOfBins();
	const int numberPixels = m_rawHisto->getScanConfig()->getNumOfPixels();
	const int multiplicity = m_rawHisto->getScanConfig()->getBytesPerPixel();

	memcpy((void*) &cmdBuf, buf, sizeof(RodSlvTcpCmd));

	/* We have a command. */
	if (SLVNET_MAGIC != ntohl(cmdBuf.magic)) {
		ERS_LOG(m_histoUnitString << ": Invalid magic word.")
		printf("Invalid magic word in command: 0x%x\r\n", ntohl(cmdBuf.magic));
		for (i = 0; i < sizeof(RodSlvTcpCmd); i++) printf("%x", ((char*) &cmdBuf)[i]);
		printf("\r\n");
		return 1;
	}
	else {
		cmdToHost(cmdBuf, cmd); // convert command struct to host order
	}


	/* Process commands. */
	switch (cmd.command) {

	/* SLVNET_HIST_DATA_CMD: Receive pixel data for certain bin. */
	case SLVNET_HIST_DATA_CMD: {
	  ERS_LOG(m_histoUnitString << ": Histogram data for bin = "
			  << cmd.bins << " (" << m_currentBin << ")")

		if (0 != cmd.payloadSize) {
			ERS_DEBUG(0, m_histoUnitString
					<< ": Receiving " << cmd.payloadSize << " bytes of histo data from ROD.")
			while (rxLen < cmd.payloadSize * sizeof(char)) {
				poll_rc = poll(ufds, 1, s_timeout);
					/* Timeout. */
					if (poll_rc == 0) {
						if (resetCheck()) return -2;
					}
					/* Error on poll. */
					else if (poll_rc == -1) {
						return -2;
					}
					/* Activity on socket. */
					else {
						rc = recv(m_rodSock, m_histoBuf + rxLen, cmd.payloadSize * sizeof(char) - rxLen, 0);
						if (-1 == rc) {
							ERS_LOG(m_histoUnitString << ": Socket error.")
							return rc;
						}
						if (0 == rc) {
							ERS_LOG(m_histoUnitString << ": Socket closed from remote.")
							return rc;
						}
						rxLen += rc;
					}
			}

			/* Optionally dump data to file. */
			if (m_dumpFile.is_open()) {
				m_dumpFile.write(m_histoBuf, rxLen);
			}

			ERS_LOG(m_histoUnitString <<
					": Histogram data received for bin " << m_currentBin << ": " <<
					rxLen / sizeof(char) << " bytes stored at 0x" << std::hex << m_histoBuf)

			std::string info = "PixFitNet";
			if ((static_cast<int>(cmd.payloadSize) / multiplicity) != numberPixels) {
			  std::string mess = "Mismatch in number of pixels. Expected " +
					  std::to_string(numberPixels) +
					  " but got " + std::to_string(cmd.payloadSize / multiplicity);
			  m_msg->publishMessage(PixMessages::ERROR, info , mess);
			  return 1;
			}

			if(m_scanConfig->scanId != static_cast<int>(cmd.scanId) && !m_instanceConfig->usingSlaveEmu()){
			  std::string mess = "Mismatch in scanId. Slave says " +
					  std::to_string(cmd.scanId) + ", PixLib says " +
					  std::to_string(m_scanConfig->scanId);
			  m_msg->publishMessage(PixMessages::ERROR, info , mess);
			  return 1;
			}

			/* Get entries and fill RawHisto. */
			RawHisto::histoWord_type *pHisto = m_rawHisto->getRawData();
			uint32_t* histoBufint = (uint32_t*) m_histoBuf;

			/* Memory layout depends on histogrammer readout mode and scan configuration. */
			PixFitScanConfig::scanType scanType = m_scanConfig->findScanType();
			PixFitScanConfig::readoutMode readoutMode =  m_scanConfig->getReadoutMode();

			//ERS_DEBUG(1,m_histoUnitString << ": Scantype " << scanType << " with readout mode " << readoutMode)

			/* -----------------------------------
			 * Analog, digital or threshold scan
			 * ----------------------------------- */
			if (scanType == PixFitScanConfig::scanType::ANALOG
					|| scanType == PixFitScanConfig::scanType::DIGITAL
					|| scanType == PixFitScanConfig::scanType::THRESHOLD) {
			  if (readoutMode == PixFitScanConfig::readoutMode::OFFLINE_OCCUPANCY) {
			    for (j = 0; j < (cmd.payloadSize / multiplicity); j++) {
			      pHisto[numberBins * j + m_currentBin] = (RawHisto::histoWord_type) m_histoBuf[j];
			      //printf("value 0x%x: \t occ %d, \t at %d \n", pHisto[j], (pHisto[j] >> 0*8) & ((1 << 8) - 1), j);
			    }
			  }
			  else if (readoutMode == PixFitScanConfig::readoutMode::ONLINE_OCCUPANCY) {
			    for (j = 0; j < (cmd.payloadSize / multiplicity); j++) {
			      pHisto[numberBins * j + m_currentBin] = (RawHisto::histoWord_type) histoBufint[j];
			      //printf("value 0x%x: \t occ %d \t at %d \n", pHisto[j], pHisto[j], j);
			    }
			  }
			  else if (readoutMode == PixFitScanConfig::readoutMode::SHORT_TOT) {
			    for ( j = 0; j < (cmd.payloadSize / multiplicity); j++) {
			      pHisto[numberBins * j + m_currentBin] = ( histoBufint[j] >> ONEWORD_MISSING_TRIGGERS_RESULT_SHIFT) & ((1 << (MISSING_TRIGGERS_RESULT_BITS)) - 1);
			      //printf("value 0x%x: \t missing triggers %d \t at %d \n",  histoBufint[j], ( histoBufint[j] >> ONEWORD_MISSING_TRIGGERS_RESULT_SHIFT) & ((1 << (MISSING_TRIGGERS_RESULT_BITS)) - 1), j);
			    }
			  }					   
			  else if (readoutMode == PixFitScanConfig::readoutMode::LONG_TOT) {
			    for ( j = 0; j < (cmd.payloadSize * 2 / multiplicity); j++) {
			      if (j % 2 == 0) { // first partial data word
				pHisto[numberBins * j/2 + m_currentBin] = ( histoBufint[j] >> TWOWORD_OCC_RESULT_SHIFT) & ((1 << OCC_RESULT_BITS) - 1);
				//printf("value 0x%x: \t occ %d \t at %d \n", histoBufint[j] , ( histoBufint[j] >> TWOWORD_OCC_RESULT_SHIFT) & ((1 << OCC_RESULT_BITS) - 1), j);
			      }
			    }
			  }
			}
			/* -----------------------------------
			 * TOT scans
			 * ----------------------------------- */
			else if (scanType == PixFitScanConfig::scanType::TOT || scanType == PixFitScanConfig::scanType::TOT_CALIB) {

				/* Other readout modes do not make sense for TOT based stuff. */
				assert(readoutMode == PixFitScanConfig::readoutMode::SHORT_TOT ||
						readoutMode == PixFitScanConfig::readoutMode::LONG_TOT);

				 /* We have 3 words for TOT: occ/missing occ, TOT, TOT² */
				const int wordsPerPixel = m_rawHisto->getScanConfig()->getWordsPerPixel();

			  if (readoutMode == PixFitScanConfig::readoutMode::SHORT_TOT) {
			    for (j = 0; j < (cmd.payloadSize / multiplicity); j++) {
			      pHisto[numberBins * wordsPerPixel * j + wordsPerPixel * m_currentBin + 0] = ( histoBufint[j] >> ONEWORD_MISSING_TRIGGERS_RESULT_SHIFT)
				& ((1 << (MISSING_TRIGGERS_RESULT_BITS)) - 1); // fill missing triggers
			      pHisto[numberBins * wordsPerPixel * j + wordsPerPixel * m_currentBin + 1] = ( histoBufint[j] >> ONEWORD_TOT_RESULT_SHIFT)
				& ((1 << (TOT_RESULT_BITS)) - 1); // then fill ToT
			      pHisto[numberBins * wordsPerPixel * j + wordsPerPixel * m_currentBin + 2] = ( histoBufint[j] >> ONEWORD_TOTSQR_RESULT_SHIFT)
				& ((1 << (TOTSQR_RESULT_BITS)) - 1); // then fill ToT2

			      /*printf("value 0x%x: \t missing triggers %d, \t tot %d, \t tot² %d \t at %d \n",  histoBufint[j], 
				( histoBufint[j] >> ONEWORD_MISSING_TRIGGERS_RESULT_SHIFT) & ((1 << (MISSING_TRIGGERS_RESULT_BITS)) - 1),
				( histoBufint[j] >> ONEWORD_TOT_RESULT_SHIFT) & ((1 << (TOT_RESULT_BITS)) - 1),
				( histoBufint[j] >> ONEWORD_TOTSQR_RESULT_SHIFT) & ((1 << (TOTSQR_RESULT_BITS)) - 1), j);*/
			    }
			  }
			  else if (readoutMode == PixFitScanConfig::readoutMode::LONG_TOT) {
			    for (j = 0; j < (cmd.payloadSize * 2 / multiplicity); j++) {
			      if (j % 2 == 0) { // first partial data word
				pHisto[numberBins * wordsPerPixel * j/2 + wordsPerPixel * m_currentBin + 0] = ( histoBufint[j] >> TWOWORD_OCC_RESULT_SHIFT)
				  & ((1 << OCC_RESULT_BITS) - 1); // fill occ
				//printf("value 0x%x: \t occ %d \t at %d \n", histoBufint[j] , ( histoBufint[j] >> TWOWORD_OCC_RESULT_SHIFT) & ((1 << OCC_RESULT_BITS) - 1), j);
			      }
			      if(j % 2 == 1) { // second partial word
				pHisto[numberBins * wordsPerPixel * (j-1)/2 + wordsPerPixel * m_currentBin + 1] = ( histoBufint[j] >> TWOWORD_TOT_RESULT_SHIFT)
				  & ((1 << (TOT_RESULT_BITS)) - 1); // then fill ToT
				pHisto[numberBins * wordsPerPixel * (j-1)/2 + wordsPerPixel * m_currentBin + 2] = ( histoBufint[j] >> TWOWORD_TOTSQR_RESULT_SHIFT)
				  & ((1 << (TOTSQR_RESULT_BITS)) - 1); // then fill ToT2
				/*printf("value 0x%x: \t tot %d, \t tot² %d \t at %d \n",  histoBufint[j], 
				  ( histoBufint[j] >> TWOWORD_TOT_RESULT_SHIFT) & ((1 << (TOT_RESULT_BITS)) - 1),
				  ( histoBufint[j] >> TWOWORD_TOTSQR_RESULT_SHIFT) & ((1 << (TOTSQR_RESULT_BITS)) - 1), j);*/
			      }
			    }
			  }
			}

			/* Handle intermediate histograms. */
			if (m_rawHisto->getScanConfig()->doIntermediateHistos()) {
				/* Duplicate PixFitScanConfig. */
				std::shared_ptr<const PixFitScanConfig> newScanConfig(new PixFitScanConfig(*m_scanConfig));

				/* Get access to PixFitScanConfig and manipulate the settings for the intermediate histo. */
				std::shared_ptr<PixFitScanConfig> \
					ncScancfg(std::const_pointer_cast<PixFitScanConfig>(newScanConfig));
				ncScancfg->binNumber = m_currentBin;

				auto scanType = newScanConfig->findScanType();
				/* THRESHOLD scans */
				if (scanType == PixFitScanConfig::scanType::THRESHOLD) {
					ncScancfg->intermediate = PixFitScanConfig::intermediateType::INTERMEDIATE_ANALOG;
				}
				/* TOT_CALIB scans */
				else if (scanType == PixFitScanConfig::scanType::TOT_CALIB) {
					ncScancfg->intermediate = PixFitScanConfig::intermediateType::INTERMEDIATE_TOT;
				}

				/* Create new RawHisto for intermediate histo. */
				std::shared_ptr<RawHisto> tmpRawHisto = std::make_shared<RawHisto>(ncScancfg);

				/* Allocate memory and get raw pointer. */
				tmpRawHisto->allocateMemory();
				RawHisto::histoWord_type *pTmpHisto = tmpRawHisto->getRawData();

				/* Fill RawHisto with relevant data. */
				int numOfPixels = ncScancfg->getNumOfPixels();
				if (scanType == PixFitScanConfig::scanType::THRESHOLD) {
					for (int k = 0; k < numOfPixels; k++) {
						pTmpHisto[k] = pHisto[numberBins * k + m_currentBin];
					}
				}
				else if (scanType == PixFitScanConfig::scanType::TOT_CALIB) {
					const int wordsPerPixel = m_rawHisto->getScanConfig()->getWordsPerPixel();
					for (int k = 0; k < numOfPixels; k++) {
						int target = k * wordsPerPixel;
						int source = numberBins * wordsPerPixel * k + wordsPerPixel * m_currentBin;
						pTmpHisto[target + 0] = pHisto[source + 0];
						pTmpHisto[target + 1] = pHisto[source + 1];
						pTmpHisto[target + 2] = pHisto[source + 2];
					}
				}

				ERS_DEBUG(0, tmpRawHisto->getScanConfig()->histogrammer.makeHistoString()
					<< ": Created intermediate histogram for bin " << m_currentBin)

				/* Enqueue intermediate histo object. */
				m_queue->addWork(tmpRawHisto);
			}
			/* End of intermediate histo section. */
		}

		/* Notice the different numbering schemes (1 to NumOfBins here, from 0 to maxBins above). */
	  	std::string currentHistoUnit = m_rawHisto->getScanConfig()->histogrammer.makeHistoString();
		if (m_currentBin + 1 != m_rawHisto->getScanConfig()->getNumOfBins()) {
		  ERS_LOG(currentHistoUnit << " is at bin " << m_currentBin + 1 << " of "
				  << m_rawHisto->getScanConfig()->getNumOfBins())
		  m_currentBin++;
		  return 0;
		}

		ERS_LOG(currentHistoUnit << ": Histogram termination")

                /* Optionally close dump file. */
                if (m_dumpFile.is_open()) {
                    m_dumpFile.write(m_histoBuf, rxLen);
		}

		return 2;
		break;
	}

	/* No other commands supported. */
	default:
	        ERS_LOG(m_histoUnitString << ": Invalid command: " << std::hex << cmd.command)
		return 0;
		break;
	}
}

const PixFitNetConfiguration* PixFitNet::getConfig() const {
	return m_configuration;
}

void PixFitNet::cmdToHost(const RodSlvTcpCmd &cmd, RodSlvTcpCmd &hostCmd) {
	hostCmd.magic = ntohl(cmd.magic);
	hostCmd.command = ntohl(cmd.command);
	hostCmd.bins = ntohl(cmd.bins);
	hostCmd.payloadSize = ntohl(cmd.payloadSize);
	hostCmd.scanId = ntohl(cmd.scanId);
}

bool PixLib::PixFitNet::resetCheck() {
	boost::lock_guard<boost::mutex> lock(m_resetMutex);

	/* Always abort for m_resetFlag == 1 or check if scanId is blacklisted for m_resetFlag == 2. */
	if ((1 == m_resetFlag) || (2 == m_resetFlag && m_instanceConfig->blacklist.isScanIdListed(m_scanConfig->getScanId())) ) {
		ERS_LOG(m_histoUnitString << ": Resetting network thread " << getThreadId())

		cleanup();
		return true;
	}
	/* Flag not set, do not abort. */
	return false;
}

/* This stuff should be obsolete, move completely to ERS once we know how to publish a stream to IGui. */
PixMessages& PixFitNet::getMrs(std::shared_ptr<const PixFitScanConfig> scanConfig)
{
  delete m_msg; // avoid leak
  m_msg = new PixMessages();
  IPCPartition partition(scanConfig->partitionName);
  m_msg->pushPartition(scanConfig->partitionName, &partition);
  m_msg->pushStream("cout", std::cout);
  return *m_msg;
}

void PixFitNet::resetNetwork(bool ifBlacklisted) {
	boost::lock_guard<boost::mutex> lock(m_resetMutex);
	if (!ifBlacklisted) {
		m_resetFlag = 1;
	}
	else {
		m_resetFlag = 2;
	}
	ERS_DEBUG(0, m_histoUnitString << ": Setting network reset flag to " << m_resetFlag)
}

void PixFitNet::cleanup() {
	shutdown(m_rodSock, SHUT_WR);
	close(m_rodSock);

	m_rawHisto.reset();
	m_scanConfig.reset();
	m_resetFlag = 0;
}
