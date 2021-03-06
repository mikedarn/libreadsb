#include <math.h>
#include <stdio.h>
#include "cpr.h"

// Always positive MOD operation, used for CPR decoding.
static int cpr_mod_int(int a, int b)
{
    int res = a % b;
    if (res < 0)
        res += b;
    return res;
}

static double cpr_mod_double(double a, double b)
{
    double res = fmod(a, b);
    if (res < 0)
        res += b;
    return res;
}

// The NL function uses the precomputed table from 1090-WP-9-14
static int cpr_nl(double lat)
{
    if (lat < 0)
        lat = -lat; // Table is simmetric about the equator
    if (lat < 10.47047130)
        return 59;
    if (lat < 14.82817437)
        return 58;
    if (lat < 18.18626357)
        return 57;
    if (lat < 21.02939493)
        return 56;
    if (lat < 23.54504487)
        return 55;
    if (lat < 25.82924707)
        return 54;
    if (lat < 27.93898710)
        return 53;
    if (lat < 29.91135686)
        return 52;
    if (lat < 31.77209708)
        return 51;
    if (lat < 33.53993436)
        return 50;
    if (lat < 35.22899598)
        return 49;
    if (lat < 36.85025108)
        return 48;
    if (lat < 38.41241892)
        return 47;
    if (lat < 39.92256684)
        return 46;
    if (lat < 41.38651832)
        return 45;
    if (lat < 42.80914012)
        return 44;
    if (lat < 44.19454951)
        return 43;
    if (lat < 45.54626723)
        return 42;
    if (lat < 46.86733252)
        return 41;
    if (lat < 48.16039128)
        return 40;
    if (lat < 49.42776439)
        return 39;
    if (lat < 50.67150166)
        return 38;
    if (lat < 51.89342469)
        return 37;
    if (lat < 53.09516153)
        return 36;
    if (lat < 54.27817472)
        return 35;
    if (lat < 55.44378444)
        return 34;
    if (lat < 56.59318756)
        return 33;
    if (lat < 57.72747354)
        return 32;
    if (lat < 58.84763776)
        return 31;
    if (lat < 59.95459277)
        return 30;
    if (lat < 61.04917774)
        return 29;
    if (lat < 62.13216659)
        return 28;
    if (lat < 63.20427479)
        return 27;
    if (lat < 64.26616523)
        return 26;
    if (lat < 65.31845310)
        return 25;
    if (lat < 66.36171008)
        return 24;
    if (lat < 67.39646774)
        return 23;
    if (lat < 68.42322022)
        return 22;
    if (lat < 69.44242631)
        return 21;
    if (lat < 70.45451075)
        return 20;
    if (lat < 71.45986473)
        return 19;
    if (lat < 72.45884545)
        return 18;
    if (lat < 73.45177442)
        return 17;
    if (lat < 74.43893416)
        return 16;
    if (lat < 75.42056257)
        return 15;
    if (lat < 76.39684391)
        return 14;
    if (lat < 77.36789461)
        return 13;
    if (lat < 78.33374083)
        return 12;
    if (lat < 79.29428225)
        return 11;
    if (lat < 80.24923213)
        return 10;
    if (lat < 81.19801349)
        return 9;
    if (lat < 82.13956981)
        return 8;
    if (lat < 83.07199445)
        return 7;
    if (lat < 83.99173563)
        return 6;
    if (lat < 84.89166191)
        return 5;
    if (lat < 85.75541621)
        return 4;
    if (lat < 86.53536998)
        return 3;
    if (lat < 87.00000000)
        return 2;
    else
        return 1;
}

static int cpr_n(double lat, int fflag)
{
    int nl = cpr_nl(lat) - (fflag ? 1 : 0);
    if (nl < 1)
        nl = 1;
    return nl;
}

static double cpr_dlon(double lat, int fflag, int surface)
{
    return (surface ? 90.0 : 360.0) / cpr_n(lat, fflag);
}

/* This algorithm comes from:
 * http://www.lll.lu/~edward/edward/adsb/DecodingADSBposition.html.
 *
 * 131072 is 2^17 since CPR latitude and longitude are encoded in 17 bits.
 */
