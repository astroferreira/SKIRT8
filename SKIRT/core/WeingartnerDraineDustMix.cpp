/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#include "WeingartnerDraineDustMix.hpp"
#include "DraineGraphiteGrainComposition.hpp"
#include "DraineIonizedPAHGrainComposition.hpp"
#include "DraineNeutralPAHGrainComposition.hpp"
#include "DraineSilicateGrainComposition.hpp"

//////////////////////////////////////////////////////////////////////

namespace
{
    // grain size ranges for each of the dust composition types (in m)
    const double amin_gra = 0.001e-6;
    const double amax_gra = 10.0e-6;
    const double amin_sil = 0.001e-6;
    const double amax_sil = 10.0e-6;
    const double amin_pah = 0.0003548e-6;
    const double amax_pah = 0.01e-6;

    // parameterized grain size distribution for graphite and silicate
    double dnda_grasil(double a, double C, double at, double ac, double alpha, double beta)
    {
        double f0 = C/a * pow(a/at,alpha);
        double f1 = 0.0;
        if (beta>0)
            f1 = 1.0+beta*a/at;
        else
            f1 = 1.0/(1.0-beta*a/at);
        double f2 = 0.0;
        if (a<at)
            f2 = 1.0;
        else
            f2 = exp(-pow((a-at)/ac,3));
        return f0*f1*f2;
    }

    // parameterized grain size distribution for pah (whether neutral or ionized)
    double dnda_pah(double a, double sigma, const double a0[2], const double bc[2])
    {
        const double mC = 1.9944e-26;    // mass of C atom in kg
        const double rho = 2.24e3;       // mass density of graphite in kg/m^3
        const double amin = 3.5e-10;     // 3.5 Angstrom in m
        double B[2];
        for (int i=0; i<2; i++)
        {
            double t0 = 3.0/pow(2*M_PI,1.5);
            double t1 = exp(-4.5*sigma*sigma);
            double t2 = 1.0/rho/pow(a0[i],3)/sigma;
            double erffac = 3.0*sigma/sqrt(2.0) + log(a0[i]/amin)/sqrt(2.0)/sigma;
            double t3 = bc[i]*mC/(1.0+erf(erffac));
            B[i] = t0*t1*t2*t3;
        }
        double sum = 0.0;
        for (int i=0; i<2; i++)
        {
            double u = log(a/a0[i])/sigma;
            sum += B[i]/a*exp(-0.5*u*u);
        }
        return sum;
    }

    // grain size distributions for Milky Way environment with R_V = 3.1
    //   -> Table 1 p300 in Weingartner & Draine 2001, ApJ, 548, 296
    //   -> Table 3 p787 in Li & Draine 2001, ApJ, 554, 778
    double dnda_gra_mwy(double a)
    {
        const double C = 9.99e-12;
        const double at = 0.0107e-6;
        const double ac = 0.428e-6;
        const double alpha = -1.54;
        const double beta = -0.165;
        return dnda_grasil(a, C, at, ac, alpha, beta);
    }

    double dnda_sil_mwy(double a)
    {
        const double C = 1.00e-13;
        const double at = 0.164e-6;
        const double ac = 0.1e-6;
        const double alpha = -2.21;
        const double beta = 0.300;
        return dnda_grasil(a, C, at, ac, alpha, beta);
    }

    double dnda_pah_mwy(double a)
    {
        const double sigma = 0.4;
        const double a0[2] = {3.5e-10, 30e-10};
        const double bc[2] = {4.5e-5, 1.5e-5};
        return 0.5 * dnda_pah(a, sigma, a0, bc); // 50% of the PAH grains are neutral, 50% are ionized
    }

    // grain size distributions for LMC environment
    //   -> Line 2 of Table 3 p305 in Weingartner & Draine 2001, ApJ, 548, 296
    //   -> For PAHs, use Milky Way values with 1/6 of total abundance
    //      Line 2 of Table 3: b_C = 1.0   <--> Table 1 for R_V = 3.1: b_C = 6
    double dnda_gra_lmc(double a)
    {
        const double C = 3.51e-15;
        const double at = 0.0980e-6;
        const double ac = 0.641e-6;
        const double alpha = -2.99;
        const double beta = 2.46;
        return dnda_grasil(a, C, at, ac, alpha, beta);
    }

    double dnda_sil_lmc(double a)       // Weingartner & Draine 2001, ApJ, 548, 296 -- Table 3 p305
    {
        const double C = 1.78e-14;
        const double at = 0.184e-6;
        const double ac = 0.1e-6;
        const double alpha = -2.49;
        const double beta = 0.345;
        return dnda_grasil(a, C, at, ac, alpha, beta);
    }

    double dnda_pah_lmc(double a)       // Weingartner & Draine 2001, ApJ, 548, 296 -- Table 3 p305
    {
        const double sigma = 0.4;                // Milky Way value
        const double a0[2] = {3.5e-10, 30e-10};  // Milky Way values
        const double bc[2] = {0.75e-5, 0.25e-5}; // 1/6 of Milky Way values
        return 0.5 * dnda_pah(a, sigma, a0, bc); // 50% of the PAH grains are neutral, 50% are ionized
    }
}

//////////////////////////////////////////////////////////////////////

void WeingartnerDraineDustMix::setupSelfBefore()
{
    MultiGrainDustMix::setupSelfBefore();

    addPopulations(new DraineGraphiteGrainComposition(this), amin_gra, amax_gra,
                   _environment==Environment::MilkyWay ? &dnda_gra_mwy : &dnda_gra_lmc, _numGraphiteSizes);
    addPopulations(new DraineSilicateGrainComposition(this), amin_sil, amax_sil,
                   _environment==Environment::MilkyWay ? &dnda_sil_mwy : &dnda_sil_lmc, _numSilicateSizes);
    addPopulations(new DraineNeutralPAHGrainComposition(this), amin_pah, amax_pah,
                   _environment==Environment::MilkyWay ? &dnda_pah_mwy : &dnda_pah_lmc, _numPAHSizes);
    addPopulations(new DraineIonizedPAHGrainComposition(this), amin_pah, amax_pah,
                   _environment==Environment::MilkyWay ? &dnda_pah_mwy : &dnda_pah_lmc, _numPAHSizes);
}

////////////////////////////////////////////////////////////////////
