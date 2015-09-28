/*
 * PixFitFitter_lmfit.cxx
 *
 *  Created on: Dec 3, 2013
 *      Author: mkretz, marx
 */

#include <iostream>
#include <memory>

#include <oh/OHRootProvider.h>
#include <ers/ers.h>

#include <TF1.h>
#include <TH1.h>
#include <TH2.h>
#include <TApplication.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TRandom.h>

#include "lmmin.h"

#include "PixFitFitter_lmfit.h"
#include "RawHisto.h"
#include "PixFitManager.h" // for global locks
#include "PixFitResult.h"
#include "PixFitInstanceConfig.h" // for threading settings

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

using namespace PixLib;

int getRow(unsigned int j);

PixFitFitter_lmfit::PixFitFitter_lmfit() {
}

PixFitFitter_lmfit::~PixFitFitter_lmfit() {
}

std::shared_ptr<PixFitResult> PixFitFitter_lmfit::fit(std::shared_ptr<RawHisto> histo) {
	vcal_bins = histo->getScanConfig()->getNumOfBins();
	inj_iterations = histo->getScanConfig()->getInjections();
	return doFit(PixFitInstanceConfig::getThreadingSettings(), histo);
}

// determines range of "good" data for a single pixel
void PixFitFitter_lmfit::analyzeData(std::shared_ptr<RawHisto> histo, ValidBins* validBins, int pixelNumber) {
	int i = 0;
	int start = 0;
	int end = 0;
	int startsigma = 0;
	int endsigma = 0;
	int size = histo->getScanConfig()->getNumOfBins();
	double a0_guess, a1_guess, a2_guess;

	// get first bin > 0
	for (i = 0; i < size; i++) {
		if ((*histo)(pixelNumber, i, 0) != 0.0) {
			if (i > 0)
				start = i - 1; // include one zero datapoint
			else
				start = 0;
			break;
		}
	}

	// guess plateau
	a0_guess = 0.999 * (*histo)(pixelNumber, size - 1, 0);
	for (i = start; i < size; i++) {
	  if ((*histo)(pixelNumber, i, 0) >= a0_guess) {
	    end = i;
	    break;
	  }
	}

	// guess lower and upper boundary for sigma
	a1_guess = 0.16 * (*histo)(pixelNumber, size - 1, 0);
	a2_guess = 0.84 * (*histo)(pixelNumber, size - 1, 0);
	// guess the DSP way - find the range over which the data crosses the 16% and 84% points
	int hi1 = 0; int hi2 = 0;
	int lo1 = 0; int lo2 = 0;
	int j = size - 1;
	for(int k = 0; k < size ;) {
	  if(!lo1 && ((*histo)(pixelNumber, k, 0) >= a1_guess)) lo1 = k;
	  if(!lo2 && ((*histo)(pixelNumber, k, 0) >= a2_guess)) lo2 = k;
	  if(!hi1 && ((*histo)(pixelNumber, size - 1 - k, 0) <= a1_guess)) hi1 = j;
	  if(!hi2 && ((*histo)(pixelNumber, size - 1 - k, 0) <= a2_guess)) hi2 = j;
	  --j;
	  ++k;
	}
	startsigma = (lo1 + hi1) * 0.5;
	endsigma = (lo2 + hi2) * 0.5;
	// guess in a more naive way
	/*for (i = start; i < size; i++) {
	  if ((*histo)(pixelNumber, i, 0) >= a1_guess) {
	    startsigma = i;
	    break;
	  }
	}
	for (i = start; i < size; i++) {
	  if ((*histo)(pixelNumber, i, 0) >= a2_guess) {
	    endsigma = i;
	    break;
	  }
	  }*/

	validBins->start = start;
	validBins->end = end;
	validBins->startsigma = startsigma;
	validBins->endsigma = endsigma;
	if( (end - start > 0) || (end - start == 0 && end != 0) ) validBins->valid = end - start + 1;
	else validBins->valid = 0;
	//std::cout << "There are " << validBins->valid << " valid bins" << std::endl;
	//std::cout << "  start = " << validBins->start << ", end = " << validBins->end << std::endl;
	//std::cout << "  sigma from 16% = " << validBins->startsigma << ", 84% = " << validBins->endsigma << std::endl;
}