int decode_cpr_airborne(int even_cprlat, int even_cprlon,
                        int odd_cprlat, int odd_cprlon,
                        int fflag,
                        double *out_lat, double *out_lon)
{
    double AirDlat0 = 360.0 / 60.0;
    double AirDlat1 = 360.0 / 59.0;
    double lat0 = even_cprlat;
    double lat1 = odd_cprlat;
    double lon0 = even_cprlon;
    double lon1 = odd_cprlon;

    double rlat, rlon;

    // Compute the Latitude Index "j"
    int j = (int)floor(((59 * lat0 - 60 * lat1) / 131072) + 0.5);
    double rlat0 = AirDlat0 * (cpr_mod_int(j, 60) + lat0 / 131072);
    double rlat1 = AirDlat1 * (cpr_mod_int(j, 59) + lat1 / 131072);

    if (rlat0 >= 270)
        rlat0 -= 360;
    if (rlat1 >= 270)
        rlat1 -= 360;

    // Check to see that the latitude is in range: -90 .. +90
    if (rlat0 < -90 || rlat0 > 90 || rlat1 < -90 || rlat1 > 90)
        return (-2); // bad data

    // Check that both are in the same latitude zone, or abort.
    if (cpr_nl(rlat0) != cpr_nl(rlat1))
        return (-1); // positions crossed a latitude zone, try again later

    // Compute ni and the Longitude Index "m"
    if (fflag)
    { // Use odd packet.
        int ni = cpr_n(rlat1, 1);
        int m = (int)floor((((lon0 * (cpr_nl(rlat1) - 1)) -
                             (lon1 * cpr_nl(rlat1))) /
                            131072.0) +
                           0.5);
        rlon = cpr_dlon(rlat1, 1, 0) * (cpr_mod_int(m, ni) + lon1 / 131072);
        rlat = rlat1;
    }
    else
    { // Use even packet.
        int ni = cpr_n(rlat0, 0);
        int m = (int)floor((((lon0 * (cpr_nl(rlat0) - 1)) -
                             (lon1 * cpr_nl(rlat0))) /
                            131072) +
                           0.5);
        rlon = cpr_dlon(rlat0, 0, 0) * (cpr_mod_int(m, ni) + lon0 / 131072);
        rlat = rlat0;
    }

    // Renormalize to -180 .. +180
    rlon -= floor((rlon + 180) / 360) * 360;

    *out_lat = rlat;
    *out_lon = rlon;

    return 0;
}

