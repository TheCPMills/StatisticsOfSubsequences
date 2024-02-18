// define EIGEN_NO_DEBUG 1

#include <Eigen/Dense>
#include <bitset>
#include <chrono>
#include <cmath>
#include <ctime>
#include <future>
#include <iostream>
#include <string>
#include <thread>
using Eigen::ArrayXd;
using std::cout;
using std::endl;

#define length 4
#define string_count 3
#define alphabet_size 3
#define NUM_THREADS 1
// Careful: ensure that NUM_THREADS divides alphabet_size^(string_count*length-1)
#define CALC_EVERY_X_ITERATIONS 1

const bool PRINT_EVERY_ITER = true;

const uint64_t powminus0 = pow(alphabet_size, string_count * length);
const uint64_t powminus1 = pow(alphabet_size, string_count * length - 1);
const uint64_t powminus2 = pow(alphabet_size, string_count * length - 2);
const uint64_t powminus3 = pow(alphabet_size, string_count * length - 3);

// F_b_step is the number of strings that start with a specific character (e.g., all triplets of strings starting with 1, 1, 0)
const uint64_t F_b_step = pow(alphabet_size, string_count * (length - 1));
// F_b_equals_1 is the number of starting character combinations between (0, 0, ...) and (1, 1, ...), which is the same length for any (x, x, ...) and (x + 1, x + 1, ...)
const uint64_t F_b_equals_1 = ((1 - pow(alphabet_size, string_count)) / (1 - alphabet_size));
const std::string base_digits = "0123456789"; // Digits in base 1-16. Add more digits if attempting to run for alphabet_size > 16

template <typename Derived>
void printArray(const Eigen::ArrayBase<Derived>& arr)
{
    for (int i = 0; i < arr.size(); i++)
    {
        cout << arr[i] << " ";
    }
    cout << endl;
}

/* Normally, we find R with R = (vNew - v[d - 1]).maxCoeff();
    However, this is slower than the multithreaded F functions if not parallelized!
    So instead we break that calculation up across threads.
    Additionally, E = std::max(0.0, (vNew + 2 * R - F_withR).maxCoeff()). We can use
    this function for that as well, and just move the constant d*R to outside this function */
double subtract_and_find_max_parallel(const ArrayXd& v1, const ArrayXd& v2)
{
    std::future<double> maxVals[NUM_THREADS];
    const uint64_t incr = powminus1 / NUM_THREADS;

    // Function to calculate the maximum coefficient in a particular (start...end) slice
    auto findMax = [&v1, &v2](uint64_t start, uint64_t end)
        {
            return (v2(Eigen::seq(start, end - 1)) - v1(Eigen::seq(start, end - 1))).maxCoeff();
        };

    // Set threads to calculate the max coef in their own smaller slices
    for (int i = 0; i < NUM_THREADS; i++)
    {
        maxVals[i] = std::async(std::launch::async, findMax, incr * i, incr * (i + 1));
    }

    // Now calculate the global max
    double R = maxVals[0].get(); // .get() waits until the thread completes
    for (int i = 1; i < NUM_THREADS; i++)
    {
        double coef = maxVals[i].get();
        if (coef > R)
        {
            R = coef;
        }
    }
    return R;
}

// Basically variate
int stringsToInt(std::string initial[], bool shouldVariate[], int variateValue) {
    int output = 0;

    for (int l = 0; l < length; l++) {
        for (int d = 0; d < string_count; d++) {
            output *= alphabet_size;

            // TODO: Would likely be much more efficient to use contiguous representation (since actual characters don't matter)
            //, and then subtract '0' or something like that
            if (shouldVariate[d]) {
                if (l == length - 1) { //At the end of the string, apply the variateValue
                    //Order doesn't matter since the order is consistent, and variateValue will take on every possible variation
                    //TODO: this is gonna need to be fixed weeeee
                    output += variateValue % alphabet_size;
                    variateValue /= alphabet_size;
                }
                else { //If not at the end of the string, shift characters left one space (str[0] is removed)
                    output += base_digits.find(initial[d][l + 1]);
                }
            }
            else {
                output += base_digits.find(initial[d][l]);
            }
        }
    }

    return output;
}

double variate(const ArrayXd& v, std::string variating[], int numNz, bool variationPos[])
{
    double output = 0.0;

    // Iterates through every combination of digits for the numNz count of strings that need to be variated
    for (int varExp = 0; varExp < pow(alphabet_size, numNz); varExp++)
    {
        uint64_t index = stringsToInt(variating, variationPos, varExp);
        output += v[index];
    }
    
    return output;
}

void intToStrings(int index, std::string ret[])
{
    for (int i = string_count * length - 1; i >= 0; i--)
    {
        // Order of strings in ret doesn't matter, adds characters from end to front
        ret[i % string_count][i / string_count] = base_digits[index % alphabet_size];
        index /= alphabet_size;
    }
}

