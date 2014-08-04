
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace cv;
using namespace std;

namespace jcodec
{
    enum subsampling_t { Y_ONLY = 0, H1V1 = 1, H2V1 = 2, H2V2 = 3 };

    class output_stream
    {
    public:
        virtual ~output_stream() { };
        virtual bool put_buf(const void* Pbuf, int len) = 0;
        template<class T> inline bool put_obj(const T& obj) { return put_buf(&obj, sizeof(T)); }
    };

    struct params
    {
        inline params() : m_quality(85), m_subsampling((subsampling_t)H2V2), m_no_chroma_discrim_flag(false), m_two_pass_flag(false), block_size(16) { }

        inline bool check() const
        {
            if ((m_quality < 1) || (m_quality > 100)) return false;
            if ((uint)m_subsampling > (uint)H2V2) return false;
            return true;
        }

        // Quality: 1-100, higher is better. Typical values are around 50-95.
        int m_quality;
        int block_size;
        // m_subsampling:
        // 0 = Y (grayscale) only
        // 1 = YCbCr, no subsampling (H1V1, YCbCr 1x1x1, 3 blocks per MCU)
        // 2 = YCbCr, H2V1 subsampling (YCbCr 2x1x1, 4 blocks per MCU)
        // 3 = YCbCr, H2V2 subsampling (YCbCr 4x1x1, 6 blocks per MCU-- very common)
        subsampling_t m_subsampling;

        // Disables CbCr discrimination - only intended for testing.
        // If true, the Y quantization table is also used for the CbCr channels.
        bool m_no_chroma_discrim_flag;

        bool m_two_pass_flag;
    };

    class MjpegWriter
    {
    public:
        MjpegWriter();
        int Open(char* outfile, uchar fps, Size ImSize);
        int Write(const Mat &Im);
        int Close();
        bool isOpened();
    private:
        const int NumOfChunks;
        double tencoding;
        FILE *outFile;
        char *outfileName;
        int outformat, outfps, quality;
        int width, height, type, FrameNum;
        int chunkPointer, moviPointer;
        vector<int> FrameOffset, FrameSize, AVIChunkSizeIndex, FrameNumIndexes;
        bool isOpen;

        int toJPGframe(const uchar * data, uint width, uint height, int step, void *& pBuf);
        void StartWriteAVI();
        void WriteStreamHeader();
        void WriteIndex();
        bool WriteFrame(const Mat & Im);
        void WriteODMLIndex();
        void FinishWriteAVI();
        void PutInt(int elem);
        void PutShort(short elem);
        void StartWriteChunk(int fourcc);
        void EndWriteChunk();
    };

    class jpeg_encoder
    {
    public:
        jpeg_encoder();
        ~jpeg_encoder();

        // Initializes the compressor.
        // pStream: The stream object to use for writing compressed data.
        // params - Compression parameters structure, defined above.
        // width, height  - Image dimensions.
        // channels - May be 1, or 3. 1 indicates grayscale, 3 indicates RGB source data.
        // Returns false on out of memory or if a stream write fails.
        bool init(output_stream *pStream, int width, int height, int src_channels, const params &comp_params = params());

        const params &get_params() const { return m_params; }

        // Deinitializes the compressor, freeing any allocated memory. May be called at any time.
        void deinit();

        uint get_total_passes() const { return m_params.m_two_pass_flag ? 2 : 1; }
        inline uint get_cur_pass() { return m_pass_num; }

        // Call this method with each source scanline.
        // width * src_channels bytes per scanline is expected (RGB or Y format).
        // You must call with 0 after all scanlines are processed to finish compression.
        // Returns false on out of memory or if a stream write fails.
        bool process_scanline(const void* pScanline);
        // Writes JPEG image to memory buffer. 
        // On entry, buf_size is the size of the output buffer pointed at by pBuf, which should be at least ~1024 bytes. 
        // If return value is true, buf_size will be set to the size of the compressed data.
        bool compress_image_to_jpeg_file_in_memory(void *&pBuf, int &buf_size, int width, int height, int num_channels, const uchar *pImage_data, const params &comp_params = params());

    private:
        jpeg_encoder(const jpeg_encoder &);
        jpeg_encoder &operator =(const jpeg_encoder &);

        typedef int sample_array_t;

        output_stream *m_pStream;
        params m_params;
        uchar m_num_components;
        uchar m_comp_h_samp[3], m_comp_v_samp[3];
        int m_image_x, m_image_y, m_image_bpp, m_image_bpl;
        int m_image_x_mcu, m_image_y_mcu;
        int m_image_bpl_xlt, m_image_bpl_mcu;
        int m_mcus_per_row;
        int m_mcu_x, m_mcu_y;
        uchar *m_mcu_linesY[16];
        uchar *m_mcu_linesCb[16];
        uchar *m_mcu_linesCr[16];
        uchar m_mcu_y_ofs;
        sample_array_t m_sample_array[64];
        uchar m_sample_array_uchar[64];
        short m_coefficient_array[64];
        int m_quantization_tables[2][64];
        uint m_huff_codes[4][256];
        uchar m_huff_code_sizes[4][256];
        uchar m_huff_bits[4][17];
        uchar m_huff_val[4][256];
        uint m_huff_count[4][256];
        int m_last_dc_val[3];
        enum { JPGE_OUT_BUF_SIZE = 2048 };
        uchar m_out_buf[JPGE_OUT_BUF_SIZE];
        uchar *m_pOut_buf;
        uint m_out_buf_left;
        uint m_bit_buffer;
        uint m_bits_in;
        uchar m_pass_num;
        bool m_all_stream_writes_succeeded;

