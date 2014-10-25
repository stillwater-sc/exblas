
#include "Superaccumulator.hpp"
#include "TRSV.hpp"

/*
 * Naive implementation of TRSV for comparision only; it is much easy to port than the BLAS implementation
 */
// assume a row-wise storage
extern "C" int TRSVUNN(
    const double *a,
    const double *b,
    const int n,
    double *x
) {
    double s;

    for(int i = n-1; i >= 0; i--) {
        s = b[i];
        for(int j = i+1; j < n; j++)
            s = s - a[i * n + j] * x[j];
        x[i] = s / a[i * (n + 1)];
    }

    return 1;
}

extern "C" int TRSVUNN_Kulisch(const double *a, const double *b, const int n, double *x) {
  Superaccumulator superacc(1023, 1023 + 52);

  for(int i = n-1; i >= 0; i--) {
      superacc.Accumulate(b[i]);
      for(int j = i+1; j < n; j++) {
          double e;
          double r = TwoProductFMA(-a[i * n + j], x[j], &e);
          superacc.Accumulate(r);
          if (e != 0.0)
            superacc.Accumulate(e);
          //s = s - a[i * n + j] * x[j];
      }
      x[i] = superacc.Round();
      x[i] = x[i] / a[i * (n + 1)];
      printf("%.4g  ", x[i]);
      superacc.Reset();
  }
  printf("\n");

  return 1;
}

// assume a row-wise storage
extern "C" int TRSVLNU(
    double *x,
    const double *a,
    const int n
) {
    for(int j = 0; j < n - 1; j++)
        for(int i = j + 1; i < n; i++)
            x[i] = x[i] - a[j * n + i] * x[j];

    return 1;
}

extern "C" bool compare(
    const double *trsv_cpu,
    const double *trsv_gpu,
    const uint n,
    const double epsilon
) {
    double norm = 0.0;

    for(uint i = 0; i < n; i++)
        norm += pow(fabs(trsv_cpu[i] - trsv_gpu[i]), 2);
    norm = ::sqrt(norm);
    printf("    Norm = %.15g\n", norm);

    return norm < epsilon ? true : false;
}

extern "C" bool compareTRSVUNNToMPFR(
    const double *a,
    double *b,
    const double *trsv,
    const int n,
    const double epsilon
) {
    mpfr_t sum, dot, div, op1;

    mpfr_init2(op1, 64);
    mpfr_init2(dot, 128);
    mpfr_init2(div, 128);
    mpfr_init2(sum, 4196);

    //Produce a result matrix of TRSV using MPFR
    for(int i = n-1; i >= 0; i--) {
        mpfr_set_d(sum, b[i], MPFR_RNDN);
        for(int j = i+1; j < n; j++) {
            mpfr_set_d(op1, a[i * n + j], MPFR_RNDN);
            mpfr_mul_d(dot, op1, b[j], MPFR_RNDN);
            mpfr_sub(sum, sum, dot, MPFR_RNDN);
        }
        mpfr_div_d(div, sum, a[i * (n + 1)], MPFR_RNDN);
        b[i] = mpfr_get_d(div, MPFR_RNDD);
        printf("%.4g  ", b[i]);
    }
    printf("\n");

    double norm = 0.0;
    //Compare the GPU and MPFR results
    for (int i = 0; i < n; i++)
        norm += pow(fabs(trsv[i] - b[i]), 2);
    norm = ::sqrt(norm);
    printf("    Compared to MPFR. Norm = %.17g\n", norm);

    mpfr_free_cache();

    return norm < epsilon ? true : false;
}

extern "C" bool compareTRSVLNUToMPFR(
    const double *a,
    double *b,
    const double *trsv,
    const int n,
    const double epsilon
) {
    mpfr_t sum, dot, op1;

    mpfr_init2(op1, 64);
    mpfr_init2(dot, 128);
    mpfr_init2(sum, 4196);

    //Produce a result matrix of TRSV using MPFR
    for(int i = 1; i < n; i++) {
        mpfr_set_d(sum, b[i], MPFR_RNDN);
        for(int j = 0; j < i; j++) {
            mpfr_set_d(op1, a[j * n + i], MPFR_RNDN);
            mpfr_mul_d(dot, op1, b[j], MPFR_RNDN);
            mpfr_sub(sum, sum, dot, MPFR_RNDN);
        }
        b[i] = mpfr_get_d(sum, MPFR_RNDD);
    }

    double norm = 0.0;
    //Compare the GPU and MPFR results
    for (int i = 0; i < n; i++)
        norm += pow(fabs(trsv[i] - b[i]), 2);
    norm = ::sqrt(norm);
    printf("    Compared to MPFR. Norm = %.17g\n", norm);

    mpfr_free_cache();

    return norm < epsilon ? true : false;
}

extern "C" double verifyTRSVUNN(
    const double *a,
    const double *b,
    const double *x,
    const int n,
    const double epsilon
) {
    double norm = 0.0;

    for(int i = 0; i < n; i++) {
        double sum = 0.0;
        for(int j = i; j < n; j++)
            sum += a[i * n + j] * x[j];

        norm += pow(fabs(sum - b[i]), 2);
    }

    return ::sqrt(norm);
}

extern "C" bool verifyTRSVLNU(
    const double *a,
    const double *b,
    const double *x,
    const int n,
    const double epsilon
) {
    bool pass = true;

    for(int i = 0; i < n; i++) {
        double sum = 0.0;
        for(int j = 0; j <= i; j++)
            sum += a[j * n + i] * x[j];

        if (fabs(sum - b[i]) > epsilon) {
            printf("[%d] %.17g \t %.17g\n", i, b[i], sum);
            pass = false;
            break;
        }
    }

    return pass;
}

extern "C" double condA(
    const double *a,
    const int n
) {
    double min = fabs(a[0]), max = fabs(a[0]);

    for (int i = 1; i < n; i++) {
        if (fabs(a[i * (n + 1)]) < min)
            min = fabs(a[i * (n + 1)]);
        if (fabs(a[i * (n + 1)]) > max)
            max = fabs(a[i * (n + 1)]);
    }
    printf("min = %.4g  %.4g\n", min, a[0]);
    printf("max = %.4g  %.4g\n", max, a[0]);

    return max / min;
}

extern "C" double TwoProductFMA(double a, double b, double *d) {
    double p = a * b;
    *d = fma(a, b, -p);
    return p;
}

extern "C" void printMatrix(
    const double *A,
    const uint n
){
    for (uint i = 0; i < n; i++) {
        for (uint j = 0; j < n; j++)
             printf("%.4g\t", A[j * n + i]);
        printf("\n");
    }
}

extern "C" void printVector(
    const double *a,
    const uint n
){
    for (uint i = 0; i < n; i++)
        printf("%.4g\t", a[i]);
    printf("\n");
}

