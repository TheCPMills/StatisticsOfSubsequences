// #define EIGEN_NO_DEBUG 1
//   TODO: PUT ABOVE BACK

#include <Eigen/Dense>
#include <bitset>
#include <chrono>
#include <cmath>
#include <ctime>
#include <future>
#include <iostream>
#include <thread>
using Eigen::ArrayXd;
using std::cout;
using std::endl;

#define length 3
#define NUM_THREADS 4
// Careful: ensure that NUM_THREADS divides 2^(2*length-1) (basically always will for l > 3 if power of 2)

const bool PRINT_EVERY_ITER = true;

// equal to pow(2, 2 * length)
const uint64_t powminus0 = uint64_t(1) << (2 * length);
// equal to pow(2, 2 * length - 1)
const uint64_t powminus1 = uint64_t(1) << ((2 * length) - 1);
// equal to pow(2, 2 * length - 2)
const uint64_t powminus2 = uint64_t(1) << ((2 * length) - 2);

template <typename Derived>
void printArray(const Eigen::ArrayBase<Derived> &arr) {
    for (int i = 0; i < arr.size(); i++) {
        cout << arr[i] << " ";
    }
    cout << endl;
}

// void loopUntilTolerance(double tol){
//     double prev = 0.0;
//     if (prev == 0.0 || tol-prev > )
// }

// TODO: consider reversing order of strings so that no &'ing needs to happen, you just bitshift right
//  instead of left? (may not work for iterating through strings. need to figure out.)

/* Normally, we find R with R = (v2 - v1).maxCoeff();
    However, this is slower than the multithreaded F functions if not parallelized!
    So instead we break that calculation up across threads.
    Additionally, E = std::max(0.0, (v2 + 2 * R - v1).maxCoeff()). We can use
    this function for that as well, and just add 2*R at the end. */
double subtract_and_find_max_parallel(const ArrayXd &v1, const ArrayXd &v2) {
    std::future<double> maxVals[NUM_THREADS];
    const uint64_t incr = powminus1 / NUM_THREADS;

    // TODO: see if writing own function to do this insttead of Eigen is faster
    // Function to calculate the maximum coefficient in a particular (start...end) slice
    auto findMax = [&v1, &v2](uint64_t start, uint64_t end) {
        return (v2(Eigen::seq(start, end - 1)) - v1(Eigen::seq(start, end - 1))).maxCoeff();
    };

    // Set threads to calculate the max coef in their own smaller slices
    for (int i = 0; i < NUM_THREADS; i++) {
        maxVals[i] = std::async(std::launch::async, findMax, incr * i, incr * (i + 1));
    }

    // Now calculate the global max
    double R = maxVals[0].get();  // .get() waits until the thread completes
    for (int i = 1; i < NUM_THREADS; i++) {
        double coef = maxVals[i].get();
        if (coef > R) {
            R = coef;
        }
    }
    return R;
}

/* This function is essentially the combined form of F_01_loop1 and F_01_loop2, with the elementwise maximum
 * between loop 1 and loop 2's values also being performed locally. This means we have two strings: the string
 * computed by iterating through loop 1 (str), and the string from loop 2 it needs to be compared to for
 * elementwise maximum (str3).
 * On top of that, another property is taken advantage of to reuse some computation.
 * If you add powminus2 to str to get str2, then some of the values computed for str2 (which is an element
 * normally computed in loop2) will be the same as those computed for str. So we reuse those values to calculate
 * str2's values here. We can then also notice that if we subtract powminus2 from str3 to get str4 (which we can
 * similarly reuse some parts of the computation to do), we get the string we need to do a maximum for str2
 * with. So we can use this to fill in another entry. So at every iteration we are computing 4 entries (reusing
 * computation where possible), and then reducing down to 2 entries (one at the start of array, one at the end)
 * by taking the max.
 * Visually:
 *     first half of arr           second half of arr           outside bounds of arr
 *                           val1                       val2
 * [                         |-->           |           <--]
 *
 *                           str                       str4 str2                     str3
 * [                         |-->           |           <--]-->          |           <--|
 * 0                       powm2         +powm3          powm1        +powm3         +powm2*/
