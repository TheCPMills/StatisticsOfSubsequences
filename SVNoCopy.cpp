#define EIGEN_NO_DEBUG 1
// TODO: PUT ABOVE BACK

#include <Eigen/Dense>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iostream>
using namespace std;
using Eigen::Array;
using Eigen::ArrayXd;

#define length 17

const bool PRINT_EVERY_ITER = true;

// equal to pow(2, 2 * length)
const uint64_t powminus0 = uint64_t(1) << (2 * length);
// equal to pow(2, 2 * length - 1)
const uint64_t powminus1 = uint64_t(1) << ((2 * length) - 1);
// equal to pow(2, 2 * length - 2)
const uint64_t powminus2 = uint64_t(1) << ((2 * length) - 2);

// TODO: in all array accesses, get rid of range checking
// https://eigen.tuxfamily.org/dox/group__QuickRefPage.html
// (what's difference between vec[i] and vec(i)?)

// make sure to pass by reference, not by value
// https://eigen.tuxfamily.org/dox/group__TopicPassingByValue.html
// TODO: look into using Ref's (or smart pointers?)

// Seems like they actually recommend using dynamic arrays for large sizes
// even if you know the size at compile time.
// https://eigen.tuxfamily.org/dox/group__TutorialMatrixClass.html

// TODO: figure out if this matters
// https://eigen.tuxfamily.org/dox/TopicFunctionTakingEigenTypes.html

// See https://eigen.tuxfamily.org/dox/TopicPreprocessorDirectives.html for flags to consider
// E.g., EIGEN_NO_DEBUG (currently set at top, but idk if I'm using it right?)
// May be possible to get around stack size issues with this. Unsure if good idea?

// TODO: see if -m64 compile flag matters
// TODO: check if can use https://eigen.tuxfamily.org/dox/TopicUsingIntelMKL.html

// TODO: see if you can take advantage of symmetry (1's complement operator ~)

void printArray(const ArrayXd &arr) {
    for (int i = 0; i < arr.size(); i++) {
        cout << arr[i] << " ";
    }
    cout << endl;
}

// void loopUntilTolerance(double tol){
//     double prev = 0.0;
//     if (prev == 0.0 || tol-prev > )
// }

void F_01(const ArrayXd &v, ArrayXd &ret) {
    // cout << "Has memory here" << endl;
    // cout.flush();
    ret = ArrayXd::Zero(powminus1);
    // cout << "Ran out of memory before here" << endl;
    // cout.flush();

    // TODO: Make these uint64_t
    const uint64_t start = powminus2;
    const uint64_t end = powminus1;

    // maybe convert into bitvec
    for (uint64_t str = start; str < end; str++) {
        const uint64_t A = str & 0xAAAAAAAAAAAAAAAA;
        // the & is to just get rid of first character
        const uint64_t TB = (str & 0x5555555555555555) & ((uint64_t(1) << (2 * length - 2)) - 1);
        uint64_t ATB0 = A | (TB << 2);
        uint64_t ATB1 = ATB0 | 1;  // 0b1 <- the smallest bit in B is set to 1

        // I wonder if assigning to new variables is faster?
        ATB0 = std::min(ATB0, (powminus0 - 1) - ATB0);
        ATB1 = std::min(ATB1, (powminus0 - 1) - ATB1);
        ret[str - start] = v[ATB0] + v[ATB1];  // if h(A) != h(B) and h(A) = z
    }

    // from halfway to 3/4 of way
    for (uint64_t str = end; str < 3 * start; str++) {
        const uint64_t TA = (str & 0xAAAAAAAAAAAAAAAA) & ((uint64_t(1) << (2 * length - 1)) - 1);
        const uint64_t B = str & 0x5555555555555555;
        uint64_t TA0B = (TA << 2) | B;
        uint64_t TA1B = TA0B | 2;

        // I wonder if assigning to new variables is faster?
        TA0B = std::min(TA0B, (powminus0 - 1) - TA0B);
        TA1B = std::min(TA1B, (powminus0 - 1) - TA1B);

        ret[str - start] = v[TA0B] + v[TA1B];  // if h(A) != h(B) and h(A) = z
    }
    //  return ret;
}