int decode_cpr_surface(double reflat, double reflon,
                       int even_cprlat, int even_cprlon,
                       int odd_cprlat, int odd_cprlon,
                       int fflag,
                       double *out_lat, double *out_lon)
{
    double AirDlat0 = 90.0 / 60.0;
    double AirDlat1 = 90.0 / 59.0;
    double lat0 = even_cprlat;
    double lat1 = odd_cprlat;
    double lon0 = even_cprlon;
    double lon1 = odd_cprlon;
    double rlon, rlat;

    // Compute the Latitude Index "j"
    int j = (int)floor(((59 * lat0 - 60 * lat1) / 131072) + 0.5);
    double rlat0 = AirDlat0 * (cpr_mod_int(j, 60) + lat0 / 131072);
    double rlat1 = AirDlat1 * (cpr_mod_int(j, 59) + lat1 / 131072);

    // Pick the quadrant that's closest to the reference location -
    // this is not necessarily the same quadrant that contains the
    // reference location.
    //
    // There are also only two valid quadrants: -90..0 and 0..90;
    // no correct message would try to encoding a latitude in the
    // ranges -180..-90 and 90..180.
    //
    // If the computed latitude is more than 45 degrees north of
    // the reference latitude (using the northern hemisphere
    // solution), then the southern hemisphere solution will be
    // closer to the refernce latitude.
    //
    // e.g. reflat=0, rlat=44, use rlat=44
    //      reflat=0, rlat=46, use rlat=46-90 = -44
    //      reflat=40, rlat=84, use rlat=84
    //      reflat=40, rlat=86, use rlat=86-90 = -4
    //      reflat=-40, rlat=4, use rlat=4
    //      reflat=-40, rlat=6, use rlat=6-90 = -84

    // As a special case, -90, 0 and +90 all encode to zero, so
    // there's a little extra work to do there.

    if (rlat0 == 0)
    {
        if (reflat < -45)
            rlat0 = -90;
        else if (reflat > 45)
            rlat0 = 90;
    }
    else if ((rlat0 - reflat) > 45)
    {
        rlat0 -= 90;
    }

    if (rlat1 == 0)
    {
        if (reflat < -45)
            rlat1 = -90;
        else if (reflat > 45)
            rlat1 = 90;
    }
    else if ((rlat1 - reflat) > 45)
    {
        rlat1 -= 90;
    }

    // Check to see that the latitude is in range: -90 .. +90
    if (rlat0 < -90 || rlat0 > 90 || rlat1 < -90 || rlat1 > 90)
        return (-2); // bad data

    // Check that both are in the same latitude zone, or abort.
    if (cpr_nl(rlat0) != cpr_nl(rlat1))
        return (-1); // positions crossed a latitude zone, try again later

    // Compute ni and the Longitude Index "m"
    if (fflag)
    { // Use odd packet.
        int ni = cpr_n(rlat1, 1);
        int m = (int)floor((((lon0 * (cpr_nl(rlat1) - 1)) -
                             (lon1 * cpr_nl(rlat1))) /
                            131072.0) +
                           0.5);
        rlon = cpr_dlon(rlat1, 1, 1) * (cpr_mod_int(m, ni) + lon1 / 131072);
        rlat = rlat1;
    }
    else
    { // Use even packet.
        int ni = cpr_n(rlat0, 0);
        int m = (int)floor((((lon0 * (cpr_nl(rlat0) - 1)) -
                             (lon1 * cpr_nl(rlat0))) /
                            131072) +
                           0.5);
        rlon = cpr_dlon(rlat0, 0, 1) * (cpr_mod_int(m, ni) + lon0 / 131072);
        rlat = rlat0;
    }

    // Pick the quadrant that's closest to the reference location -
    // this is not necessarily the same quadrant that contains the
    // reference location. Unlike the latitude case, all four
    // quadrants are valid.

    // if reflon is more than 45 degrees away, move some multiple of 90 degrees towards it
    rlon += floor((reflon - rlon + 45) / 90) * 90; // this might move us outside (-180..+180), we fix this below

    // Renormalize to -180 .. +180
    rlon -= floor((rlon + 180) / 360) * 360;

    *out_lat = rlat;
    *out_lon = rlon;
    return 0;
}

/* This algorithm comes from:
 * 1090-WP29-07-Draft_CPR101 (which also defines decodeCPR() )
 *
 * Despite what the earlier comment here said, we should *not* be using trunc().
 * See Figure 5-5 / 5-6 and note that floor is applied to (0.5 + fRP - fEP), not
 * directly to (fRP - fEP). Eq 38 is correct.
 */
int decode_cpr_relative(double reflat, double reflon,
                        int cprlat, int cprlon,
                        int fflag, int surface,
                        double *out_lat, double *out_lon)
{
    double AirDlat;
    double AirDlon;
    double fractional_lat = cprlat / 131072.0;
    double fractional_lon = cprlon / 131072.0;
    double rlon, rlat;
    int j, m;

    AirDlat = (surface ? 90.0 : 360.0) / (fflag ? 59.0 : 60.0);

    // Compute the Latitude Index "j"
    j = (int)(floor(reflat / AirDlat) +
              floor(0.5 + cpr_mod_double(reflat, AirDlat) / AirDlat - fractional_lat));
    rlat = AirDlat * (j + fractional_lat);
    if (rlat >= 270)
        rlat -= 360;

    // Check to see that the latitude is in range: -90 .. +90
    if (rlat < -90 || rlat > 90)
    {
        return (-1); // Time to give up - Latitude error
    }

    // Check to see that answer is reasonable - ie no more than 1/2 cell away
    if (fabs(rlat - reflat) > (AirDlat / 2))
    {
        return (-1); // Time to give up - Latitude error
    }

    // Compute the Longitude Index "m"
    AirDlon = cpr_dlon(rlat, fflag, surface);
    m = (int)(floor(reflon / AirDlon) +
              floor(0.5 + cpr_mod_double(reflon, AirDlon) / AirDlon - fractional_lon));
    rlon = AirDlon * (m + fractional_lon);
    if (rlon > 180)
        rlon -= 360;

    // Check to see that answer is reasonable - ie no more than 1/2 cell away
    if (fabs(rlon - reflon) > (AirDlon / 2))
        return (-1); // Time to give up - Longitude error

    *out_lat = rlat;
    *out_lon = rlon;
    return (0);
}
