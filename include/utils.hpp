#ifndef _MGARD_UTILS_HPP
#define _MGARD_UTILS_HPP

#include <vector>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <quantizer/Quantizer.hpp>
#include <encoder/HuffmanEncoder.hpp>
#include "zstd.h"

namespace MGARD{

using namespace std;

#define ZSTD_COMPRESSOR 1
unsigned long sz_lossless_compress(int losslessCompressor, int level, unsigned char* data, unsigned long dataLength, unsigned char** compressBytes)
{
    unsigned long outSize = 0; 
    size_t estimatedCompressedSize = 0;
    switch(losslessCompressor)
    {
    case ZSTD_COMPRESSOR:
        if(dataLength < 100) 
            estimatedCompressedSize = 200;
        else
            estimatedCompressedSize = dataLength*1.2;
        *compressBytes = (unsigned char*)malloc(estimatedCompressedSize);
        *reinterpret_cast<size_t*>(*compressBytes) = dataLength;
        outSize = ZSTD_compress(*compressBytes + sizeof(size_t), estimatedCompressedSize, data, dataLength, level); //default setting of level is 3
        break;
    default:
        printf("Error: Unrecognized lossless compressor in sz_lossless_compress()\n");
    }
    return outSize + sizeof(size_t);
}
unsigned long sz_lossless_decompress(int losslessCompressor, const unsigned char* compressBytes, unsigned long cmpSize, unsigned char** oriData)
{
    unsigned long outSize = 0;
    switch(losslessCompressor)
    {
    case ZSTD_COMPRESSOR:
        outSize = *reinterpret_cast<const size_t*>(compressBytes);
        *oriData = (unsigned char*)malloc(outSize);
        ZSTD_decompress(*oriData, outSize, compressBytes + sizeof(size_t), cmpSize - sizeof(size_t));
        break;
    default:
        printf("Error: Unrecognized lossless compressor in sz_lossless_decompress()\n");
    }
    return outSize;
}
template<typename Type>
std::vector<Type> readfile(const char *file, size_t &num) {
    std::ifstream fin(file, std::ios::binary);
    if (!fin) {
        std::cout << " Error, Couldn't find the file" << "\n";
        return std::vector<Type>();
    }
    fin.seekg(0, std::ios::end);
    const size_t num_elements = fin.tellg() / sizeof(Type);
    fin.seekg(0, std::ios::beg);
    auto data = std::vector<Type>(num_elements);
    fin.read(reinterpret_cast<char *>(&data[0]), num_elements * sizeof(Type));
    fin.close();
    num = num_elements;
    return data;
}
template<typename Type>
void writefile(const char *file, Type *data, size_t num_elements) {
    std::ofstream fout(file, std::ios::binary);
    fout.write(reinterpret_cast<const char *>(&data[0]), num_elements * sizeof(Type));
    fout.close();
}
template <class T>
void print(T * data, size_t n1, size_t n2, string s){
    cout << "Print data: " << s << endl;
    for(int i=0; i<n1; i++){
        for(int j=0; j<n2; j++){
            cout << data[i * n2 + j] << " ";
        }
        cout << endl;
    }
    cout << endl;
}
// switch the rows in the data for coherent memory access
/*
@params data_pos: starting position of data
@params data_buffer: buffer to store intermediate data
@params n1, n2: dimensions
@params stride: stride for the non-continguous dimension
Illustration:
o: nodal data, x: coefficient data
    oooxx       oooxx
    xxxxx       oooxx
    oooxx   =>  oooxx
    xxxxx       xxxxx
    oooxx       xxxxx
*/
template <class T>
void switch_rows_2D_by_buffer(T * data_pos, T * data_buffer, size_t n1, size_t n2, size_t stride){
    size_t n1_nodal = (n1 >> 1) + 1;
    size_t n1_coeff = n1 - n1_nodal;
    T * nodal_data_buffer = data_buffer + n2; // skip the first nodal row
    T * coeff_data_buffer = data_buffer + n1_nodal * n2;
    T * cur_data_pos = data_pos + stride;
    for(int i=0; i<n1_coeff; i++){
        // copy coefficient rows
        memcpy(coeff_data_buffer + i * n2, cur_data_pos, n2 * sizeof(T));
        cur_data_pos += stride;
        // copy nodal rows
        memcpy(nodal_data_buffer + i * n2, cur_data_pos, n2 * sizeof(T));
        cur_data_pos += stride;
    }
    if(!(n1&1)){
        // n1 is even, move the last nodal row
        memcpy(coeff_data_buffer - n2, cur_data_pos, n2 * sizeof(T));
    }
    // copy data back
    cur_data_pos = data_pos + stride;
    for(int i=1; i<n1; i++){
        memcpy(cur_data_pos, data_buffer + i * n2, n2 * sizeof(T));
        cur_data_pos += stride;
    }
}
// inverse operation for switch_rows_2D_by_buffer
/*
@params data_pos: starting position of data
@params data_buffer: buffer to store intermediate data
@params n1, n2: dimensions
@params stride: stride for the non-continguous dimension
Illustration:
o: nodal data, x: coefficient data
    oooxx       oooxx
    oooxx       xxxxx
    oooxx   =>  oooxx
    xxxxx       xxxxx
    xxxxx       oooxx
*/
template <class T>
void switch_rows_2D_by_buffer_reverse(T * data_pos, T * data_buffer, size_t n1, size_t n2, size_t stride){
    size_t n1_nodal = (n1 >> 1) + 1;
    size_t n1_coeff = n1 - n1_nodal;
    T * nodal_data_buffer = data_pos + stride; // skip the first nodal row
    T * coeff_data_buffer = data_pos + n1_nodal * stride;
    T * cur_data_pos = data_buffer + n2;
    for(int i=0; i<n1_coeff; i++){
        // copy coefficient rows
        memcpy(cur_data_pos, coeff_data_buffer + i * stride, n2 * sizeof(T));
        cur_data_pos += n2;
        // copy nodal rows
        memcpy(cur_data_pos, nodal_data_buffer + i * stride, n2 * sizeof(T));
        cur_data_pos += n2;
    }
    if(!(n1&1)){
        // n1 is even, move the last nodal row
        memcpy(cur_data_pos, coeff_data_buffer - stride, n2 * sizeof(T));
    }
    // copy data back
    cur_data_pos = data_pos + stride;
    for(int i=1; i<n1; i++){
        memcpy(cur_data_pos, data_buffer + i * n2, n2 * sizeof(T));
        cur_data_pos += stride;
    }
}
const double alpha = 1.0/12;
const double beta = 0.5;
const double gamma = 5.0/6;
// compute entries for load vector in nodal rows
// for uniform decomposition only
/*
@params load_v_buffer: buffer to store the output load vector
@params n_nodal, n_coeff: number of nodal values and coeff values
@params h: interval length
@params coeff_buffer: starting position of coefficients
Given nodal and adjacent coeff (o    x    o   x      o)
                                0    c[0] 0   c[1]   0
according to derivation, load_v[n[1]] = (c[0] * 1/2 + c[1] * 1/2) * h
*/
template <class T>
void  compute_load_vector_nodal_row(T * load_v_buffer, size_t n_nodal, size_t n_coeff, T h, const T * coeff_buffer){
    T const * coeff = coeff_buffer;
    // T ah = h * 0.5; // derived constant in the formula
    // eliminate h for efficiency
    T ah = beta; 
    // first nodal value
    load_v_buffer[0] = coeff[0] * ah;
    // iterate through nodal values
    for(int i=1; i<n_coeff; i++){
        load_v_buffer[i] = (coeff[i-1] + coeff[i]) * ah;
    }
    // last nodal value
    load_v_buffer[n_coeff] = coeff[n_coeff-1] * ah;
    // if next n is even, load_v_buffer[n_nodal - 1] = 0
    if(n_nodal == n_coeff + 2) load_v_buffer[n_coeff + 1] = 0;
}

// compute entries for load vector in coeff rows
// for uniform decomposition only
/*
@params load_v_buffer: buffer to store the output load vector
@params n_nodal, n_coeff: number of nodal values and coeff values
@params h: interval length
@params coeff_buffer: starting position of nodal values
@params coeff_buffer: starting position of coefficients
Given nodal coeff and adjacent coeff (x    x    x      x      x)
                                     n[0] c[0] n[1]   c[1]   n[2]
according to derivation, 
load_v[n[1]] = (n[0] * 1/12 + c[0] * 1/2 + n[1] * 5/6 + c[1] * 1/2 + n[2] * 1/12) * h
*/
template <class T>
void  compute_load_vector_coeff_row(T * load_v_buffer, size_t n_nodal, size_t n_coeff, T h, const T * nodal_buffer, const T * coeff_buffer){
    T const * coeff = coeff_buffer;
    T const * nodal = nodal_buffer;
    // T ah = h * 0.5; // derived constant in the formula
    // eliminate h for efficiency
    T ah = alpha;   // 1/12
    T bh = beta;    // 1/2
    T ch = gamma;   // 5/6
    // first nodal value
    load_v_buffer[0] = nodal[0] * ch / 2 + coeff[0] * bh + nodal[1] * ah;
    // iterate through nodal values
    for(int i=1; i<n_coeff; i++){
        load_v_buffer[i] = (nodal[i - 1] + nodal[i + 1]) * ah + (coeff[i - 1] + coeff[i]) * bh + nodal[i] * ch;
    }
    // last nodal value
    load_v_buffer[n_coeff] = nodal[n_coeff - 1] * ah + coeff[n_coeff - 1] * bh + nodal[n_coeff] * ch / 2;
    // if next n is even, load_v_buffer[n_nodal - 1] = 0
    if(n_nodal == n_coeff + 2) load_v_buffer[n_coeff + 1] = 0;
}
// compute correction on nodal value for 1D case 
// using Thomas algorithm for tridiagonal inverse
/*
@params correction_buffer: buffer to store the output correction
@params n_nodal: number of nodal values
@params h: interval length
@params load_v_buffer: computed load vector in previous step, will be modified during computation
*/
template <class T>
void compute_correction(T * correction_buffer, size_t n_nodal, T h, T * load_v_buffer){
    size_t n = n_nodal;
    // Thomas algorithm for solving M_l x = load_v
    // forward pass
    // simplified algorithm
    T * d = load_v_buffer;
    // vector<T> b(n, h*4/3);
    // b[0] = h*2/3;
    // b[n-1] = h*2/3;
    // T c = h/3;
    // eliminate h for efficiency
    vector<T> b(n, 4.0/3);
    b[0] = 2.0/3;
    b[n-1] = 2.0/3;
    T c = 1.0/3;
    for(int i=1; i<n; i++){
        auto w = c / b[i-1];
        b[i] = b[i] - w * c;
        d[i] = d[i] - w * d[i-1];
    }
    // backward pass
    correction_buffer[n-1] = d[n-1] / b[n-1];
    for(int i=n-2; i>=0; i--){
        correction_buffer[i] = (d[i] - c * correction_buffer[i+1]) / b[i];
    }
}
// compute entries for load vector in vertical (non-contiguous) direction
// for uniform decomposition only
/*
@params load_v_buffer: buffer to store the output load vector
@params nodal_buffer: starting position of nodal values
@params coeff_buffer: starting position of coefficients
@params n1_nodal, n1_coeff: number of nodal values and coeffcicients in non-continguous dimension
@params stride: stride for adjacent data in non-continguous dimension
@params h: interval length
@params batchsize: number of columns to be computed together
*/
template <class T>
void compute_load_vector_vertical(T * load_v_buffer, const T * nodal_buffer, const T * coeff_buffer, size_t n1_nodal, size_t n1_coeff, size_t stride, T h, int batchsize){
    // T ah = h * 0.25; // derived constant in the formula
    T ah = alpha * h;   // 1/12
    T bh = beta * h;    // 1/2
    T ch = gamma * h;   // 5/6
    T const * nodal_pos = nodal_buffer;
    T const * coeff_pos = coeff_buffer;
    T * load_v_pos = load_v_buffer;
    // first nodal value
    for(int j=0; j<batchsize; j++){
        // load_v_pos[j] = coeff_pos[j] * ah;
        load_v_pos[j] = nodal_pos[j] * ch / 2 + coeff_pos[j] * bh + nodal_pos[stride + j] * ah;
    }
    load_v_pos += batchsize;
    nodal_pos += stride;
    coeff_pos += stride;
    for(int i=1; i<n1_coeff; i++){
        for(int j=0; j<batchsize; j++){
            // load_v_pos[j] = (coeff_pos[-stride + j] + coeff_pos[j]) * ah;
            load_v_pos[j] = (nodal_pos[j - stride] + nodal_pos[j + stride]) * ah + (coeff_pos[j - stride] + coeff_pos[j]) * bh + nodal_pos[j] * ch;
        }
        load_v_pos += batchsize;
        nodal_pos += stride;
        coeff_pos += stride;
    }
    // last nodal value
    for(int j=0; j<batchsize; j++){
        // load_v_pos[j] = coeff_pos[-stride + j] * ah;
        load_v_pos[j] = nodal_pos[j - stride] * ah + coeff_pos[j - stride] * bh + nodal_pos[j] * ch / 2;
    }
    // if next n is even, load_v_buffer[n_nodal - 1] = 0
    if(n1_nodal == n1_coeff + 2){
        load_v_pos += batchsize;
        for(int j=0; j<batchsize; j++){
            load_v_pos[j] = 0;
        }
    }
}
// compute correction on nodal value for 1D case
// using Thomas algorithm for tridiagonal inverse
/*
@params correction_buffer: buffer to store the output correction
@params h: interval length
@params b, w: pre-computed auxilliary array
@params n_nodal: number of nodal values
@params batchsize: number of columns to be computed together
@params correction_stride: stride for adjacent correction data in non-continguous dimension
@params load_v_buffer: buffer to store load vector
*/
template <class T>
void compute_correction_batched(T * correction_buffer, T h, const T * b, const T * w, size_t n_nodal, int batchsize, size_t correction_stride, T * load_v_buffer){
    size_t n = n_nodal;
    T c = h/3;
    // Thomas algorithm for solving M_l x = load_v
    // forward pass
    // simplified algorithm
    // b[:], w[:] are precomputed
    T * load_v_pos = load_v_buffer + batchsize;
    for(int i=1; i<n; i++){
        for(int j=0; j<batchsize; j++){
            load_v_pos[j] -= w[i] * load_v_pos[-batchsize + j];
        }
        load_v_pos += batchsize;
    }
    // backward pass
    T * correction_pos = correction_buffer + (n - 1) * correction_stride;
    load_v_pos -= batchsize;
    for(int j=0; j<batchsize; j++){
        correction_pos[j] = load_v_pos[j] / b[n - 1];
    }
    correction_pos -= correction_stride;
    load_v_pos -= batchsize;
    for(int i=n-2; i>=0; i--){
        for(int j=0; j<batchsize; j++){
            correction_pos[j] = (load_v_pos[j] - c * correction_pos[correction_stride + j]) / b[i];
        }
        correction_pos -= correction_stride;
        load_v_pos -= batchsize;
    }
}
// apply correction back to the nodal values
/*
@params nodal_pos: starting position of nodal values
@params correction_buffer: buffer that stores the corresponding correction
@params n_nodal: number of points along the nodal dimension
@params stride: stride across adjacent nodal values
@params batchsize: number of points to be computed in a batch
@params decompose: whether this function is called during decompose or not
*/
template <class T>
void apply_correction_batched(T * nodal_pos, const T * correction_buffer, int n_nodal, int stride, int batchsize, bool decompose){
    const T * correction_pos = correction_buffer;
    if(decompose){
        for(int i=0; i<n_nodal; i++){
            for(int j=0; j<batchsize; j++){
                nodal_pos[j] += correction_pos[j];
            }
            nodal_pos += stride;
            correction_pos += batchsize;
        }
    }
    else{
        for(int i=0; i<n_nodal; i++){
            for(int j=0; j<batchsize; j++){
                nodal_pos[j] -= correction_pos[j];
            }
            nodal_pos += stride;
            correction_pos += batchsize;
        }
    }
}
// compute correction the vertical (non-contiguous) dimension
/*
@params data_pos: starting position of data
@params n1: number of points in vertical dimension
@params n2: number of points in contiguous dimension
@params h: interval length
@params horizontal_corrections: starting position of computed horizontal correction (last sweep)
@params stride: stride across adjacent vertical points in horizontal_corrections
@params load_v_buffer: buffer to store load vectors. Contents of buffer will be modified
@params default_batch_size: batchsize of vertical correction computation
*/
template <class T>
void compute_correction_vertical(T * data_pos, size_t n1, size_t n2, T h, T * horizontal_correction, size_t stride, T * load_v_buffer, int default_batch_size=1){
    size_t n1_nodal = (n1 >> 1) + 1;
    size_t n1_coeff = n1 - n1_nodal;
    size_t n2_nodal = (n2 >> 1) + 1;
    size_t n2_coeff = n2 - n2_nodal;
    vector<T> b(n1_nodal, h*4/3);
    vector<T> w(n1_nodal, 0);
    b[0] = h*2/3, b[n1_nodal - 1] = h*2/3;
    T c = h/3;
    // pre-compute w and b
    for(int i=1; i<n1_nodal; i++){
        w[i] = c / b[i-1];
        b[i] = b[i] - w[i] * c;
    }
    int batchsize = default_batch_size;
    int num_batches = (n2_nodal - 1) / batchsize;
    T * nodal_pos = horizontal_correction;
    T * coeff_pos = horizontal_correction + n1_nodal * stride;
    // compute vertical correction
    T * data_nodal_pos = data_pos;
    for(int i=0; i<num_batches; i++){
        compute_load_vector_vertical(load_v_buffer, nodal_pos, coeff_pos, n1_nodal, n1_coeff, stride, h, batchsize);
        compute_correction_batched(nodal_pos, h, b.data(), w.data(), n1_nodal, batchsize, stride, load_v_buffer);
        nodal_pos += batchsize, coeff_pos += batchsize, data_nodal_pos += batchsize;
    }
    if(n2_nodal - batchsize * num_batches > 0){
        batchsize = n2_nodal - batchsize * num_batches;
        compute_load_vector_vertical(load_v_buffer, nodal_pos, coeff_pos, n1_nodal, n1_coeff, stride, h, batchsize);
        compute_correction_batched(nodal_pos, h, b.data(), w.data(), n1_nodal, batchsize, stride, load_v_buffer);
    }
}
// compute the corrections for 2D cases
/*
@params data_pos: starting position of data
@params correction_buffer: buffer to store the output correction
@params load_v_buffer: computed load vector in previous step, will be modified during computation
@params n1, n2: dimensions
@params nodal_rows: number of nodal_rows (0 for coefficient plane)
@params h: interval length
@params stride: stride for adjacent data in non-continguous dimension
@params default_batch_size: batchsize of vertical correction computation
*/
template <class T>
void compute_correction_2D(T * data_pos, T * correction_buffer, T * load_v_buffer, size_t n1, size_t n2, size_t nodal_rows, T h, size_t stride, int default_batch_size=1){
    size_t n1_nodal = (n1 >> 1) + 1;
    size_t n1_coeff = n1 - n1_nodal;
    size_t n2_nodal = (n2 >> 1) + 1;
    size_t n2_coeff = n2 - n2_nodal;
    // compute horizontal correction
    T * nodal_pos = data_pos;
    const T * coeff_pos = data_pos + n2_nodal;
    // store horizontal corrections in the data_buffer
    T * correction_pos = correction_buffer;
    for(int i=0; i<n1; i++){
        if(i < nodal_rows) compute_load_vector_nodal_row(load_v_buffer, n2_nodal, n2_coeff, h, coeff_pos);
        else  compute_load_vector_coeff_row(load_v_buffer, n2_nodal, n2_coeff, h, nodal_pos, coeff_pos);
        compute_correction(correction_pos, n2_nodal, h, load_v_buffer);
        // subtract_correction(n2_nodal, nodal_pos);
        nodal_pos += stride, coeff_pos += stride;
        correction_pos += n2_nodal;
    }
    // compute vertical correction
    compute_correction_vertical(data_pos, n1, n2, h, correction_buffer, n2_nodal, load_v_buffer, default_batch_size);
}
// compute the corrections for 3D cases
/*
@params data_pos: starting position of data
@params correction_buffer: buffer to store the output correction
@params load_v_buffer: computed load vector in previous step, will be modified during computation
@params n1, n2, n3: dimensions
@params nodal_rows: number of nodal planes (0 for coefficient cube)
@params h: interval length
@params dim0_stride, dim1_stride: stride for adjacent data in non-continguous dimension
@params default_batch_size: batchsize of vertical correction computation
*/
template <class T>
void compute_correction_3D(T * data_pos, T * correction_buffer, T * load_v_buffer, size_t n1, size_t n2, size_t n3, size_t nodal_rows, T h, size_t dim0_stride, size_t dim1_stride, int default_batch_size=1){
    size_t n1_nodal = (n1 >> 1) + 1;
    size_t n1_coeff = n1 - n1_nodal;
    size_t n2_nodal = (n2 >> 1) + 1;
    size_t n2_coeff = n2 - n2_nodal;
    size_t n3_nodal = (n3 >> 1) + 1;
    size_t n3_coeff = n3 - n3_nodal;
    // compute 2D corrections
    T * nodal_pos = data_pos;
    // store 2D corrections in the correction_buffer
    T * correction_pos = correction_buffer;
    for(int i=0; i<n1; i++){
        size_t nodal_rows = (i < n1_nodal) ? n2_nodal : 0;
        compute_correction_2D(nodal_pos, correction_pos, load_v_buffer, n2, n3, nodal_rows, h, dim1_stride, default_batch_size);
        nodal_pos += dim0_stride;
        correction_pos += n2_nodal * n3_nodal;
    }        
    // compute vertical correction
    correction_pos = correction_buffer;
    nodal_pos = data_pos;
    for(int i=0; i<n2_nodal; i++){
        compute_correction_vertical(data_pos, n1, n3, h, correction_pos, n2_nodal * n3_nodal, load_v_buffer, default_batch_size);
        nodal_pos += dim1_stride;
        correction_pos += n3_nodal;
    }
}

}
#endif