void F_01_combined(const uint64_t start, const uint64_t end, const ArrayXd &v, ArrayXd &ret, const double R) {
    for (uint64_t str = start; str < end; str++) {
        // save val for loop 2
        const uint64_t str2 =
            str + powminus2;  // this will ALWAYS have the effect of setting biggest 1 to 0, and putting 1 to left
        const uint64_t str3 = powminus0 + powminus2 - 1 - str2;
        // may be able to do this subtraction as 2 bitwise ops? & and ~?
        const uint64_t str4 = str3 - powminus2;

        // Compute as in Loop 1
        // Take every other bit (starting at first position)
        const uint64_t A = str & 0xAAAAAAAAAAAAAAAA;
        // Take every other bit (starting at second position)
        // The second & gets rid of the first bit
        const uint64_t TB = (str & 0x5555555555555555) & (powminus2 - 1);
        const uint64_t ATB0 = A | (TB << 2);
        const uint64_t ATB1 = ATB0 | 1;  // 0b1 <- the smallest bit in B is set to 1

        // Compute as in Loop 2
        const uint64_t TA = A;
        // const uint64_t TA = (str2 & 0xAAAAAAAAAAAAAAAA) & (powminus1 - 1);  //equivalent!
        // Take every other bit (starting at second position)
        const uint64_t B = TB;
        // const uint64_t B = str2 & 0x5555555555555555; //equivalent!
        uint64_t TA0B = (TA << 2) | B;
        uint64_t TA1B = TA0B | 2;

        // TODO: may be clever way of figuring out these mins ahead of time
        TA0B = std::min(TA0B, (powminus0 - 1) - TA0B);
        TA1B = std::min(TA1B, (powminus0 - 1) - TA1B);

        // Calculate the values for str3 (done as in Loop 2)
        const uint64_t TA3 = (str3 & 0xAAAAAAAAAAAAAAAA) & (powminus1 - 1);

        // Take every other bit (starting at second position)
        const uint64_t B3 = str3 & 0x5555555555555555;
        uint64_t TA0B3 = (TA3 << 2) | B3;
        uint64_t TA1B3 = TA0B3 | 2;
        // TODO: there should be a way of doing HALF the calculations, since +2 does nothing?

        TA0B3 = std::min(TA0B3, (powminus0 - 1) - TA0B3);
        TA1B3 = std::min(TA1B3, (powminus0 - 1) - TA1B3);

        const double loop1val = v[ATB0] + v[ATB1];
        const double loop2val = v[TA0B] + v[TA1B];
        const double loop2valcomp = v[TA0B3] + v[TA1B3];
        ret[str] = 0.5 * std::max(loop1val, loop2valcomp) + R;  //+2*R //TODO:ADD THIS

        // for str4
        const uint64_t A_2 = TA3;
        const uint64_t TB_2 = B3;
        const uint64_t ATB0_2 = A_2 | (TB_2 << 2);
        const uint64_t ATB1_2 = ATB0_2 | 1;  // 0b1 <- the smallest bit in B is set to
        const double loop1val2 = v[ATB0_2] + v[ATB1_2];
        ret[str4] = 0.5 * std::max(loop2val, loop1val2) + R;

        //  TODO: loop2val is symmetric USE THIS FACT!

        //(+R because +R in each v value, but then 0.5*result)
    }
}

void F_01(const ArrayXd &v, ArrayXd &ret, const double R) {
    const uint64_t start = powminus2;
    const uint64_t end = powminus2 + powminus2 / 2;
    std::thread threads[NUM_THREADS];
    const uint64_t incr = (end - start) / (NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] =
            std::thread(F_01_combined, start + incr * i, start + incr * (i + 1), std::cref(v), std::ref(ret), R);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].join();
    }

    //  return ret;
}

