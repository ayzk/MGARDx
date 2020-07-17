#ifndef _REFACTOR_DATA_ENC_OPT_HPP
#define _REFACTOR_DATA_ENC_OPT_HPP

#include <vector>
#include <cstdlib>

namespace REFACTOR{

using namespace std;

// index for switching to rle
#define RLE_2_INDEX_F32 25

class EncoderInterface{
public:
    virtual void encode(bool) = 0;
    virtual void flush() = 0;
    virtual size_t size() = 0;
    virtual unsigned char * save() = 0;
};

class DecoderInterface{
public:
    virtual ~DecoderInterface() = default;
    virtual bool decode() = 0;
    virtual void load(const unsigned char *) = 0;
};
// A class to read data bit by bit
// Currently does not consider the case when decoding size is larger than capacity
class BitDecoder : public DecoderInterface{
public:
    BitDecoder() = default;
    ~BitDecoder() = default;
    bool decode(){
        if (!(buffer >> 1u)) buffer = 0x100u + *current++;
        bool bit = buffer & 1u;
        buffer >>= 1u;
        return bit;
    }
    size_t size(){ return (buffer == 1u) ? current - start : current - start + 1; }
    void load(const unsigned char * encoded_data){
        start = encoded_data;
        current = start;
        buffer = 1u;
    }
private:
    unsigned char const * start = NULL;
    unsigned char const * current = NULL;
    unsigned int buffer = 1u;
};
/*******************************************/

// Runlength encoder
#define RLE_CUTOFF_COUNT 512
class RunlengthEncoder : public EncoderInterface{
public:
    RunlengthEncoder(){}
    void encode(bool bit){
        if(lastbit == bit){
            count ++;
            if(count == RLE_CUTOFF_COUNT){
                length.push_back(count);
                count = 0;
                lastbit = !bit;
            }
        }
        else {
            length.push_back(count);
            count = 1;
            lastbit = bit;
        }
    }
    void flush(){
        if(count != 0){
            length.push_back(count);
            count = 0;
            lastbit = false;
        }
    }
    size_t size(){
        return encoded_size;
    }
    unsigned char * save(){
        // Huffman
        unsigned char * encoded = (unsigned char *) malloc(length.size() * sizeof(int));
        auto encoded_pos = encoded;
        *reinterpret_cast<size_t*>(encoded_pos) = length.size();
        encoded_pos += sizeof(size_t);
        auto encoder = SZ::HuffmanEncoder<int>();
        encoder.preprocess_encode(length, 2*RLE_CUTOFF_COUNT);
        encoder.save(encoded_pos);
        encoder.encode(length, encoded_pos);
        encoder.postprocess_encode();
        encoded_size = encoded_pos - encoded;
        // return encoded;
        unsigned char * lossless_encoded = lossless_encode(encoded, encoded_size);
        free(encoded);
        return lossless_encoded;
    }
private:
    unsigned char * lossless_encode(unsigned char * encoded, size_t& encoded_size){
        unsigned char * lossless_compressed = NULL;
        size_t lossless_length = MGARD::sz_lossless_compress(ZSTD_COMPRESSOR, 3, encoded, encoded_size, &lossless_compressed);
        encoded_size = lossless_length + sizeof(size_t);
        unsigned char * lossless_compressed_with_size = (unsigned char *) malloc(encoded_size);
        *reinterpret_cast<size_t*>(lossless_compressed_with_size) = lossless_length;
        memcpy(lossless_compressed_with_size + sizeof(size_t), lossless_compressed, lossless_length);
        free(lossless_compressed);
        return lossless_compressed_with_size;
    }
    vector<int> length;
    bool lastbit = false;
    int count = 0;
    int index = 0;
    size_t encoded_size = 0;
};

class RunlengthDecoder : public DecoderInterface{
public:
    RunlengthDecoder() = default;
    ~RunlengthDecoder() = default;
    bool decode(){
        if(count){
            count --;
            return lastbit;
        }
        else{
            count = length[index ++];
            lastbit = !lastbit;
            return decode();
        }
    }
    // void load(const unsigned char * encoded){
    void load(const unsigned char * lossless_encoded){
        auto encoded = lossless_decode(lossless_encoded);
        const unsigned char * encoded_pos = encoded;
        size_t n = *reinterpret_cast<const size_t*>(encoded_pos);
        encoded_pos += sizeof(size_t);
        auto encoder = SZ::HuffmanEncoder<int>();
        size_t remaining_length = INT_MAX;
        encoder.load(encoded_pos, remaining_length);
        length = encoder.decode(encoded_pos, n);
        encoder.postprocess_decode();
        free(encoded);
        count = 0;
        index = 0;
        // toggle lastbit to true such that the first decode would be false since count=0
        lastbit = true;
    }
private:
    unsigned char * lossless_decode(const unsigned char * lossless_encoded){
        unsigned char * encoded = NULL;
        size_t lossless_length = *reinterpret_cast<const size_t*>(lossless_encoded);
        size_t compressed_length = MGARD::sz_lossless_decompress(ZSTD_COMPRESSOR, lossless_encoded + sizeof(size_t), lossless_length, &encoded);
        return encoded;
    }
    vector<int> length;
    bool lastbit = false;
    int count = 0;
    int index = 0;
};

// encode bitplanes by byte
/*
@params data: coefficient data
@params n: number of coefficients in current level
@params level_exp: exponent of max level element
@params num_level_component: number of encoded bitplanes
@params byte_encoders: vector of byte-wise encoder
*/
template <class T>
size_t byte_wise_direct_encoding(const T * data, int n, int level_exp, int num_level_component, vector<unsigned char *>& byte_encoders){
    size_t data_index = 0;
    size_t buffer_index = 0;
    for(int i=0; i<n/8; i++){
        unsigned char tmp[32] = {0};
        for(int j=0; j<8; j++){
            T val = data[data_index ++];
            T cur_data = ldexp(val, num_level_component - 1 - level_exp);
            long int fix_point = (long int) cur_data;
            unsigned int sign = val < 0;
            unsigned int fp = sign ? -fix_point : +fix_point;
            tmp[0] += sign << j;
            for(int k=num_level_component - 2; k>=0; k--){
                tmp[num_level_component - 1 - k] += ((fp >> k) & 1) << j;
            }
        }
        for(int k=0; k<num_level_component; k++){
            byte_encoders[k][buffer_index] = tmp[k];
        }
        buffer_index ++;
    }
    {
        int rest = n % 8;
        unsigned char tmp[32] = {0};
        for(int j=0; j<rest; j++){
            T val = data[data_index ++];
            T cur_data = ldexp(val, num_level_component - 1 - level_exp);
            long int fix_point = (long int) cur_data;
            unsigned int sign = val < 0;
            unsigned int fp = sign ? -fix_point : +fix_point;
            tmp[0] += sign << j;
            for(int k=num_level_component - 2; k>=0; k--){
                tmp[num_level_component - 1 - k] += ((fp >> k) & 1) << j;
            }
        }
        for(int k=0; k<num_level_component; k++){
            byte_encoders[k][buffer_index] = tmp[k];
        }
        buffer_index ++;
    }
    return buffer_index;
}

// encode bitplanes by byte (unrolled version)
/*
@params data: coefficient data
@params n: number of coefficients in current level
@params level_exp: exponent of max level element
@params num_level_component: number of encoded bitplanes
@params byte_encoders: vector of byte-wise encoder
*/
template <class T>
size_t byte_wise_direct_encoding_unrolled(const T * data, int n, int level_exp, int num_level_component, vector<unsigned char *>& byte_encoders){
    size_t data_index = 0;
    size_t buffer_index = 0;
    for(int i=0; i<n/8; i++){
        unsigned char tmp = 0;
        T cur_data;
        long int fix_point;
        bool sign;
        cur_data = ldexp(data[data_index ++], num_level_component - 1 - level_exp);
        fix_point = (long int) cur_data;
        sign = cur_data < 0;
        tmp += sign;
        unsigned int fp0 = sign ? -fix_point : +fix_point;
        cur_data = ldexp(data[data_index ++], num_level_component - 1 - level_exp);
        fix_point = (long int) cur_data;
        sign = cur_data < 0;
        tmp += sign << 1;
        unsigned int fp1 = sign ? -fix_point : +fix_point;
        cur_data = ldexp(data[data_index ++], num_level_component - 1 - level_exp);
        fix_point = (long int) cur_data;
        sign = cur_data < 0;
        tmp += sign << 2;
        unsigned int fp2 = sign ? -fix_point : +fix_point;
        cur_data = ldexp(data[data_index ++], num_level_component - 1 - level_exp);
        fix_point = (long int) cur_data;
        sign = cur_data < 0;
        tmp += sign << 3;
        unsigned int fp3 = sign ? -fix_point : +fix_point;
        cur_data = ldexp(data[data_index ++], num_level_component - 1 - level_exp);
        fix_point = (long int) cur_data;
        sign = cur_data < 0;
        tmp += sign << 4;
        unsigned int fp4 = sign ? -fix_point : +fix_point;
        cur_data = ldexp(data[data_index ++], num_level_component - 1 - level_exp);
        fix_point = (long int) cur_data;
        sign = cur_data < 0;
        tmp += sign << 5;
        unsigned int fp5 = sign ? -fix_point : +fix_point;
        cur_data = ldexp(data[data_index ++], num_level_component - 1 - level_exp);
        fix_point = (long int) cur_data;
        sign = cur_data < 0;
        tmp += sign << 6;
        unsigned int fp6 = sign ? -fix_point : +fix_point;
        cur_data = ldexp(data[data_index ++], num_level_component - 1 - level_exp);
        fix_point = (long int) cur_data;
        sign = cur_data < 0;
        tmp += sign << 7;
        unsigned int fp7 = sign ? -fix_point : +fix_point;
        byte_encoders[0][buffer_index] = tmp;
        for(int k=num_level_component - 2; k>=0; k--){
            byte_encoders[num_level_component - 1 - k][buffer_index] 
                =   ((fp0 >> k) & 1) + (((fp1 >> k) & 1) << 1) 
                    + (((fp2 >> k) & 1) << 2) + (((fp3 >> k) & 1) << 3)
                    + (((fp4 >> k) & 1) << 4) + (((fp5 >> k) & 1) << 5)
                    + (((fp6 >> k) & 1) << 6) + (((fp7 >> k) & 1) << 7);
            // tmp = 0;
            // tmp += (fp0 >> k) & 1;
            // tmp += ((fp1 >> k) & 1) << 1;
            // tmp += ((fp2 >> k) & 1) << 2;
            // tmp += ((fp3 >> k) & 1) << 3;
            // tmp += ((fp4 >> k) & 1) << 4;
            // tmp += ((fp5 >> k) & 1) << 5;
            // tmp += ((fp6 >> k) & 1) << 6;
            // tmp += ((fp7 >> k) & 1) << 7;
            // byte_encoders[num_level_component - 1 - k][buffer_index] = tmp;
        }
        // unsigned char tmp_[32] = {0};
        // for(int k=30; k>=0; k--){
        //     tmp_[31 - k] += (fp0 >> k) & 1;
        // }
        // for(int k=30; k>=0; k--){
        //     tmp_[31 - k] += ((fp1 >> k) & 1) << 1;
        // }
        // for(int k=30; k>=0; k--){
        //     tmp_[31 - k] += ((fp2 >> k) & 1) << 2;
        // }
        // for(int k=30; k>=0; k--){
        //     tmp_[31 - k] += ((fp3 >> k) & 1) << 3;
        // }
        // for(int k=30; k>=0; k--){
        //     tmp_[31 - k] += ((fp4 >> k) & 1) << 4;
        // }
        // for(int k=30; k>=0; k--){
        //     tmp_[31 - k] += ((fp5 >> k) & 1) << 5;
        // }
        // for(int k=30; k>=0; k--){
        //     tmp_[31 - k] += ((fp6 >> k) & 1) << 6;
        // }
        // for(int k=30; k>=0; k--){
        //     tmp_[31 - k] += ((fp7 >> k) & 1) << 7;
        // }
        // for(int k=1; k<32; k++){
        //     byte_encoders[k][buffer_index] = tmp_[k];
        // }
        buffer_index ++;
    }
    {
        // leftover
        int rest = n % 8;
        unsigned char tmp[32] = {0};
        for(int j=0; j<rest; j++){
            T val = data[data_index ++];
            T cur_data = ldexp(val, num_level_component - 1 - level_exp);
            long int fix_point = (long int) cur_data;
            unsigned int sign = val < 0;
            unsigned int fp = sign ? -fix_point : +fix_point;
            tmp[0] += sign << j;
            for(int k=num_level_component - 2; k>=0; k--){
                tmp[num_level_component - 1 - k] += ((fp >> k) & 1) << j;
            }
        }
        for(int k=0; k<num_level_component; k++){
            byte_encoders[k][buffer_index] = tmp[k];
        }
        buffer_index ++;
    }
    return buffer_index;
}

template <class T>
T * direct_decoding(const vector<const unsigned char*>& level_components, size_t n, int level_exp, int num_level_component){
    T * level_data = (T *) malloc(n * sizeof(T));
    size_t level_component_size = (n * sizeof(T) - 1) / num_level_component + 1 + 8;
    vector<BitDecoder> decoders;
    for(int i=0; i<num_level_component; i++){
        decoders.push_back(BitDecoder());
        decoders[i].load(level_components[i]);
    }
    T * data_pos = level_data;
    for(int i=0; i<n; i++){
        // decode each bit of the data for each level component
        bool sign = decoders[0].decode();
        unsigned int fp = 0;
        for(int j=1; j<num_level_component; j++){
            unsigned int current_bit = decoders[j].decode();
            fp = (fp << 1) + current_bit;
        }
        long int fix_point = fp;
        if(sign) fix_point = -fix_point;
        *data_pos = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        data_pos ++;
    }
    return level_data;
}

template <class T>
T * byte_wise_direct_decoding(const vector<const unsigned char*>& level_components, int n, int level_exp, int num_level_component){
    T * level_data = (T *) malloc(n * sizeof(T));
    size_t level_component_size = (n * sizeof(T) - 1) / num_level_component + 1 + 8;
    cout << "level element = " << n << endl;
    cout << "num_level_component = " << num_level_component << endl;
    cout << "level_component_size = " << level_component_size << endl;
    size_t buffer_index = 0;
    T * data_pos = level_data;
    for(int i=0; i<n/8; i++){
        unsigned int fp0 = 0, fp1 = 0, fp2 = 0, fp3 = 0, fp4 = 0, fp5 = 0, fp6 = 0, fp7 = 0;
        for(int j=1; j<num_level_component; j++){
            unsigned char cur_byte = level_components[j][buffer_index];
            fp0 = (fp0 << 1) + (cur_byte & 1);
            fp1 = (fp1 << 1) + ((cur_byte >> 1) & 1);
            fp2 = (fp2 << 1) + ((cur_byte >> 2) & 1);
            fp3 = (fp3 << 1) + ((cur_byte >> 3) & 1);
            fp4 = (fp4 << 1) + ((cur_byte >> 4) & 1);
            fp5 = (fp5 << 1) + ((cur_byte >> 5) & 1);
            fp6 = (fp6 << 1) + ((cur_byte >> 6) & 1);
            fp7 = (fp7 << 1) + ((cur_byte >> 7) & 1);
            // this is slower
            // fp0 += (cur_byte & 1) << (num_level_component - 1 - j);
            // fp1 += ((cur_byte >> 1) & 1) << (num_level_component - 1 - j);
            // fp2 += ((cur_byte >> 2) & 1) << (num_level_component - 1 - j);
            // fp3 += ((cur_byte >> 3) & 1) << (num_level_component - 1 - j);
            // fp4 += ((cur_byte >> 4) & 1) << (num_level_component - 1 - j);
            // fp5 += ((cur_byte >> 5) & 1) << (num_level_component - 1 - j);
            // fp6 += ((cur_byte >> 6) & 1) << (num_level_component - 1 - j);
            // fp7 += ((cur_byte >> 7) & 1) << (num_level_component - 1 - j);
        }
        unsigned char sign = level_components[0][buffer_index];
        signed int fix_point;
        fix_point = fp0;
        fix_point = (sign & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp1;
        fix_point = ((sign >> 1) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp2;
        fix_point = ((sign >> 2) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp3;
        fix_point = ((sign >> 3) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp4;
        fix_point = ((sign >> 4) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp5;
        fix_point = ((sign >> 5) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp6;
        fix_point = ((sign >> 6) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp7;
        fix_point = ((sign >> 7) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        buffer_index ++;
    }
    {
        // leftover
        int rest = n % 8;
        unsigned int fp[8] = {0};
        for(int j=1; j<num_level_component; j++){
            unsigned char cur_byte = level_components[j][buffer_index];
            for(int r=0; r<rest; r++){
                fp[r] = (fp[r] << 1) + ((cur_byte >> r) & 1);
            }
        }
        unsigned char sign = level_components[0][buffer_index];
        for(int r=0; r<rest; r++){
            signed int fix_point = fp[r];
            fix_point = ((sign >> r) & 1) ? -fix_point : fix_point;
            *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        }
        buffer_index ++;
    }
    return level_data;
}

template <class T>
T * direct_hybrid_decoding(const vector<const unsigned char*>& level_components, size_t n, int level_exp, int num_level_component, const vector<unsigned char>& bitplane_indicator){
    T * level_data = (T *) malloc(n * sizeof(T));
    cout << "level element = " << n << endl;
    cout << "num_level_component = " << num_level_component << endl;
    vector<DecoderInterface*> decoders;
    for(int i=0; i<num_level_component; i++){
        switch(bitplane_indicator[i]){
            case 0:{
                decoders.push_back(new BitDecoder());
                break;
            }
            case 1:{
                decoders.push_back(new RunlengthDecoder());
                break;
            }
            default:{
                cerr << "Only direct encoding (indicator = 0) and RLE (indicator = 1) are supported\n";
                exit(0);
            }
        }
        decoders[i]->load(level_components[i]);
    }
    T * data_pos = level_data;
    for(int i=0; i<n; i++){
        // decode each bit of the data for each level component
        bool sign = decoders[0]->decode();
        unsigned int fp = 0;
        for(int j=1; j<num_level_component; j++){
            unsigned int current_bit = decoders[j]->decode();
            fp = (fp << 1) + current_bit;
        }
        long int fix_point = fp;
        if(sign) fix_point = -fix_point;
        *data_pos = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        data_pos ++;
    }
    for(int i=0; i<num_level_component; i++){
        delete decoders[i];
    }
    return level_data;
}

template <class T>
T * byte_wise_hybrid_decoding(const vector<const unsigned char*>& level_components, size_t n, int level_exp, int num_level_component, const vector<unsigned char>& bitplane_indicator){
    T * level_data = (T *) malloc(n * sizeof(T));
    int rle_switch_index = 0;   // index for which the encoder switches from rle to direct
    for(int i=1; i<bitplane_indicator.size(); i++){
        if(bitplane_indicator[i] == 0){
            rle_switch_index = i;
            break;
        }
    }
    if(rle_switch_index > num_level_component) rle_switch_index = num_level_component;
    const int direct_switch_index = (num_level_component > RLE_2_INDEX_F32) ? RLE_2_INDEX_F32 : num_level_component;   // index for which the encoder switches from direct to rle
    // decoders for rle in bp 1~switch
    vector<RunlengthDecoder*> pre_decoders;
    for(int i=1; i<rle_switch_index; i++){
        pre_decoders.push_back(new RunlengthDecoder());
        pre_decoders.back()->load(level_components[i]);
    }
    // decoders for rle after bp RLE_2_INDEX_F32
    vector<RunlengthDecoder*> suf_decoders;
    for(int i=RLE_2_INDEX_F32; i<num_level_component; i++){
        suf_decoders.push_back(new RunlengthDecoder());
        suf_decoders.back()->load(level_components[i]);
    }
    size_t buffer_index = 0;
    T * data_pos = level_data;
    for(int i=0; i<n/8; i++){
        unsigned int fp0 = 0, fp1 = 0, fp2 = 0, fp3 = 0, fp4 = 0, fp5 = 0, fp6 = 0, fp7 = 0;
        for(int j=1; j<rle_switch_index; j++){
            fp0 = (fp0 << 1) + pre_decoders[j-1]->decode();
            fp1 = (fp1 << 1) + pre_decoders[j-1]->decode();
            fp2 = (fp2 << 1) + pre_decoders[j-1]->decode();
            fp3 = (fp3 << 1) + pre_decoders[j-1]->decode();
            fp4 = (fp4 << 1) + pre_decoders[j-1]->decode();
            fp5 = (fp5 << 1) + pre_decoders[j-1]->decode();
            fp6 = (fp6 << 1) + pre_decoders[j-1]->decode();
            fp7 = (fp7 << 1) + pre_decoders[j-1]->decode();
        }
        for(int j=rle_switch_index; j<direct_switch_index; j++){
            unsigned char cur_byte = level_components[j][buffer_index];
            fp0 = (fp0 << 1) + (cur_byte & 1);
            fp1 = (fp1 << 1) + ((cur_byte >> 1) & 1);
            fp2 = (fp2 << 1) + ((cur_byte >> 2) & 1);
            fp3 = (fp3 << 1) + ((cur_byte >> 3) & 1);
            fp4 = (fp4 << 1) + ((cur_byte >> 4) & 1);
            fp5 = (fp5 << 1) + ((cur_byte >> 5) & 1);
            fp6 = (fp6 << 1) + ((cur_byte >> 6) & 1);
            fp7 = (fp7 << 1) + ((cur_byte >> 7) & 1);            
        }
        for(int j=RLE_2_INDEX_F32; j<num_level_component; j++){
            fp0 = (fp0 << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
            fp1 = (fp1 << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
            fp2 = (fp2 << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
            fp3 = (fp3 << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
            fp4 = (fp4 << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
            fp5 = (fp5 << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
            fp6 = (fp6 << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
            fp7 = (fp7 << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
        }
        unsigned char sign = level_components[0][buffer_index];
        signed int fix_point;
        fix_point = fp0;
        fix_point = (sign & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp1;
        fix_point = ((sign >> 1) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp2;
        fix_point = ((sign >> 2) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp3;
        fix_point = ((sign >> 3) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp4;
        fix_point = ((sign >> 4) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp5;
        fix_point = ((sign >> 5) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp6;
        fix_point = ((sign >> 6) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        fix_point = fp7;
        fix_point = ((sign >> 7) & 1) ? -fix_point : fix_point;
        *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        buffer_index ++;
    }
    {
        // leftover
        int rest = n % 8;
        unsigned int fp[8] = {0};
        for(int j=1; j<rle_switch_index; j++){
            for(int r=0; r<rest; r++){
                fp[r] = (fp[r] << 1) + pre_decoders[j-1]->decode();
            }
        }        
        for(int j=rle_switch_index; j<direct_switch_index; j++){
            unsigned char cur_byte = level_components[j][buffer_index];
            for(int r=0; r<rest; r++){
                fp[r] = (fp[r] << 1) + ((cur_byte >> r) & 1);
            }
        }
        for(int j=direct_switch_index; j<num_level_component; j++){
            for(int r=0; r<rest; r++){
                fp[r] = (fp[r] << 1) + suf_decoders[j-RLE_2_INDEX_F32]->decode();
            }
        }
        unsigned char sign = level_components[0][buffer_index];
        for(int r=0; r<rest; r++){
            signed int fix_point = fp[r];
            fix_point = ((sign >> r) & 1) ? -fix_point : fix_point;
            *(data_pos ++) = ldexp((float)fix_point, - num_level_component + 1 + level_exp);
        }
        buffer_index ++;        
    }
    for(int i=0; i<pre_decoders.size(); i++){
        delete pre_decoders[i];
    }
    for(int i=0; i<suf_decoders.size(); i++){
        delete suf_decoders[i];
    }
    return level_data;
}

}
#endif