std::shared_ptr<PixFitResult> PixFitFitter_lmfit::doFit(int nthreads, std::shared_ptr<RawHisto> histo) {
	return lmfit(nthreads, histo);
}

/////////// LM FIT //////////////
double PixFitFitter_lmfit::simpleerfWrapper(double x, const double *par, const PixFitFitter_lmfit *fitterInstance) {
        return fitterInstance->simpleerf(x, par);
}

double PixFitFitter_lmfit::simpleerf(double x, const double *par) const{
  return 0.5 * inj_iterations * (2 - erfc((x - par[0]) / (par[1] * cSqrt2))); // use analytic function - 2 params (careful with normalisation!)
	//return 0.5 * inj_iterations*( 2 - matchLUT((x-par[0])/(par[1] * cSqrt2)) ); // use LUTs - only works if lmmin.c values get tweaked
	//return lut[((int) x)]*par[1]+par[0]; // use x-y reversed lut - need to change x-y order in lmcurve
}


std::shared_ptr<PixFitResult> PixFitFitter_lmfit::lmfit(int nthread,
		std::shared_ptr<RawHisto> histo) {

	// Init options
	unsigned int threads = nthread;
	const unsigned int npoints = histo->getScanConfig()->getNumOfBins();
	const unsigned int pixels = histo->getScanConfig()->getNumOfPixels();

	ERS_DEBUG(1, "Reading " << npoints << " points of data for " << pixels << " pixels")

	std::unique_ptr<double[]> x(new double[npoints * pixels]);
	std::unique_ptr<double[]> y(new double[npoints * pixels]);

	// for debugging purposes, make a root file with occ histograms (only temporary)
#if 0
	{
	boost::lock_guard<boost::mutex> lock(root_m);

	TString filename = "threshold.root";
	TFile file(filename.Data(), "RECREATE");
	TH1F* histo_scurve = new TH1F("scurve", "scurve", vcal_bins, 0., vcal_bins);
	for(int thebin = 0; thebin<vcal_bins; thebin++){
	  TString k = Form ("%d", thebin);
	  TH2F* histo_occ = new TH2F("occupancy"+k, "occupancy"+k, 80, 0., 80., 336, 0., 336.);
	  for (unsigned int i = 0; i < pixels; i++) {
	    int row = getRow(i);
	    histo_occ->Fill(i - (row * 80), row, (*histo)(i, thebin, 0));
	    if((*histo)(i, thebin, 0) <= inj_iterations) histo_scurve->Fill(thebin,(*histo)(i, thebin, 0));
	  }
	  histo_occ->Write();
	}
	histo_scurve->Write();
	file.Close();
	}
#endif

	// Init fitter
	ERS_LOG("Initializing lmfit fitter with " << threads << " threads.")
	std::unique_ptr<lm_status_struct[]> status(new lm_status_struct[pixels]);
	/* Initialize outcome so that we can tell if lmfit was actually run for this pixel later on. */
	for (unsigned int i = 0; i < pixels; i++) {
		status[i].outcome = -1;
	}

	std::unique_ptr<lm_control_struct[]> control(new lm_control_struct[pixels]);

	const int n_par = 2;
	/* Allocate memory for the results of the fit (n_par variables: mu and sigma) plus chi2 at the end of the array. */
	std::unique_ptr<double[]> par(new double[pixels * n_par + pixels]);

	int overall_counter = 0;
	ValidBins validBins;

	/* Guess initial values. */
	for (unsigned int i = 0; i < pixels; i++) {
		control[i] = lm_control_double;
		control[i].verbosity = 0;
		analyzeData(histo, &validBins, i);
		par[i*n_par+0] = validBins.start + (validBins.valid / 2);
		par[i*n_par+1] = (validBins.endsigma - validBins.startsigma) / cSqrt2; //TODO: this could do with a bit of optimization
		//ERS_DEBUG(1, std::cout << "Pixel " << i << ": initial guess thr = " << par[i*n_par+0] << ", noise = " << par[i*n_par+1])
	}

	// Thread pool

	// Timer
	timeval begin, finish;

	// Counters for fit results
	int exh = 0, trap = 0, zero = 0, convbad = 0, conv = 0;

	// Only keep data that holds bins around s-curve centroid and do fit
	gettimeofday(&begin, 0);

	/* Use boost's io_service to handle the fit work with a threadpool. */
	boost::asio::io_service ioservice(threads);
	boost::thread_group tp;

	for (unsigned int i = 0; i < pixels; i++) {
	  analyzeData(histo, &validBins, i);
	  if (i == 0)
	    overall_counter = validBins.start;
	  if(validBins.valid > 0){
	    for (int point = validBins.start; point < (validBins.end + 1); point++) {
	      x[overall_counter] = point; // VCal steps
	      y[overall_counter] = (*histo)(i, point, 0); //number of injections
	      overall_counter++;
	    }
	  }
	  //cout << "Pixel "<< i << " start " << data->start << " end " << data->end << " bins " << data->validBins << " counter " << overall_counter << endl;
	  
	  /* Schedule fit only when there are more than 2 valid bins. */
	  if (validBins.valid > 2) {
			ioservice.post(boost::bind(lmcurve, n_par, &par[n_par*i], validBins.valid,
				    &x[overall_counter - validBins.valid],
				    &y[overall_counter - validBins.valid], simpleerfWrapper,
				    &control[i], &status[i], this));
	  }
	  /* @todo: test if code is quick enough and can handle 3 bins or if analytical solution should
	   * be added here also if only 2, use analytical solution from DSP code. */
	  else if (validBins.valid == 2) {
	    par[i*n_par+0] = (validBins.start + validBins.end) / 2;
	    par[i*n_par+1] = (validBins.end - validBins.start) * cInvSqrt6;
	  }
	  else {
	    if (validBins.valid == 0) zero++;
	    par[i*n_par+0] = -1;
	    par[i*n_par+1] = -1;
	  }
	}
	for (unsigned int i = 0; i < threads; i++) {
	    tp.create_thread(boost::bind(&boost::asio::io_service::run, &ioservice));
	}

	
	/* Wait to finish tasks. */
	tp.join_all();

	gettimeofday(&finish, 0);
	
	double time = finish.tv_sec - begin.tv_sec + 1e-6 * (finish.tv_usec - begin.tv_usec);
	ERS_LOG("Done fitting! (in " << time << "s with " << time / static_cast<double>(pixels) * 1e6 << "us per pixel)")

	// Fit results
	for (unsigned int i = 0; i < pixels; i++) {
	  if (status[i].outcome != -1) {
	    std::string searchstatus = lm_infmsg[status[i].outcome];
	    if (searchstatus.find("exhausted") != std::string::npos) exh++;
	    if (searchstatus.find("trapped") != std::string::npos) trap++;
	    if (searchstatus.find("zero") != std::string::npos) zero++;
	    // fit clearly failed - possibly improve these cases (e.g. filter out those that still have approximate s-curve behaviour)
	    if (searchstatus.find("converged") != std::string::npos) conv++; 

	    if (!(searchstatus.find("converged") != std::string::npos)) {
				if (s_doverbose	&& !(searchstatus.find("zero") != std::string::npos)) {
					std::cout << "Pixel " << i << ": Fit failed " << searchstatus
									<< " -- mean = " << par[n_par * i + 0]
									<< ", noise = " << par[n_par * i + 1] << std::endl;
					std::cout << "  raw bin values: ";
					for (unsigned int j = 0; j < npoints; j++) {
						std::cout << (*histo)(i, j, 0) << " ";
					}
					std::cout << std::endl;
				}

	      par[n_par*i+0] = -1;
	      par[n_par*i+1] = -1;
	    }
	    // fit converged but outcome is negative and needs to be understood (most likely noisy pixels)
			if ((searchstatus.find("converged") != std::string::npos)
					&& par[n_par * i + 0] < 0) {
				convbad++;
				if (s_doverbose) {
					std::cout << "Pixel " << i << ": Fit converged -- mean = "
							<< par[n_par * i + 0] << ", noise = "
							<< par[n_par * i + 1] << std::endl;
					std::cout << "  raw bin values: ";
					for (unsigned int j = 0; j < npoints; j++) {
						std::cout << (*histo)(i, j, 0) << " ";
					}
					std::cout << std::endl;
				}
				par[n_par * i + 0] = -1;
				par[n_par * i + 1] = -1;
			}
	  }
	}
	ERS_LOG("Fit failure summary: total bad = " << exh + trap + convbad << ", total zero = " << zero)
	ERS_LOG("  (ex " << exh << " / trap " << trap << " / convbad " << convbad << " / converged " << conv << ")")

	/* Fill threshold and noise values for a chip into array to pass to Publisher */
	std::shared_ptr<PixFitResult> result = std::make_shared<PixFitResult>(histo->getScanConfig());

	/* Get chi2 values and write them at the back of the array. In case no fit was run fill with -1. */
	int offset = pixels*n_par;
	for (unsigned int i = 0; i < pixels; i++) {
	  if (status[i].outcome != -1) {
	    par[offset + i] = status[i].fnorm / (npoints - n_par - 1); //chi2 per n.d.f.
	  }
	  else {
	    par[offset + i] = -1;
	  }
	}

	result->thresh_array = std::move(par);

	// In case you want to print the whole shebang
#if 0
	for (unsigned int i = 0; i < pixels; i++) {
	  std::cout << "Pixel: " << i << " -- status: " << lm_infmsg[status[i].outcome] << std::endl;
	  std::cout << "  Mean = " << par[n_par*i+0] << std::endl;
	  std::cout << "  Sigma = " << par[n_par*i+1] << std::endl;
	  std::cout << "  Chi2 = " << status[i].fnorm  / (npoints - n_par - 1) << std::endl;
	}
#endif
	return result;
}