void F_12(const ArrayXd &v, ArrayXd &ret) {
    auto loop = [&v, &ret](uint64_t start, uint64_t end) {
        for (uint64_t str = start; str < end; str++) {
            // const uint64_t TA = (str & 0xAAAAAAAAAAAAAAAA) & (powminus1 - 1);
            // const uint64_t TB = (str & 0x5555555555555555) & (powminus2 - 1);
            // uint64_t TA0TB0 = (TA << 2) | (TB << 2);
            uint64_t TA0TB0 = (str & (powminus2 - 1)) << 2;  // equivalent to above 3 lines!
            uint64_t TA0TB1 = TA0TB0 | 0b1;
            uint64_t TA1TB0 = TA0TB0 | 0b10;
            uint64_t TA1TB1 = TA0TB0 | 0b11;
            // There might be an argument to split this for loop into 4,
            // so that something something cache hits/optimization.
            // Probably not though.

            // TODO: there may be a clever way of figuring out these mins AHEAD of time (check if any unnecessary)
            // like, it's really just asking if it's > powminus1
            // I wonder if assigning to new variables is faster?
            TA0TB0 = std::min(TA0TB0, (powminus0 - 1) - TA0TB0);
            TA0TB1 = std::min(TA0TB1, (powminus0 - 1) - TA0TB1);
            TA1TB0 = std::min(TA1TB0, (powminus0 - 1) - TA1TB0);
            TA1TB1 = std::min(TA1TB1, (powminus0 - 1) - TA1TB1);
            ret[str] = 1 + .25 * (v[TA0TB0] + v[TA0TB1] + v[TA1TB0] + v[TA1TB1]);
            // TODO: figure out if doing 1+.25* is faster here (locally) or after at end
            // TODO: implement the symmetry
        }
    };
    std::thread threads[NUM_THREADS];
    const uint64_t start = 0;
    const uint64_t end = powminus2;
    const uint64_t incr = (end - start) / (NUM_THREADS);  // Careful to make sure NUM_THREADS is a divisor!
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = std::thread(loop, start + incr * i, start + incr * (i + 1));
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].join();
    }

    // return ret;
}

// (remember: any changes made in here have to be made in F_withplusR as well)
void F(const ArrayXd &v1, const ArrayXd &v2, ArrayXd &ret) {
    // Computes f01 and f11, does their elementwise maximum, and places it into second half of ret
    auto start2 = std::chrono::system_clock::now();
    F_01(v1, ret, 0);
    auto end2 = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end2 - start2;
    cout << "Elapsed time F1 (s): " << elapsed_seconds.count() << endl;

    //  Puts f_double into the first half of the ret vector
    start2 = std::chrono::system_clock::now();
    F_12(v2, ret);
    end2 = std::chrono::system_clock::now();
    elapsed_seconds = end2 - start2;
    cout << "Elapsed time F3 (s): " << elapsed_seconds.count() << endl;
}

// Exactly like F, but saves memory as it can be fed v, v+R but use only one vector
void F_withplusR(const double R, ArrayXd &v2, ArrayXd &ret) {
    // v2+R is normally fed to F_01. However, we can actually avoid doing so.

    // v2+R is NOT fed to F_01. Instead, since ret[] is always set to v[] + v[], we can just add 2*R at the end.
    F_01(v2, ret, R);
    /* While it would be nice to do this function first and then add 2*R to the vector, it would prevent us from reusing
    memory since F_12 requires half a vector, but F_01 temporarily requires a full vector (as currently implemented).
    v2 without R added*/
    F_12(v2, ret);
}