        bool WriteImage(const uchar* data, int step, int width, int height, int _channels);
        void emit_byte(uchar i);
        void emit_word(uint i);
        void emit_marker(int marker);
        void emit_jfif_app0();
        void emit_dqt();
        void emit_sof();
        void emit_dht(uchar *bits, uchar *val, int index, bool ac_flag);
        void emit_dhts();
        void emit_sos();
        void emit_markers();
        void Put(int val, int bits);
        void compute_huffman_table(uint *codes, uchar *code_sizes, uchar *bits, uchar *val);
        void compute_quant_table(int *dst, short *src);
        void adjust_quant_table(int *dst, int *src);
        void first_pass_init();
        bool second_pass_init();
        bool jpg_open(int p_x_res, int p_y_res, int src_channels);
        void load_block_8_8(int x, int y);
        void load_block_16_8(int x, int comp);
        void DCT2D(int component_num);
        void load_quantized_coefficients(int component_num);
        void flush_output_buffer();
        void put_bits(uint bits, uint len);
        void code_coefficients_pass_one(int component_num);
        void code_coefficients_pass_two(int component_num);
        void code_block(int component_num);
        void process_mcu_row();
        bool terminate_pass_two();
        bool process_end_of_image();
        void load_mcu(const void* src);
        void clear();
        void init();
    };

    //////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

#if _MSC_VER >= 1200
#pragma warning( disable: 4711 4324 )
#endif

#define  RBS_THROW_EOS    -123  /* <end of stream> exception code */
#define  RBS_THROW_FORB   -124  /* <forrbidden huffman code> exception code */
#define  RBS_HUFF_FORB    2047  /* forrbidden huffman code "value" */

    typedef unsigned char uchar;
    typedef unsigned long ulong;


    // WBaseStream - base class for output streams
    class WBaseStream
    {
    public:
        //methods
        WBaseStream();
        virtual ~WBaseStream();

        virtual bool  Open();
        virtual void  Close();
        void          SetBlockSize(int block_size);
        bool          IsOpened();
        int           GetPos();

    protected:

        uchar*  m_start;
        uchar*  m_end;
        uchar*  m_current;
        int     m_block_size;
        int     m_block_pos;
        output_stream *m_stream;
        bool    m_is_opened;

        virtual void  WriteBlock();
        virtual void  Release();
        virtual void  Allocate();
    };


    // class WLByteStream - uchar-oriented stream.
    // l in prefix means that the least significant uchar of a multi-byte value goes first
    class WLByteStream : public WBaseStream
    {
    public:
        virtual ~WLByteStream();

        void    PutByte(int val);
        void    PutBytes(const void* buffer, int count);
        void    PutWord(int val);
        void    PutDWord(int val);
    };


    // class WLByteStream - uchar-oriented stream.
    // m in prefix means that the least significant uchar of a multi-byte value goes last
    class WMByteStream : public WLByteStream
    {
    public:
        virtual ~WMByteStream();

        void    PutWord(int val);
        void    PutDWord(int val);
    };


    // class WLBitStream - bit-oriented stream.
    // l in prefix means that the least significant bit of a multi-bit value goes first
    class WLBitStream : public WBaseStream
    {
    public:
        virtual ~WLBitStream();

        int     GetPos();
        void    Put(int val, int bits);
        void    PutHuff(int val, const int* table);

    protected:
        int     m_bit_idx;
        int     m_val;
        virtual void  WriteBlock();
    };


    // class WMBitStream - bit-oriented stream.
    // l in prefix means that the least significant bit of a multi-bit value goes first
    class WMBitStream : public WBaseStream
    {
    public:
        WMBitStream();
        virtual ~WMBitStream();

        bool    Open();
        void    Close();
        virtual void  Flush();

        int     GetPos();
        void    Put(int val, int bits);
        void    PutHuff(int val, const ulong* table);

    protected:
        int     m_bit_idx;
        ulong   m_pad_val;
        ulong   m_val;
        virtual void  WriteBlock();
        void    ResetBuffer();
    };

    class WJpegBitStream : public WMBitStream
    {
    public:
        WMByteStream  m_low_strm;

        WJpegBitStream();
        ~WJpegBitStream();

        virtual void  Flush();
        virtual bool  Open(output_stream *stream);
        virtual void  Close();

    protected:
        virtual void  WriteBlock();
    };

    ///////////////////////////// base class for writers ////////////////////////////
    class   GrFmtWriter
    {
    public:

        GrFmtWriter();
        virtual ~GrFmtWriter() {};
        virtual bool  WriteImage(const uchar* data, int step,
            int width, int height, int depth, int channels) = 0;
    protected:
        char    m_filename[_MAX_PATH]; // filename
    };

    class GrFmtJpegWriter : public GrFmtWriter
    {
    public:

        GrFmtJpegWriter(const char* filename);
        ~GrFmtJpegWriter();

        bool  WriteImage(const uchar* data, int step,
            int width, int height, int depth, int channels);

        WJpegBitStream  m_strm;
    };

#define BSWAP(v)    (((v)<<24)|(((v)&0xff00)<<8)| \
    (((v) >> 8) & 0xff00) | ((unsigned)(v) >> 24))

    int* bsCreateSourceHuffmanTable(const uchar* src, int* dst,
        int max_bits, int first_bits);
    bool bsCreateDecodeHuffmanTable(const int* src, short* dst, int max_size);
    bool bsCreateEncodeHuffmanTable(const int* src, ulong* dst, int max_size);

    void bsBSwapBlock(uchar *start, uchar *end);
    bool bsIsBigEndian(void);

    extern const ulong bs_bit_mask[];

}