int getRow(unsigned int j){
  unsigned int row = 0;
  for (unsigned int x = 0; x < 336; x++) {
    if ((j >= (80 * x)) && (j < (80 * (x + 1)))) {
      row = x;
    }
  }
  return row;
}


void PixFitFitter_lmfit::lmcurve_evaluate( const double *par, int m_dat, const void *data,
                       double *fvec, int*)
{
    int i;
    for ( i = 0; i < m_dat; i++ )
        fvec[i] =
            ((pixfit_lmcurve_data_struct*)data)->y[i] -
            ((pixfit_lmcurve_data_struct*)data)->f(
                ((pixfit_lmcurve_data_struct*)data)->t[i], par, ((pixfit_lmcurve_data_struct*)data)->fitterInstance );
}


void PixFitFitter_lmfit::lmcurve( int n_par, double *par, int m_dat, 
              const double *t, const double *y,
              double (*f)( double t, const double *par, const PixFitFitter_lmfit *fitterInstance ),
              const lm_control_struct *control,
              lm_status_struct *status, const PixFitFitter_lmfit *fitterInstance )
{
    pixfit_lmcurve_data_struct datavar;
    datavar.t = t;
    datavar.y = y;
    datavar.f = f;
    datavar.fitterInstance = fitterInstance;

    lmmin( n_par, par, m_dat, (const void*) &datavar,
           lmcurve_evaluate, control, status );
}