void F_12(const ArrayXd &v, ArrayXd &ret) {
    // ret = ArrayXd::Zero(powminus2);
    for (uint64_t str = 0; str < powminus2; str++) {
        // TODO: LENGTHEN THESE FIXED STRINGS
        const uint64_t TA = (str & 0xAAAAAAAAAAAAAAAA) & ((uint64_t(1) << (2 * length - 1)) - 1);
        const uint64_t TB = (str & 0x5555555555555555) & ((uint64_t(1) << (2 * length - 2)) - 1);
        uint64_t TA0TB0 = (TA << 2) | (TB << 2);
        uint64_t TA0TB1 = TA0TB0 | 0b1;
        uint64_t TA1TB0 = TA0TB0 | 0b10;
        uint64_t TA1TB1 = TA0TB0 | 0b11;
        // There might be an argument to split this for loop into 4,
        // so that something something cache hits/optimization.
        // Probably not though.

        // I wonder if assigning to new variables is faster?
        TA0TB0 = std::min(TA0TB0, (powminus0 - 1) - TA0TB0);
        TA0TB1 = std::min(TA0TB1, (powminus0 - 1) - TA0TB1);
        TA1TB0 = std::min(TA1TB0, (powminus0 - 1) - TA1TB0);
        TA1TB1 = std::min(TA1TB1, (powminus0 - 1) - TA1TB1);
        ret[str] = 1 + .25 * (v[TA0TB0] + v[TA0TB1] + v[TA1TB0] + v[TA1TB1]);
        // todo: figure out if doing 1+.25* is faster here (locally) or after at end
    }
    // return ret;
}

void F(const ArrayXd &v1, const ArrayXd &v2, ArrayXd &ret) {
    // try a potential optimization? perform 0.5* locally?
    // ArrayXd f01f11;
    F_01(v1, ret);  // contains both f01 and f11 back to back
    // reverses f11
    // ret(Eigen::seq(powminus2, powminus1 - 1)).reverseInPlace();
    // TODO: having to reverse seems inefficient. can it be computed in reverse order to start?
    // (reversing is just so we can compare vals?)
    //(remember: any changes made in here have to be made in F_withplusR as well)

    // The second array has its elements reversed (iterated in reverse order)
    // The .eval() is necessary to prevent aliasing issues.

    // mmap can do 2GB at once
    ret(Eigen::seq(powminus2, powminus1 - 1)) =
        0.5 * (ret(Eigen::seq(0, powminus2 - 1)).max(ret(Eigen::seq(powminus1 - 1, powminus2, -1))).eval());

    // ArrayXd f_double;
    F_12(v2, ret);

    // Combine f_double and f01f11
    // TODO: figure out how to assign this without doing any (or very minimal) copying
    // because I am unsure if this copies the values to ret or just uses pointer
    // ret << (1 + 0.25 * f_double), f01f11;
    // ret(Eigen::seq(powminus2, powminus1 - 1)) = f01f11;
    // TODO: BENCHMARK IF THIS NEW NO-COPY METHOD IS FASTER
}

// Exactly like F, but saves memory as it can be fed v, v+R but use only one vector
void F_withplusR(double R, ArrayXd &v2, ArrayXd &ret) {
    // now add R to v2
    // v2 = v2 + R;  // I wonder if this even necessary to do here. Why not do it after everything has been computed?
    // that is, why not do f01f11 = f01f11 + 2*R
    // ArrayXd f01f11;
    F_01(v2, ret);  // contains both f01 and f11 back to back
    // reverses f11
    // ret(Eigen::seq(powminus2, powminus1 - 1)).reverseInPlace();

    // v2+R is NOT fed to F. Instead, since ret[] is always set to v[] + v[], we can just add 2*R at the end.
    // The .eval() is necessary to prevent aliasing issues.
    ret(Eigen::seq(powminus2, powminus1 - 1)) =
        0.5 * (ret(Eigen::seq(0, powminus2 - 1)).max(ret(Eigen::seq(powminus1 - 1, powminus2, -1))).eval()) + 2 * R;

    // v2 without R added
    // ArrayXd f_double;
    F_12(v2, ret);

    // Combine f_double and f01f11
    // TODO: figure out how to assign this without doing any (or very minimal) copying
    // because I am unsure if this copies the values to ret or just uses pointer
    // ret << (1 + 0.25 * f_double), f01f11;
    // ret(Eigen::seq(powminus2, powminus1 - 1)) = f01f11;
}