void FeasibleTriplet(int n) {
    //  ArrayXd v0 = ArrayXd::Zero(powminus1); // unneeded in single vec impl
    auto start = std::chrono::system_clock::now();
    ArrayXd v1 = ArrayXd::Zero(powminus1);

    // ArrayXd u = ArrayXd::Zero(powminus1);  // u never used
    ArrayXd v2(powminus1);  // we pass a pointer to this, and have it get filled in

    double r = 0;
    double e = 0;
    for (int i = 2; i < n + 1; i++) {
        // Writes new vector (v2) into v2
        auto start2 = std::chrono::system_clock::now();
        F(v1, v1, v2);
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start2;
        cout << "Elapsed time F (s): " << elapsed_seconds.count() << endl;

        start2 = std::chrono::system_clock::now();
        // TODO: if this uses a vector of mem temporarily, store v2-v1 into v1?
        const double R = subtract_and_find_max_parallel(v1, v2);
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start2;
        cout << "Elapsed time (s) mc1: " << elapsed_seconds.count() << endl;
        //  Beyond this point, v1's values are no longer needed, so we reuse
        //  its memory for other computations.

        // NOTE: THE v1's HERE ARE REUSED MEMORY. v1's VALUES NOT USED, INSTEAD
        // v1 IS COMPLETELY OVERWRITTEN HERE.
        // Normally F(v2+R, v2, v0), but I think this ver saves an entire vector
        // of memory.
        start2 = std::chrono::system_clock::now();
        F_withplusR(R, v2, v1);
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start2;
        cout << "Elapsed time FpR (s): " << elapsed_seconds.count() << endl;
        // TODO: INVESTIGATE WHY ABOVE IS SO MUCH FASTER THAN F??
        // except now it isnt being faster???

        // Idea: normally below line is ArrayXd W = v2 + 2 * R - v0;
        // We again reuse v1 to store W, and with single vector v0 is replaced by v1.
        start2 = std::chrono::system_clock::now();
        const double E = std::max(subtract_and_find_max_parallel(v1, v2) + 2 * R, 0.0);
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start2;
        cout << "Elapsed time mc2 (s): " << elapsed_seconds.count() << endl;

        // TODO: +2*R keeps getting added places... wonder if it's at all necessary to do,
        //  and if we can instead just do it at the end for R and E?

        // // FIXME: FIGURE OUT WHAT NEW IF STATEMENT NEEDS TO BE
        // if (R - E >= r - e) {
        //     // u = v2;  // u is never used
        //     r = R;
        //     e = E;
        // }
        r = R;
        e = E;
        // Swap pointers of v1 and v2
        // v0 = v1;
        // v1 = v2;
        std::swap(v1, v2);

        // FIXME: Need to verify mathematically the correctness of this new calculation.
        if (PRINT_EVERY_ITER) {
            // (other quantities of potential interest)
            // cout << "At n=" << i << ": " << 2.0 * (r - e) << endl;
            // cout << "At n=" << i << ": " << 2.0 * (r - e) / (1 + (r - e)) << endl;
            // cout << "At n=" << i << ": " << 2.0 * (e - r) / (1 + (e - r)) << endl;
            // cout << "At n=" << i << ": " << 2.0 * (e - r) << endl;
            // cout << "R, E: " << R << " " << E << endl;
            // cout << (2.0 * r / (1 + r)) << endl;
            cout << "At n=" << i << ": " << (2.0 * r / (1 + r)) + 2.0 * (r - e) / (1 + (r - e)) << endl;
            end = std::chrono::system_clock::now();
            elapsed_seconds = end - start;
            cout << "Elapsed time (s): " << elapsed_seconds.count() << endl;
        }

        // cout << r << " " << R << endl;
    }

    // return u, r, e
    // cout << "Result: " << 2.0 * (r - e) << endl;
    cout << "Single Vec Result: " << 2.0 * r / (1 + r) << endl;
    cout << "(Alt more acc): " << (2.0 * r / (1 + r)) + 2.0 * (r - e) / (1 + (r - e)) << endl;
    // cout << "v1" << endl;
    // printArray(v1);
    // cout << "v2" << endl;
    // printArray(v2);
}

int main() {
    cout << "Starting with l = " << length << "..." << endl;
    auto start = std::chrono::system_clock::now();
    FeasibleTriplet(50);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    cout << "Elapsed time (s): " << elapsed_seconds.count() << endl;
    // TODO: check if eigen init parallel is something to do
    return 0;
}