double Fz(int z, const ArrayXd v[], int index)
{
    std::string indices[string_count];
    for (int i = 0; i < string_count; i++)
    {
        indices[i].insert(0, length, ' ');
    }

    intToStrings(index, indices);

    int numNz = 0;
    bool NzPos[string_count]{};
    for (int i = 0; i < string_count; i++)
    {
        bool zDigit = indices[i][0] != base_digits[z];
        numNz += zDigit; // Adds 1 if digit is not the same
        NzPos[i] = zDigit;
    }

    if (numNz == 0) // All digits are the same, no variation occurs
    {
        return 0;
    }

    return pow(alphabet_size, -numNz) * variate(v[numNz - 1], indices, numNz, NzPos);
}

// (remember: any changes made in here have to be made in F_withplusR as well)
void F(const ArrayXd v[], ArrayXd& ret)
{
    // Computes F, does their elementwise maximum, and places it into second half of ret
    auto start2 = std::chrono::system_clock::now();

    // TODO: It may be more efficient to keep index as a collection of strings and define an increment method instead of constantly converting to a string.
    // At the very least, it is definitely more efficient to define the memory for the strings externally instead of reallocating every time
    // This would also greatly reduce the risk of the index overflowing
    double calculated;
    for (int index = 0; index < powminus0; index++)
    {
        calculated = 0.0;
        for (int s = 0; s < alphabet_size; s++)
        {
            calculated = std::max(Fz(s, v, index), calculated);
        }
        // TODO: It is possible to change the outer for loop
        if ((index / F_b_step) % F_b_equals_1 == 0)
        {
            calculated += 1;
        }

        ret[index] = calculated;
    }
    auto end2 = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end2 - start2;
    cout << "Elapsed time F (s): " << elapsed_seconds.count() << endl;
}

// TODO: Original optimization doesn't work, but could make a duplicate F function/subfunctions that manually adds the (d - i - 1) * R
// to avoid allocating an additional d vectors
void F_withplusR(const double R, const ArrayXd& vNew, ArrayXd& ret)
{
    ArrayXd vR[string_count];

    for (int i = 0; i < string_count; i++)
    {
        vR[i] = vNew + (string_count - i - 1) * R;
    }

    F(vR, ret);
}

void FeasibleTriplet(int n)
{
    auto start = std::chrono::system_clock::now();
    ArrayXd v[string_count]; // Vector of previous results, initialized to 0

    for (int i = 0; i < string_count; i++)
    {
        v[i] = ArrayXd::Zero(powminus0); // we pass a pointer to this, and have it get filled in
    }

    ArrayXd vNew(powminus0);

    double r = 0;
    double e = 0;
    for (int i = string_count; i < n + 1; i++)
    {
        cout << "ITERATION " << i - string_count + 1 << endl;
        // Writes new vector (v2) into v2
        auto start2 = std::chrono::system_clock::now();
        F(v, vNew);
        //printArray(vNew);
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start2;
        cout << "Elapsed time F (s): " << elapsed_seconds.count() << endl;

        start2 = std::chrono::system_clock::now();
        const double R = subtract_and_find_max_parallel(v[string_count - 1], vNew);
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start2;
        cout << "Elapsed time (s) mc1: " << elapsed_seconds.count() << endl;

        //  Beyond this point, v[0]'s values are no longer needed, so we reuse
        //  its memory for other computations.
        start2 = std::chrono::system_clock::now();
        F_withplusR(R, vNew, v[0]);
        //printArray(v[0]);
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start2;
        cout << "Elapsed time FpR (s): " << elapsed_seconds.count() << endl;

        start2 = std::chrono::system_clock::now();
        const double E = std::max(subtract_and_find_max_parallel(v[0], vNew) + string_count * R, 0.0);
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start2;
        cout << "Elapsed time mc2 (s): " << elapsed_seconds.count() << endl;

        if (R - E >= r - e)
        {
            r = R;
            e = E;
        }
        cout << "Result (iteration " << i - string_count + 1 << "): " << string_count * (r - e) << endl;
        cout << "Calculated R-E: R = " << R << ", E = " << E << endl;

        // Update v to contain vNew
        std::swap(v[0], vNew); // Replaces junk value in v[0] with vNew
        for (int i = 0; i < string_count - 1; i++)
        { // Shifts vNew to the end of v while moving other vectors up one position in array
            std::swap(v[i], v[i + 1]);
        }
    }

    // return u, r, e
    cout << "Result: " << string_count * (r - e) << endl;
}

int main()
{
    cout << "Starting with l = " << length << ", d = " << string_count << ", sigma = " << alphabet_size << "..." << endl;
    auto start = std::chrono::system_clock::now();
    FeasibleTriplet(100);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    cout << "Elapsed time (s): " << elapsed_seconds.count() << endl;

    return 0;
}