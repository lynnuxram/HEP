/*
 * PixFitFitter_lmfit.h
 *
 *  Created on: Dec 3, 2013
 *      Author: mkretz, marx
 */

#ifndef PIXFITFITTER_LMFIT_H_
#define PIXFITFITTER_LMFIT_H_

#include <string>
#include <memory>

#include <boost/thread/mutex.hpp>
#include <oh/OHRootProvider.h>

#include <TF1.h>
#include <TH1.h>

#include "lmmin.h"

#include "PixFitAbstractFitter.h"

namespace PixLib {

class PixFitResult;
class RawHisto;

class PixFitFitter_lmfit : public PixFitAbstractFitter {
public:
	PixFitFitter_lmfit();
	virtual ~PixFitFitter_lmfit();

	typedef struct {
            const double *t;
            const double *y;
            double (*f) (double t, const double *par, const PixFitFitter_lmfit *fitterInstance);
            const PixFitFitter_lmfit *fitterInstance;
        } pixfit_lmcurve_data_struct;

struct ValidBins {
		int start;
		int end;
		int valid;
		int startsigma;
		int endsigma;
	};

	virtual std::shared_ptr<PixFitResult> fit(std::shared_ptr<RawHisto> histo);

	void analyzeData(std::shared_ptr<RawHisto> histo, ValidBins* validBins, int pixelNumber);

	std::shared_ptr<PixFitResult> doFit(int nthreads, std::shared_ptr<RawHisto> histo);

	// LM fit
	double simpleerf(double x, const double *par) const;
	static double simpleerfWrapper(double x, const double *par, const PixFitFitter_lmfit *fitterInstance);
	static double matchLUT(double x);
	std::shared_ptr<PixFitResult> lmfit(int nthread, std::shared_ptr<RawHisto> histo);


private:
	int vcal_bins;
        static void lmcurve( int n_par, double *par, int m_dat, 
              const double *t, const double *y,
              double (*f)( double t, const double *par, const PixFitFitter_lmfit *fitterInstance ),
              const lm_control_struct *control,
              lm_status_struct *status, const PixFitFitter_lmfit *fitterInstance );
 	static void lmcurve_evaluate( const double *par, int m_dat, const void *data,
                       double *fvec, int *info );
        int inj_iterations;

	constexpr static const double cSqrt2 = 1.41421356237309504880f;
	constexpr static const double cInvSqrt6 = 0.40824829046f;

	/** Controls the verbosity of the fit output. */
	static const bool s_doverbose = false;
};
} /* end of namespace PixLib */

#endif /* PIXFITFITTER_LMFIT_H_ */