void FeasibleTriplet(int n) {
    // TODO: time benchmarking for each part
    //  ArrayXd v0 = ArrayXd::Zero(powminus1);
    auto start = std::chrono::system_clock::now();
    ArrayXd v1 = ArrayXd::Zero(powminus1);

    // ArrayXd u = ArrayXd::Zero(powminus1);  // is u ever used? get rid of it?
    double r = 0;
    double e = 0;
    for (int i = 2; i < n + 1; i++) {
        ArrayXd v2(powminus1);
        // cout << "AAAAAAAAAA";
        // cout << "snarf" << endl;
        // cout.flush();
        // Writes new vector (v2) into v2
        F(v1, v1, v2);

        // if this uses a vector of mem temporarily, store v2-v1 into v1?
        double R = (v2 - v1).maxCoeff();
        // Beyond this point, v1's values are no longer needed, so we reuse
        // its memory for other computations.

        // ArrayXd retF(powminus1);
        // NOTE: THE v1's HERE ARE REUSED MEMORY. v1's VALUES NOT USED, INSTEAD
        // v1 IS COMPLETELY OVERWRITTEN HERE.
        // Normally F(v2+R, v2, v0), but I think this ver saves an entire vector
        // of memory at the cost of a small amount more computation.
        F_withplusR(R, v2, v1);
        // v2 = v2 + R;

        // Idea: normally below line is ArrayXd W = v2 + 2 * R - v0;
        // However, special F function adds R, then we add R again to v2 beforehand (this has since been changed)
        // Further, we again reuse v1 to store W.
        v1 = v2 + 2 * R - v1;
        double E = std::max(0.0, v1.maxCoeff());

        // FIXME: FIGURE OUT WHAT NEW IF STATEMENT NEEDS TO BE
        if (R - E >= r - e) {
            // u = v2;  // TODO: is u ever used? get rid of it?
            r = R;
            e = E;
        }
        r = R;
        e = E;
        // TODO: verify that these are copying reference, not value?
        // v0 = v1;
        // could maybe make this swap pointers instead
        v1 = v2;

        // FIXME: ISSUE, NORMAL FUNCTION IS DECREASING AS IT RUNS INSTEAD OF GROWING.
        // RESULTS ARE THUS NOT NECESSARILY VALID LOWER BOUNDS? NEED TO VERIFY CORRECTNESS.
        if (PRINT_EVERY_ITER) {
            // cout << "At n=" << i << ": " << 2.0 * (r - e) << endl;
            // cout << "At n=" << i << ": " << 2.0 * (r - e) / (1 + (r - e)) << endl;
            cout << "At n=" << i << ": " << (2.0 * r / (1 + r)) + 2.0 * (r - e) / (1 + (r - e)) << endl;
            // cout << "At n=" << i << ": " << 2.0 * (e - r) / (1 + (e - r)) << endl;
            // cout << "At n=" << i << ": " << 2.0 * (e - r) << endl;
            // cout << "R, E: " << R << " " << E << endl;
            // cout << (2.0 * r / (1 + r)) << endl;
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = end - start;
            cout << "Elapsed time: " << elapsed_seconds.count() << endl;
        }

        // cout << r << " " << R << endl;
    }

    // return u, r, e
    cout << "Result: " << 2.0 * (r - e) << endl;
    cout << "Testing New Result: " << 2.0 * r / (1 + r) << endl;
}

int main() {
    cout << "Starting with l = " << length << "..." << endl;
    auto start = std::chrono::system_clock::now();
    FeasibleTriplet(200);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    cout << "Elapsed time: " << elapsed_seconds.count() << endl;

    return 0;
}