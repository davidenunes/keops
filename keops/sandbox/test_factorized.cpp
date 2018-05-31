// test convolution using factorized formula
// compile with
//		g++ -I.. -D__TYPE__=float -std=c++11 -O2 -o build/test_factorized test_factorized.cpp

// we define an arbitrary function F,
// then use a factorized version FF of the same function and test
// 

#include <stdio.h>
#include <assert.h>
#include <vector>
#include <ctime>
#include <algorithm>
#include <iostream>

#include "core/formulas/constants.h"
#include "core/formulas/maths.h"
#include "core/formulas/kernels.h"
#include "core/formulas/norms.h"
#include "core/formulas/factorize.h"

#include "core/autodiff.h"

#include "core/CpuConv.cpp"

using namespace std::cout;
using namespace std::endl;



__TYPE__ floatrand() {
    return ((__TYPE__) std::rand())/RAND_MAX-.5;    // random value between -.5 and .5
}

template < class V > void fillrandom(V& v) {
    generate(v.begin(), v.end(), floatrand);    // fills vector with random values
}

int main() {


    // symbolic variables of the function
    using X = Var<1,3,0>; 	// X is the first variable and represents a 3D vector
    using Y = Var<2,3,1>; 	// Y is the second variable and represents a 3D vector
    using B = Var<3,3,1>;	// B is the fifth variable and represents a 3D vector
    using U = Var<4,3,0>;
    using V = Var<5,3,1>;
    using C = Param<0,1>;		// C is the first extra parameter

    // symbolic expression of the function : 3rd order gradient with respect to X, X and Y of the Gauss kernel
    using F = Grad<Grad<Grad<GaussKernel<C,X,Y,B>,X,U>,X,U>,Y,V>;

    cout << endl << "Function F : " << endl;
    PrintFormula<F>();
    cout << endl << endl;

    using FF = AutoFactorize<F>;

    cout << "Function FF = factorized version of F :" << endl;    
    PrintFormula<FF>();

    using FUNCONVF = typename Generic<F>::sEval;
    using FUNCONVFF = typename Generic<FF>::sEval;

    // now we test ------------------------------------------------------------------------------

    cout << endl << endl << "Testing F" << endl;

    int Nx=500, Ny=200;

    vector<__TYPE__> vf(Nx*F::DIM);    fillrandom(vf); __TYPE__ *f = vf.data();
    vector<__TYPE__> vx(Nx*X::DIM);    fillrandom(vx); __TYPE__ *x = vx.data();
    vector<__TYPE__> vy(Ny*Y::DIM);    fillrandom(vy); __TYPE__ *y = vy.data();
    vector<__TYPE__> vu(Nx*U::DIM);    fillrandom(vu); __TYPE__ *u = vu.data();
    vector<__TYPE__> vv(Ny*V::DIM);    fillrandom(vv); __TYPE__ *v = vv.data();
    vector<__TYPE__> vb(Ny*B::DIM); fillrandom(vb); __TYPE__ *b = vb.data();

    vector<__TYPE__> rescpu1(Nx*F::DIM), rescpu2(Nx*F::DIM);

    __TYPE__ params[1];
    __TYPE__ Sigma = 1;
    params[0] = 1.0/(Sigma*Sigma);

    clock_t begin, end;

    begin = clock();
    CpuConv(FUNCONVF(), Nx, Ny, f, params, x, y, b, u, v);
    end = clock();
    cout << "time for CPU computation : " << double(end - begin) / CLOCKS_PER_SEC << endl;

    rescpu1 = vf;



    /// testing FF

    cout << endl << endl << "Testing FF" << endl;

    begin = clock();
    CpuConv(FUNCONVFF(), Nx, Ny, f, params, x, y, b, u, v);
    end = clock();
    cout << "time for CPU computation : " << double(end - begin) / CLOCKS_PER_SEC << endl;

    rescpu2 = vf;

    // display values
    cout << "rescpu1 = ";
    for(int i=0; i<5; i++)
        cout << rescpu1[i] << " ";
    cout << endl << "rescpu2 = ";
    for(int i=0; i<5; i++)
        cout << rescpu2[i] << " ";

    // display mean of errors
    __TYPE__ s = 0;
    for(int i=0; i<Nx*F::DIM; i++)
        s += abs(rescpu1[i]-rescpu2[i]);
    cout << endl << "mean abs error =" << s/Nx << endl;

}



