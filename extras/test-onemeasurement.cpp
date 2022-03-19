#include <stdlib.h>
#include <cstdint>
#include <iostream>

static constexpr std::uint32_t kNumMeasurements = 21;

extern "C" {
  static int compare16(const void *pLeft, const void *pRight);
}

static int compare16(const void *pLeft, const void *pRight)
    {
    auto p = (const std::uint16_t *)pLeft;
    auto q = (const std::uint16_t *)pRight;

    return (int)*p - (int)*q;
    }

void processOneMeasurement(
    float &r,
    std::uint16_t *pv
    )
    {
    // set pointers to q1 and q3 cells by counting in symmetrically from the ends of the vector.
    // For example if kNumMeasurements is 10, then we call the q1 value pv[2], and the
    // q3 value is pv[7]; values [0],[1] are below q1, and [8],[9] are above q3.
    const std::uint16_t * const pq1 = pv + kNumMeasurements / 4;
    const std::uint16_t * const pq3 = pv + kNumMeasurements - (kNumMeasurements / 4) - 1;

    // sort pv in place.
    qsort(pv, kNumMeasurements, sizeof(pv[0]), compare16);

    // calculate IQR = q3 - q1
    std::int32_t iqr = *pq3 - *pq1;

    // calculate 1.5 IRQ. It's positive, so >> is well defined.
    std::int32_t iqr15 = (3 * iqr) >> 1;

    // calcluate the low and high limits.
    std::int32_t lowlim = pq1[0] - iqr15;
    std::int32_t highlim = pq3[0] + iqr15;

    std::cout << "min: " << pv[0] << " q1: " << pq1[0] << " q3: " << pq3[0] << " max: " << pv[kNumMeasurements - 1] << "\n";
    std::cout << "iqr: " << iqr << "  iqr15: " << iqr15 << " lowlim: " << lowlim << " highlim: " << highlim << "\n";

    // define left and right pointers for summation.
    const std::uint16_t *p1;
    const std::uint16_t *p2;

    // scan from left to find first value to accumulate.
    for (p1 = pv; p1 < pq1 && *p1 < lowlim; ++p1)
        ;

    // scan from right to find last value to accumulate.
    for (p2 = pv + kNumMeasurements - 1; pq3 < p2 && *p2 > highlim; --p2)
        ;

    // sum the values.
    std::uint32_t sum = 0;
    for (auto p = p1; p <= p2; ++p)
        sum += *p;

    // divide by n * 65536.0
    float const div = (p2 - p1 + 1); // * 65535.0f;
    std::cout << "div: " << div << "\n";

    // store into r.
    r = sum / div;
    }

int main(int ac, char **av)
    {
    std::uint16_t vIn[kNumMeasurements] = {
        777, 789, 771, 753, 738, 750, 762, 711, 729, 798, 798, 756, 756, 747, 747, 795, 795, 789, 789, 831, 831,
    };
    float result;

    processOneMeasurement(result, vIn);

    std::cout << "result: " << result << "\n";
    return 0;
    }
