#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include "common/keops_io.h"

namespace pykeops {

using namespace keops;
namespace py = pybind11;

// <__TYPE__, py::array::c_style>  ensures 2 things whatever is the arguments:
//  1) the precision used is __TYPE__ (float or double typically) on the device,
//  2) everything is convert as contiguous before being loaded in memory
// this is maybe not the best in term of performance... but at least it is safe.
using __NUMPYARRAY__ = py::array_t<__TYPE__, py::array::c_style>;

/////////////////////////////////////////////////////////////////////////////////
//                             Utils
/////////////////////////////////////////////////////////////////////////////////

template <>
int get_size(__NUMPYARRAY__ obj_ptri, int l){
    return obj_ptri.shape(l);
}

template <>
__TYPE__* get_data(__NUMPYARRAY__ obj_ptri){
    return (__TYPE__ *) obj_ptri.data();
}

template <>
__INDEX__* get_rangedata(__NUMPYARRAY__ obj_ptri){
    return (__INDEX__ *) obj_ptri.data();
}

template <>
bool is_contiguous(__NUMPYARRAY__ obj_ptri){
    return obj_ptri.c_style;  // always true because of py::array::c_style
}

/////////////////////////////////////////////////////////////////////////////////
//                    Call Cuda functions
/////////////////////////////////////////////////////////////////////////////////


template <>
__NUMPYARRAY__ launch_keops(int tag1D2D, int tagCpuGpu, int tagHostDevice, int Device_Id,
                        int nx, int ny, int nout, int dimout,
                        int tagRanges, int nranges_x, int nranges_y, __INDEX__ **castedranges,
                        __TYPE__ ** castedargs){

    auto result_array = __NUMPYARRAY__({nout,dimout});
    if (tagCpuGpu == 0) {
        if (tagRanges == 0) { // Full M-by-N computation
            CpuReduc(nx, ny, get_data(result_array), castedargs);
        } else if( tagRanges == 1) { // Block sparsity
            CpuReduc_ranges(nx, ny, nranges_x, nranges_y, castedranges, get_data(result_array), castedargs);
        }
    }
    else if (tagCpuGpu == 1) {
#if USE_CUDA
        if (tagHostDevice == 0) {
            if (tagRanges == 0) { // Full M-by-N computation
                if (tag1D2D == 0)
                    GpuReduc1D_FromHost( nx, ny, get_data(result_array), castedargs, Device_Id);
                else if (tag1D2D == 1)
                    GpuReduc2D_FromHost( nx, ny, get_data(result_array), castedargs, Device_Id);
            } else if( tagRanges == 1) { // Block sparsity
                GpuReduc1D_ranges_FromHost(nx, ny, nranges_x, nranges_y, castedranges, get_data(result_array), castedargs, Device_Id);
            }
        } else if (tagHostDevice==1)
            throw std::runtime_error("[KeOps] Gpu computations with Numpy are performed from host data... try to set tagHostDevice to 0.");
#else
        throw std::runtime_error("[KeOps] No cuda device detected... try to set tagCpuGpu to 0.");
#endif
    }

    return result_array;
}



/////////////////////////////////////////////////////////////////////////////////
//                    PyBind11 entry point
/////////////////////////////////////////////////////////////////////////////////


// the following macro force the compiler to change MODULE_NAME to its value
#define VALUE_OF(x) x

#define xstr(s) str(s)
#define str(s) #s

PYBIND11_MODULE(VALUE_OF(MODULE_NAME), m) {
    m.doc() = "This module has been generated by pyKeOps."; // optional module docstring

    m.def("genred_numpy",
          &generic_red<__NUMPYARRAY__>,
          "Entry point to keops - numpy version.");

    m.attr("tagIJ") = TAGIJ;
    m.attr("dimout") = DIMOUT;
    m.attr("formula") = f;
    m.attr("compiled_formula") = xstr(FORMULA_OBJ_STR);
    m.attr("compiled_aliases") = xstr(VAR_ALIASES_STR);
}

}
