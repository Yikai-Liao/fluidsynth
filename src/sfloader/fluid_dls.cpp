// for <Windows.h> max macro, std::numeric_limits::max
#define NOMINMAX
// for mingw-w64 <stdio.h> %ll, see
// https://sourceforge.net/p/mingw-w64/mailman/mingw-w64-public/thread/20120411101049.GA4263%40glandium.org/#msg29128250
// the issue is that formatter-width specifier %ll isn't supported for all msvcrt-DLL versions,
// therefore gcc warns about its use. The variant for specifying 64-bit integer-scalar-width in
// formatter for msvcrt in a backward-compatible way is by using %I64. Use %I64u on Windows, or just
// use inttypes.h PRIuMAX. If you must use %llu, define __USE_MINGW_ANSI_STDIO macro before
// including stdio.h. Be aware that if you do this, MS type %I64* format will no longer work.
#define __USE_MINGW_ANSI_STDIO 1

#include "fluid_dls.h"

#include "fluid_sys.h"
#include "fluid_sfont.h"
#include "fluidsynth_priv.h"
#include "fluid_defsfont.h"
#include "fluid_mod.h"
#include "fluid_synth.h"
#include "fluid_chan.h"

#if LIBSNDFILE_SUPPORT
#include <sndfile.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <exception>
#include <optional>
#include <utility>
#include <functional>
#include <limits>
#include <string>
#include <array>

using std::int16_t;
using std::int32_t;
using std::int8_t;
using std::size_t;
using std::uint16_t;
using std::uint32_t;
using std::uint8_t;

// based of https://stackoverflow.com/a/26221725
// original license: CC0 1.0
template<size_t N, typename... Args>
static inline std::string string_format(const char (&format)[N], Args... args)
{
    int size_s = std::snprintf(nullptr, 0, format, args...);

    if(size_s < 0)
    {
        throw std::runtime_error("Error while formatting a message (dry)");
    }

    auto size = static_cast<size_t>(size_s);
    std::string result;
    result.resize(size);
    size_s = std::snprintf(result.data(), size + 1, format, args...); // snprintf writes \0 into buffer

    if(size_s < 0)
    {
        throw std::runtime_error("Error while formatting a message");
    }

    return result;
}

static inline void log_exception(int level, const std::exception &exc, int nest_level = 0)
{
    const auto &exc_type = typeid(exc);
    std::string prefix;

    if(nest_level > 0)
    {
        prefix = std::string(1 * nest_level, ' ');
    }

    if(exc_type == typeid(std::bad_alloc))
    {
        FLUID_LOG(FLUID_PANIC, "%sOut of memory!!: %s", prefix.c_str(), exc.what());
    }
    else
    {
        FLUID_LOG(level, "%sexc: %s", prefix.c_str(), exc.what());
    }

    try
    {
        std::rethrow_if_nested(exc);
    }
    catch(const std::exception &nested)
    {
        if(nest_level + 1 >= 16)
        {
            FLUID_LOG(level, "%s...", prefix.c_str());
            return;
        }

        log_exception(level, nested, nest_level + 1);
    }
}

// format specifier and arguments used for printing FOURCCs
#define FMT_4CC_SPEC "%c%c%c%c"
#define FMT_4CC_ARG(x)                                                                    \
    (reinterpret_cast<const char *>(&(x)))[0], (reinterpret_cast<const char *>(&(x)))[1], \
    (reinterpret_cast<const char *>(&(x)))[2], (reinterpret_cast<const char *>(&(x)))[3]

// The single authority for callback I/O and the physical file position. All
// parsing, including libsndfile, goes through this source after initialization.
class fluid_dls_input
{
    const fluid_file_callbacks_t &callbacks;
    void *file;
    fluid_long_long_t position = -1;

public:
    fluid_dls_input(const fluid_file_callbacks_t &callbacks, void *file)
        : callbacks(callbacks), file(file) {}

    fluid_dls_input(const fluid_dls_input &) = delete;
    fluid_dls_input &operator=(const fluid_dls_input &) = delete;

    void read_at(fluid_long_long_t offset, void *dest, fluid_long_long_t count)
    {
        if(position != offset)
        {
            position = -1;
            if(callbacks.fseek(file, offset, SEEK_SET) != FLUID_OK)
            {
                throw std::runtime_error{ "Failed to seek DLS data" };
            }
        }
        // A failed callback may have partially advanced the physical cursor.
        position = -1;
        if(callbacks.fread(dest, count, file) != FLUID_OK)
        {
            throw std::runtime_error{ "Failed to read DLS data" };
        }
        position = offset + count;
    }
};

// A copy is an independent cursor over the same bounded file range. Nested
// parsers never depend on the physical cursor left by another reader.
class fluid_dls_reader
{
    fluid_dls_input *input;
    fluid_long_long_t begin;
    fluid_long_long_t length;
    fluid_long_long_t position{};

public:
    fluid_dls_reader(fluid_dls_input &input, fluid_long_long_t begin, fluid_long_long_t length)
        : input(&input), begin(begin), length(length)
    {
        if(begin < 0 || length < 0 || begin > std::numeric_limits<fluid_long_long_t>::max() - length)
        {
            throw std::runtime_error{ "Invalid DLS reader range" };
        }
    }

    fluid_long_long_t size() const { return length; }
    fluid_long_long_t tell() const { return position; }
    fluid_long_long_t offset() const { return begin + position; }
    fluid_long_long_t remaining() const { return length - position; }

    void seek(fluid_long_long_t pos)
    {
        if(pos < 0 || pos > length)
        {
            throw std::runtime_error{ "DLS seek exceeds chunk boundary" };
        }
        position = pos;
    }

    void skip(fluid_long_long_t count)
    {
        if(count < 0 || count > remaining())
        {
            throw std::runtime_error{ "DLS skip exceeds chunk boundary" };
        }
        position += count;
    }

    fluid_dls_reader take(fluid_long_long_t count)
    {
        const auto start = offset();
        skip(count);
        return fluid_dls_reader(*input, start, count);
    }

    void read(void *dest, fluid_long_long_t count)
    {
        if(count < 0 || count > remaining())
        {
            throw std::runtime_error{ "DLS read exceeds chunk boundary" };
        }
        if(count == 0)
        {
            return;
        }
        input->read_at(offset(), dest, count);
        position += count;
    }

    uint8_t read_u8() { uint8_t value; read(&value, 1); return value; }
    uint16_t read_u16() { uint16_t value; read(&value, 2); return FLUID_LE16TOH(value); }
    uint32_t read_u32() { uint32_t value; read(&value, 4); return FLUID_LE32TOH(value); }
    uint32_t read_fourcc() { uint32_t value; read(&value, 4); return value; }
};

// Produced only after the complete chunk (including padding) fits its parent.
// Retaining the encoded range also gives libsndfile a bounded virtual WAV.
struct fluid_dls_chunk
{
    uint32_t id;
    int headersize;
    fluid_long_long_t offset;
    fluid_dls_reader encoded;
    fluid_dls_reader data;
};

// RAII wrapper for scope defer execution
template<class Callable> struct scope_guard
{
private:
    Callable on_exit;

public:
    explicit scope_guard(Callable on_exit) noexcept : on_exit(std::move(on_exit))
    {
    }

    ~scope_guard() noexcept(noexcept(on_exit()))
    // clang-format off
    // There seems to be a bug in GCC 8.1.0 when the definition of on_exit is put after the exception-spec.
    // > error: there are no arguments to 'on_exit' that depend on a template parameter, so a declaration of 'on_exit' must be available.
    // But on_exit should lookup to the member definition wherever the definition is put in the template definition.
    // see [temp.dep] 4 https://eel.is/c++draft/temp.dep#type-4
    // It compiles fine with GCC 15.2.1.
    // clang-format on
    {
        on_exit();
    }
};

// RAII wrapper for mlock
struct mlock_guard
{
    mlock_guard() noexcept : ptr(nullptr), size(0), locked(false)
    {
    }

    mlock_guard(void *ptr, fluid_long_long_t size) noexcept : ptr(ptr), size(size), locked(false)
    {
    }

    mlock_guard(const mlock_guard &) = delete;
    mlock_guard &operator=(const mlock_guard &) = delete;
    mlock_guard(mlock_guard &&other) noexcept
        : ptr(std::exchange(other.ptr, nullptr)), size(std::exchange(other.size, 0)),
          locked(std::exchange(other.locked, false))
    {
    }

    mlock_guard &operator=(mlock_guard &&other) noexcept
    {
        if(this == &other)
        {
            return *this; // self-assignment
        }

        unlock();
        ptr = std::exchange(other.ptr, nullptr);
        size = std::exchange(other.size, 0);
        locked = std::exchange(other.locked, false);
        return *this;
    }

    int lock() noexcept
    {
        if(ptr == nullptr || locked)
        {
            return 0;
        }

        int result = fluid_mlock(ptr, size);

        if(result == 0)
        {
            locked = true;
        }

        return result;
    }

    void unlock() noexcept
    {
        if(ptr == nullptr || !locked)
        {
            return;
        }

        fluid_munlock(ptr, size);
        locked = false;
    }

    ~mlock_guard() noexcept
    {
        unlock();
    }

private:
    void *ptr;
    fluid_long_long_t size;
    bool locked;
};

// fluid_sfloader_t interface
static fluid_sfont_t *fluid_dls_loader_load(fluid_sfloader_t *loader, const char *filename) noexcept;
static void fluid_dls_loader_delete(fluid_sfloader_t *loader) noexcept;

// fluid_sfont_t interface
static const char *fluid_dls_sfont_get_name(fluid_sfont_t *sfont) noexcept;
static fluid_preset_t *fluid_dls_sfont_get_preset(fluid_sfont_t *sfont, int bank, int prenum) noexcept;
static void fluid_dls_iteration_start(fluid_sfont_t *sfont) noexcept;
static fluid_preset_t *fluid_dls_iteration_next(fluid_sfont_t *sfont) noexcept;
static int fluid_dls_sfont_delete(fluid_sfont_t *sfont) noexcept;

// fluid_preset_t interface
static const char *fluid_dls_preset_get_name(fluid_preset_t *preset) noexcept;
static int fluid_dls_preset_get_banknum(fluid_preset_t *preset) noexcept;
static int fluid_dls_preset_get_num(fluid_preset_t *preset) noexcept;
static int fluid_dls_preset_noteon(fluid_preset_t *preset, fluid_synth_t *synth, int chan, int key, int vel) noexcept;
static void fluid_dls_preset_free(fluid_preset_t *preset) noexcept;

// internal struct for keeping some nice information of the DLS

// note that it is at the level of lart (one fluid_dls_articulation per instrument (region))
struct fluid_dls_articulation
{
    std::optional<fluid_real_t> gens[GEN_LAST];
    std::vector<fluid_mod_t> mods;

    // Scale of the Key Num Generator, "MIDI Note to Key", divided by 12800.
    // note that this is only valid at instrument level, as it affects region selection process.
    fluid_real_t keynum_scale = 1.0;

    // clang-format off
    fluid_mod_t keyToPitch
    {
        GEN_FINETUNE,
        FLUID_MOD_KEY,
        FLUID_MOD_GC | FLUID_MOD_LINEAR | FLUID_MOD_UNIPOLAR | FLUID_MOD_POSITIVE,
        0,
        0,
        FLUID_MOD_TRANSFORM_LINEAR,
        12800,
        nullptr,
        nullptr,
        nullptr
    }; // DLS articulator Key Number to Pitch (default 12800 cents)
    // clang-format on

    fluid_dls_articulation()
    {
        // overriding default gens

        gens[GEN_MODLFOFREQ] = -851.3179423647571;  // 5 Hz
        gens[GEN_MODLFODELAY] = -7972.627427729669; // 10 ms
        gens[GEN_VIBLFOFREQ] = -851.3179423647571;  // 5 Hz
        gens[GEN_VIBLFODELAY] = -7972.627427729669; // 10 ms
    }

    void add_mod(const fluid_mod_t &mod)
    {
        if(fluid_mod_test_identity(&keyToPitch, &mod))
        {
            keyToPitch.amount = mod.amount;
            return;
        }

        for(auto &existing_mod : mods)
        {
            if(fluid_mod_test_identity(&existing_mod, &mod))
            {
                existing_mod.amount = mod.amount;
                return;
            }
        }

        mods.push_back(mod);
    }
};

// wsmp is used in LIST[wave] chunk and LIST[rgn ] chunk, converting to different fluid structures
struct fluid_dls_wsmp
{
    uint16_t unity_note;
    int16_t fine_tune; // in cents
    int32_t gain;      // in (cB/65536)
    uint32_t loop_type;
    uint32_t loop_start;
    uint32_t loop_length;
};

struct fluid_dls_sample
{
    // N.B. wlnk is at region level in DLS, but sample level in SF2 (FluidSynth)
    // as wave link is not implemented now, it is simply ignored.

    std::string name;
    unsigned samplerate;
    unsigned start;
    unsigned end; // past the end
    std::optional<fluid_dls_wsmp> wsmp;
};

struct fluid_dls_region
{
    fluid_zone_range_t range{};
    std::optional<fluid_dls_wsmp> wsmp;
    int exclusive_class{};

    uint32_t artindex = -1;
    uint32_t sampleindex{};
    unsigned char samplemode_inherited{}; // loop_type from sample's wsmp, converted to SF2 sample mode
    fluid_real_t gain_inherited{};        // gain from sample's wsmp, in cB
};

// SF2's instrument level does not exist in DLS, preset is called instrument in DLS
struct fluid_dls_instrument
{
    int pcnum;
    int bankmsb;
    int banklsb;
    bool is_drums{};

    std::string name;
    std::vector<fluid_dls_region> regions;

    fluid_sample_t *samples_fluid;
    fluid_dls_articulation *articulations;
    fluid_synth_t *synth;
    uint8_t *drum_note_aliasing;

    // see fluid_dls_articulation::keynum_scale
    fluid_real_t keynum_scale = 1.0;

    struct fluid_dls_instrument_alias
    {
        int pcnum;
        int bankmsb;
        int banklsb;
    };

    // for 'pgal' chunk in MobileBAE DLS banks
    std::vector<fluid_dls_instrument_alias> aliases;
};

struct fluid_dls_instrument_fluid_data
{
    int pcnum;
    int bankmsb;
    int banklsb;
    bool is_drums;

    fluid_preset_t fluid{};           // its user data is `this` (fluid_dls_instrument_fluid_data*)
    fluid_dls_instrument *instrument; // backing instrument
};

struct DLSID;

struct fluid_dls_font
{
    fluid_synth_t *synth;
    fluid_sfont_t *sfont;
    const fluid_file_callbacks_t fcbs;
    uint32_t output_sample_rate; // only used for cdl eval
    std::string filename;

    void *file{};
    // this RAII wrapper is used to provide exception safety (initializer may throws!)
    scope_guard<std::function<void()>> on_file_exit{ [this]()   // Once `this` is captured, it must not be copied or moved
        {
            if(file != nullptr)
            {
                fcbs.fclose(file);
                file = nullptr;
            }
        } };
    // this MUST NOT be modified after initialization, because of probable mlock
    std::vector<int16_t> sampledata;
    mlock_guard sampledata_mlock;
    std::vector<uint32_t> poolcues; // data of ptbl

    std::vector<fluid_dls_sample> samples;
    // this MUST NOT be modified after initialization, because of instrument.articulations pointer
    std::vector<fluid_dls_articulation> articulations;
    // this MUST NOT be modified after initialization, because of fluid_dls_instrument_fluid_data points to this
    std::vector<fluid_dls_instrument> instruments;
    // this MUST NOT be modified after initialization, because of self-reference
    std::vector<fluid_dls_instrument_fluid_data> instruments_fluid_data;
    // this MUST NOT be modified after initialization, because of instrument.samples_fluid pointer
    std::vector<fluid_sample_t> samples_fluid;

    // for 'pgal' chunk in MobileBAE DLS banks
    std::optional<std::array<uint8_t, 128>> drum_note_aliasing;

    decltype(instruments_fluid_data)::iterator fluid_preset_iterator;

    fluid_long_long_t total_presets{}; // total number of presets, including aliases

    // ---

    // The constructor throws exceptions, so exception safety must be guaranteed
    inline fluid_dls_font(fluid_synth_t *synth,
                          fluid_sfont_t *sfont,
                          const fluid_file_callbacks_t *fcbs,
                          const char *filename,
                          uint32_t output_sample_rate,
                          bool try_mlock);

    fluid_dls_font(const fluid_dls_font &) = delete;
    fluid_dls_font &operator=(const fluid_dls_font &) = delete;
    fluid_dls_font(fluid_dls_font &&) = delete;
    fluid_dls_font &operator=(fluid_dls_font &&) noexcept = delete;

    ~fluid_dls_font() = default;

    // parsing functions

    // visitor(fluid_dls_chunk chunk)
    template<class Visitor>
    inline std::enable_if_t<std::is_invocable_v<Visitor, fluid_dls_chunk>, fluid_dls_chunk>
    visit_subchunks(fluid_dls_chunk chunk, uint32_t expected_4cc, Visitor &&visitor);

    // cdl
    inline std::optional<uint32_t> eval_dlsid_query(const DLSID &dlsid);
    inline bool execute_cdls(fluid_dls_reader reader);

    // wave
    inline int16_t *append_sample_frames(fluid_dls_sample &sample, fluid_long_long_t frames);
    inline void parse_wvpl(fluid_dls_chunk chunk);
    inline void parse_wave(fluid_dls_chunk chunk, fluid_dls_sample &sample);
    inline uint32_t parse_wsmp(fluid_dls_reader reader, fluid_dls_wsmp &wsmp);

#if LIBSNDFILE_SUPPORT
    inline void parse_wave_sndfile(fluid_dls_chunk chunk, fluid_dls_sample &sample);
#endif

    // lins
    inline void parse_lins(fluid_dls_chunk chunk);
    inline void parse_ins(fluid_dls_chunk chunk, fluid_dls_instrument &instrument);
    inline bool parse_lart(fluid_dls_chunk chunk, fluid_dls_articulation &articulation);
    inline void parse_art(fluid_dls_chunk chunk, fluid_dls_articulation &articulation);
    inline void parse_lrgn(fluid_dls_chunk chunk, fluid_dls_instrument &instrument);
    inline bool parse_rgn(fluid_dls_chunk chunk, fluid_dls_region &region);

    // pgal
    // see PGAL_FCC
    inline void parse_pgal(fluid_dls_reader reader);

    // info
    inline std::string read_name_from_info_entries(fluid_dls_reader reader);
};

// helper function to create a new fluid_dls_font
template<class... Args>
static std::enable_if_t<std::is_constructible_v<fluid_dls_font, Args...>, fluid_dls_font *> // definitely there should be a requires clause
new_fluid_dls_font(Args &&...args) noexcept
{
    try
    {
        return new fluid_dls_font(std::forward<Args>(args)...);
    }
    catch(const std::bad_alloc &)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory when allocing fluid_dls_font");
        return nullptr;
    }
    catch(const std::exception &exc)
    {
        FLUID_LOG(FLUID_ERR, "Exception thrown in fluid_dls_font constructor");
        log_exception(FLUID_ERR, exc);
        return nullptr;
    }
}

static void delete_fluid_dls_font(fluid_dls_font *dlsfont) noexcept
{
    if(dlsfont == nullptr)
    {
        return;
    }

    try
    {
        delete dlsfont;
    }
    catch(const std::exception &err)
    {
        FLUID_LOG(FLUID_ERR, "Exception thrown in fluid_dls_font destructor");
        log_exception(FLUID_ERR, err);
    }
}

// Basic DLS structures

// non-LIST chunks
#define RIFF_FCC FLUID_FOURCC('R', 'I', 'F', 'F')
#define LIST_FCC FLUID_FOURCC('L', 'I', 'S', 'T')
#define DLID_FCC FLUID_FOURCC('d', 'l', 'i', 'd')
#define CDL_FCC FLUID_FOURCC('c', 'd', 'l', ' ')
#define PTBL_FCC FLUID_FOURCC('p', 't', 'b', 'l')
#define VERS_FCC FLUID_FOURCC('v', 'e', 'r', 's')
#define COLH_FCC FLUID_FOURCC('c', 'o', 'l', 'h')
#define INSH_FCC FLUID_FOURCC('i', 'n', 's', 'h')
#define WSMP_FCC FLUID_FOURCC('w', 's', 'm', 'p')
#define FMT_FCC FLUID_FOURCC('f', 'm', 't', ' ')
#define DATA_FCC FLUID_FOURCC('d', 'a', 't', 'a')
#define ART1_FCC FLUID_FOURCC('a', 'r', 't', '1')
#define ART2_FCC FLUID_FOURCC('a', 'r', 't', '2')
#define RGNH_FCC FLUID_FOURCC('r', 'g', 'n', 'h')
#define WLNK_FCC FLUID_FOURCC('w', 'l', 'n', 'k')

// LIST chunks
#define INFO_FCC FLUID_FOURCC('I', 'N', 'F', 'O')
#define LINS_FCC FLUID_FOURCC('l', 'i', 'n', 's')
#define WVPL_FCC FLUID_FOURCC('w', 'v', 'p', 'l')
#define INS_FCC FLUID_FOURCC('i', 'n', 's', ' ')
#define WAVE_FCC FLUID_FOURCC('w', 'a', 'v', 'e')
#define LART_FCC FLUID_FOURCC('l', 'a', 'r', 't')
#define LRGN_FCC FLUID_FOURCC('l', 'r', 'g', 'n')
#define LAR2_FCC FLUID_FOURCC('l', 'a', 'r', '2')
#define RGN_FCC FLUID_FOURCC('r', 'g', 'n', ' ')
#define RGN2_FCC FLUID_FOURCC('r', 'g', 'n', '2')

#define DLS_FCC FLUID_FOURCC('D', 'L', 'S', ' ')

// not required but recognized
#define FACT_FCC FLUID_FOURCC('f', 'a', 'c', 't')
#define CUE_FCC FLUID_FOURCC('c', 'u', 'e', ' ')

// this is seen as another 'dlid' in LIST[wave]
#define GUID_FCC FLUID_FOURCC('g', 'u', 'i', 'd')

// The 'crs1' chunk is seen in Crystal's DLS (The CrysDLS, see https://www.vogons.org/viewtopic.php?f=62&t=70307)
// It has wrong size and need manual fix
// See https://github.com/spessasus/spessasynth_core/issues/5 and https://github.com/FluidSynth/fluidsynth/pull/1626
#define CRS1_FCC FLUID_FOURCC('c', 'r', 's', '1')

// The 'crs2' chunk is also seen in Crystal's DLS.
// But it has correct size (0 bytes!)
#define CRS2_FCC FLUID_FOURCC('c', 'r', 's', '2')

// MobileBAE proprietary chunk for instrument aliasing
// https://lpcwiki.miraheze.org/wiki/MobileBAE#Proprietary_instrument_aliasing_chunk
#define PGAL_FCC FLUID_FOURCC('p', 'g', 'a', 'l')

// info
#define INAM_FCC FLUID_FOURCC('I', 'N', 'A', 'M')


static fluid_dls_chunk fluid_dls_read_chunk(fluid_dls_reader &parent)
{
    const auto start = parent.tell();
    const auto offset = parent.offset();
    auto encoded = parent;
    uint32_t id = parent.read_fourcc();
    uint32_t size = parent.read_u32();
    int headersize = 8;

    if(id == LIST_FCC || id == RIFF_FCC)
    {
        if(size < 4)
        {
            throw std::runtime_error{ "Bad LIST header: size < 4" };
        }
        id = parent.read_fourcc();
        size -= 4;
        headersize = 12;
    }
    else if(id == CRS1_FCC)
    {
        // Preserve Crystal's nonstandard chunk size before checking its extent.
        size = 28;
    }

    auto data = parent.take(size);
    parent.skip(size & 1u);
    encoded = encoded.take(parent.tell() - start - (size & 1u));
    return { id, headersize, offset, encoded, data };
}

template<class Visitor>
inline std::enable_if_t<std::is_invocable_v<Visitor, fluid_dls_chunk>, fluid_dls_chunk>
fluid_dls_font::visit_subchunks(fluid_dls_chunk chunk, uint32_t expected_4cc, Visitor &&visitor)
{
    const auto offset = chunk.offset;
    if(chunk.headersize != 12)  // only LIST chunks contains subchunks
    {
        if(expected_4cc == 0)
        {
            // clang-format off
            throw std::runtime_error{ string_format(
                                          "Expected chunk LIST[....], got " FMT_4CC_SPEC,
                                          FMT_4CC_ARG(chunk.id)
                                      )};
            // clang-format on
        }

        // clang-format off
        throw std::runtime_error{ string_format(
                                      "Expected chunk LIST[" FMT_4CC_SPEC "], got " FMT_4CC_SPEC,
                                      FMT_4CC_ARG(expected_4cc), // 0x20202020 is '    ' (four spaces)
                                      FMT_4CC_ARG(chunk.id)
                                  )};
        // clang-format on
    }

    if(expected_4cc != 0 && chunk.id != expected_4cc)
    {
        // clang-format off
        throw std::runtime_error{ string_format(
                                      "Expected chunk LIST[" FMT_4CC_SPEC "], got LIST[" FMT_4CC_SPEC "]",
                                      FMT_4CC_ARG(expected_4cc),
                                      FMT_4CC_ARG(chunk.id)
                                  )};
        // clang-format on
    }

    auto reader = chunk.data;
    while(reader.remaining() != 0)
    {
        auto subchunk = fluid_dls_read_chunk(reader);
        const auto subchunkpos = subchunk.offset;
        const auto headersize = subchunk.headersize;

        try
        {
            visitor(subchunk);
        }
        catch(...)
        {
            // clang-format off
            std::throw_with_nested(std::runtime_error{string_format(
                                       "visit in LIST '" FMT_4CC_SPEC "' ofs=0x%llx -> %s'" FMT_4CC_SPEC "' ofs=0x%llx",
                                       FMT_4CC_ARG(chunk.id),
                                       static_cast<unsigned long long>(offset),
                                       (headersize == 12) ? "LIST " : "",
                                       FMT_4CC_ARG(subchunk.id),
                                       static_cast<unsigned long long>(subchunkpos)
                                   )});
            // clang-format on
        }
    }

    return chunk;
}

inline std::string fluid_dls_font::read_name_from_info_entries(fluid_dls_reader reader)
{
    while(reader.remaining() != 0)
    {
        auto chunk = fluid_dls_read_chunk(reader);

        if(chunk.id == INAM_FCC)
        {
            std::string name;
            name.resize(chunk.data.size());

            chunk.data.read(name.data(), chunk.data.size());

            FLUID_LOG(FLUID_DBG, "Read INAM: '%s'", name.c_str());
            return name;
        }


    }

    return "";
}

// DLS-2.2 2.5 <dlid-ck>, DLSID Chunk
struct DLSID
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];

    friend bool operator==(const DLSID &lhs, const DLSID &rhs) noexcept
    {
        return lhs.Data1 == rhs.Data1 && lhs.Data2 == rhs.Data2 && lhs.Data3 == rhs.Data3 &&
               std::equal(std::begin(lhs.Data4), std::end(lhs.Data4), std::begin(rhs.Data4));
    }
};

#define DEFINE_DLSID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const DLSID name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }

// Specified in DLS-2.2 2.6 <cdl-ck>, Conditional Chunk
// From DLS-2.2 4.2 DLS Level 2 Header File (dls2.h)
// Copyright: Written by Microsoft 1998. Released for public use.
DEFINE_DLSID(DLSID_GMInHardware, 0x178f2f24, 0xc364, 0x11d1, 0xa7, 0x60, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12);
DEFINE_DLSID(DLSID_GSInHardware, 0x178f2f25, 0xc364, 0x11d1, 0xa7, 0x60, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12);
DEFINE_DLSID(DLSID_XGInHardware, 0x178f2f26, 0xc364, 0x11d1, 0xa7, 0x60, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12);
DEFINE_DLSID(DLSID_SupportsDLS1, 0x178f2f27, 0xc364, 0x11d1, 0xa7, 0x60, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12);
DEFINE_DLSID(DLSID_SupportsDLS2, 0xf14599e5, 0x4689, 0x11d2, 0xaf, 0xa6, 0x0, 0xaa, 0x0, 0x24, 0xd8, 0xb6);
DEFINE_DLSID(DLSID_SampleMemorySize, 0x178f2f28, 0xc364, 0x11d1, 0xa7, 0x60, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12);
DEFINE_DLSID(DLSID_ManufacturersID, 0xb03e1181, 0x8095, 0x11d2, 0xa1, 0xef, 0x0, 0x60, 0x8, 0x33, 0xdb, 0xd8);
DEFINE_DLSID(DLSID_ProductID, 0xb03e1182, 0x8095, 0x11d2, 0xa1, 0xef, 0x0, 0x60, 0x8, 0x33, 0xdb, 0xd8);
DEFINE_DLSID(DLSID_SamplePlaybackRate, 0x2a91f713, 0xa4bf, 0x11d2, 0xbb, 0xdf, 0x0, 0x60, 0x8, 0x33, 0xdb, 0xd8);

inline static void fluid_dls_read_guid(fluid_dls_reader &reader, DLSID &id)
{
    id.Data1 = reader.read_u32();
    id.Data2 = reader.read_u16();
    id.Data3 = reader.read_u16();

    reader.read(id.Data4, 8);
}

// macro WAVE_FORMAT_PCM is defined in some Windows headers
#ifndef WAVE_FORMAT_PCM
constexpr uint16_t WAVE_FORMAT_PCM = 0x0001;
#endif

static inline void read_data_lpcm(void *dest, const void *data, fluid_long_long_t size, int bitdepth)
{
    if(bitdepth == 16)
    {
        memcpy(dest, data, size);
        return;
    }

    if(bitdepth != 8)
    {
        // this should never happen!
        throw std::runtime_error{ "Unsupported bitdepth" };
    }

    const auto *src = static_cast<const uint8_t *>(data);
    auto *dest16 = static_cast<int16_t *>(dest);

    for(fluid_long_long_t i = 0; i < size; ++i)
    {
        dest16[i] = static_cast<int16_t>((static_cast<int>(src[i]) - 128) * 256);
    }
}

// DLS-2.2 2.8 <rgnh-ck>, Region Header Chunk
[[maybe_unused]] constexpr uint16_t F_RGN_OPTION_SELFNONEXCLUSIVE = 0x0001;

// DLS-2.2 2.10 <art2-ck>, Level 2 Articulator Chunk; Table 9 and 10
// DLS Level 2 (1 is included) Sources, Controls, Destinations and Transforms
// Modulator Sources
[[maybe_unused]] constexpr uint16_t CONN_SRC_NONE = 0x0000;            // No Source
[[maybe_unused]] constexpr uint16_t CONN_SRC_LFO = 0x0001;             // Low Frequency Oscillator
[[maybe_unused]] constexpr uint16_t CONN_SRC_KEYONVELOCITY = 0x0002;   // Note-On Velocity
[[maybe_unused]] constexpr uint16_t CONN_SRC_KEYNUMBER = 0x0003;       // Note Number
[[maybe_unused]] constexpr uint16_t CONN_SRC_EG1 = 0x0004;             // Envelope Generator 1
[[maybe_unused]] constexpr uint16_t CONN_SRC_EG2 = 0x0005;             // Envelope Generator 2
[[maybe_unused]] constexpr uint16_t CONN_SRC_PITCHWHEEL = 0x0006;      // Pitch Wheel
[[maybe_unused]] constexpr uint16_t CONN_SRC_POLYPRESSURE = 0x0007;    // Polyphonic Pressure
[[maybe_unused]] constexpr uint16_t CONN_SRC_CHANNELPRESSURE = 0x0008; // Channel Pressure
[[maybe_unused]] constexpr uint16_t CONN_SRC_VIBRATO = 0x0009;         // Vibrato LFO

// MIDI Controller Sources
[[maybe_unused]] constexpr uint16_t CONN_SRC_CC1 = 0x0081;  // Modulation
[[maybe_unused]] constexpr uint16_t CONN_SRC_CC7 = 0x0087;  // Channel Volume
[[maybe_unused]] constexpr uint16_t CONN_SRC_CC10 = 0x008a; // Pan
[[maybe_unused]] constexpr uint16_t CONN_SRC_CC11 = 0x008b; // Expression
[[maybe_unused]] constexpr uint16_t CONN_SRC_CC91 = 0x00db; // Chorus Send
[[maybe_unused]] constexpr uint16_t CONN_SRC_CC93 = 0x00dd; // Reverb Send

// Registered Parameter Numbers
[[maybe_unused]] constexpr uint16_t CONN_SRC_RPN0 = 0x0100; // RPN0 - Pitch Bend Range
[[maybe_unused]] constexpr uint16_t CONN_SRC_RPN1 = 0x0101; // RPN1 - Fine Tune
[[maybe_unused]] constexpr uint16_t CONN_SRC_RPN2 = 0x0102; // RPN2 - Coarse Tune

// Generic Destinations
[[maybe_unused]] constexpr uint16_t CONN_DST_NONE = 0x0000;      // No Destination
[[maybe_unused]] constexpr uint16_t CONN_DST_GAIN = 0x0001;      // Gain
[[maybe_unused]] constexpr uint16_t CONN_DST_RESERVED = 0x0002;  // Reserved
[[maybe_unused]] constexpr uint16_t CONN_DST_PITCH = 0x0003;     // Pitch
[[maybe_unused]] constexpr uint16_t CONN_DST_PAN = 0x0004;       // Pan
[[maybe_unused]] constexpr uint16_t CONN_DST_KEYNUMBER = 0x0005; // Key Number Generator

// Channel Output Destinations
[[maybe_unused]] constexpr uint16_t CONN_DST_LEFT = 0x0010;        // Left Channel Send
[[maybe_unused]] constexpr uint16_t CONN_DST_RIGHT = 0x0011;       // Right Channel Send
[[maybe_unused]] constexpr uint16_t CONN_DST_CENTER = 0x0012;      // Center Channel Send
[[maybe_unused]] constexpr uint16_t CONN_DST_LFE_CHANNEL = 0x0013; // LFE Channel Send
[[maybe_unused]] constexpr uint16_t CONN_DST_LEFTREAR = 0x0014;    // Left Rear Channel Send
[[maybe_unused]] constexpr uint16_t CONN_DST_RIGHTREAR = 0x0015;   // Right Rear Channel Send
[[maybe_unused]] constexpr uint16_t CONN_DST_CHORUS = 0x0080;      // Chorus Send
[[maybe_unused]] constexpr uint16_t CONN_DST_REVERB = 0x0081;      // Reverb Send

// Modulator LFO Destinations
[[maybe_unused]] constexpr uint16_t CONN_DST_LFO_FREQUENCY = 0x0104;  // LFO Frequency
[[maybe_unused]] constexpr uint16_t CONN_DST_LFO_STARTDELAY = 0x0105; // LFO Start Delay Time

// Vibrato LFO Destinations
[[maybe_unused]] constexpr uint16_t CONN_DST_VIB_FREQUENCY = 0x0114;  // Vibrato Frequency
[[maybe_unused]] constexpr uint16_t CONN_DST_VIB_STARTDELAY = 0x0115; // Vibrato Start Delay

// EG Destinations
[[maybe_unused]] constexpr uint16_t CONN_DST_EG1_ATTACKTIME = 0x0206;   // EG1 Attack Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG1_DECAYTIME = 0x0207;    // EG1 Decay Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG1_RESERVED = 0x0208;     // EG1 Reserved
[[maybe_unused]] constexpr uint16_t CONN_DST_EG1_RELEASETIME = 0x0209;  // EG1 Release Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG1_SUSTAINLEVEL = 0x020A; // EG1 Sustain Level
[[maybe_unused]] constexpr uint16_t CONN_DST_EG1_DELAYTIME = 0x020B;    // EG1 Delay Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG1_HOLDTIME = 0x020C;     // EG1 Hold Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG1_SHUTDOWNTIME = 0x020D; // EG1 Shutdown Time

[[maybe_unused]] constexpr uint16_t CONN_DST_EG2_ATTACKTIME = 0x030A;   // EG2 Attack Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG2_DECAYTIME = 0x030B;    // EG2 Decay Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG2_RESERVED = 0x030C;     // EG2 Reserved
[[maybe_unused]] constexpr uint16_t CONN_DST_EG2_RELEASETIME = 0x030D;  // EG2 Release Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG2_SUSTAINLEVEL = 0x030E; // EG2 Sustain Level
[[maybe_unused]] constexpr uint16_t CONN_DST_EG2_DELAYTIME = 0x030F;    // EG2 Delay Time
[[maybe_unused]] constexpr uint16_t CONN_DST_EG2_HOLDTIME = 0x0310;     // EG2 Hold Time

// Filter Destinations
[[maybe_unused]] constexpr uint16_t CONN_DST_FILTER_CUTOFF = 0x0500; // Filter Cutoff Frequency
[[maybe_unused]] constexpr uint16_t CONN_DST_FILTER_Q = 0x0501;      // Filter Resonance

// Transforms
[[maybe_unused]] constexpr uint16_t CONN_TRN_NONE = 0x0000;    // No Transform
[[maybe_unused]] constexpr uint16_t CONN_TRN_CONCAVE = 0x0001; // Concave Transform
[[maybe_unused]] constexpr uint16_t CONN_TRN_CONVEX = 0x0002;  // Convex Transform
[[maybe_unused]] constexpr uint16_t CONN_TRN_SWITCH = 0x0003;  // Switch Transform

// DLS-2.2 2.10 <art2-ck>, Level 2 Articulator Chunk
// transform mask
[[maybe_unused]] constexpr uint16_t TRN_SRC_INV_MASK = 0b100000'000000'0000;
[[maybe_unused]] constexpr uint16_t TRN_SRC_BIP_MASK = 0b010000'000000'0000;
[[maybe_unused]] constexpr uint16_t TRN_SRC_TRN_MASK = 0b001111'000000'0000;
[[maybe_unused]] constexpr uint16_t TRN_CTL_INV_MASK = 0b000000'100000'0000;
[[maybe_unused]] constexpr uint16_t TRN_CTL_BIP_MASK = 0b000000'010000'0000;
[[maybe_unused]] constexpr uint16_t TRN_CTL_TRN_MASK = 0b000000'001111'0000;
[[maybe_unused]] constexpr uint16_t TRN_OUT_TRN_MASK = 0b000000'000000'1111;

struct DLSTransform
{
    uint16_t out_trans : 4;
    uint16_t ctl_trans : 4;
    uint16_t src_trans : 4;
    bool ctl_inv : 1;
    bool src_inv : 1;
    bool ctl_bip : 1;
    bool src_bip : 1;

    explicit DLSTransform(uint16_t transform) noexcept
        : out_trans(transform & TRN_OUT_TRN_MASK), ctl_trans((transform & TRN_CTL_TRN_MASK) >> 4),
          src_trans((transform & TRN_SRC_TRN_MASK) >> 10), ctl_inv((transform & TRN_CTL_INV_MASK) != 0),
          src_inv((transform & TRN_SRC_INV_MASK) != 0), ctl_bip((transform & TRN_CTL_BIP_MASK) != 0),
          src_bip((transform & TRN_SRC_BIP_MASK) != 0)
    {
    }

    explicit operator uint16_t() const noexcept
    {
        return (src_inv ? TRN_SRC_INV_MASK : 0) | (src_bip ? TRN_SRC_BIP_MASK : 0) |
               (src_trans << 10) | (ctl_inv ? TRN_CTL_INV_MASK : 0) |
               (ctl_bip ? TRN_CTL_BIP_MASK : 0) | (ctl_trans << 4) | out_trans;
    }
};

// move usControl transform to usSource transform
constexpr static uint16_t dls_transform_ctl_to_src(uint16_t transform)
{
    return ((transform << 6) & 0b111111'000000'0000) | (transform & 0b1111);
}

constexpr static std::optional<unsigned char> convert_dls_transform_to_fluid(uint16_t transform_mode)
{
    switch(transform_mode)
    {
    case CONN_TRN_NONE:
        return FLUID_MOD_LINEAR;

    case CONN_TRN_CONCAVE:
        return FLUID_MOD_CONCAVE;

    case CONN_TRN_CONVEX:
        return FLUID_MOD_CONVEX;

    case CONN_TRN_SWITCH:
        return FLUID_MOD_SWITCH;

    default:
        return std::nullopt;
    }
}

constexpr static std::optional<uint16_t> convert_dls_mod_dest_to_gen(uint16_t dest)
{
    if(dest == CONN_DST_NONE || dest == CONN_DST_RESERVED || dest == CONN_DST_EG1_RESERVED ||
            dest == CONN_DST_EG2_RESERVED || (dest >= CONN_DST_LEFT && dest <= CONN_DST_RIGHTREAR))
    {
        return std::nullopt;
    }

    switch(dest)
    {
    case CONN_DST_GAIN:
        return GEN_ATTENUATION; // NOTE: attenuation = -gain

    case CONN_DST_EG1_SUSTAINLEVEL:
        return GEN_VOLENVSUSTAIN; // NOTE: SF2 sustain (atten) in dB = (100% - sustainlevel) * 96 dB (or 144 dB, whatever)

    case CONN_DST_EG2_SUSTAINLEVEL:
        return GEN_MODENVSUSTAIN; // NOTE: SF2 sustain (atten) = 100% - sustainlevel

    case CONN_DST_PITCH:
        return GEN_FINETUNE;

    case CONN_DST_PAN:
        return GEN_PAN;

    case CONN_DST_CHORUS:
        return GEN_CHORUSSEND;

    case CONN_DST_REVERB:
        return GEN_REVERBSEND;

    case CONN_DST_LFO_FREQUENCY:
        return GEN_MODLFOFREQ;

    case CONN_DST_LFO_STARTDELAY:
        return GEN_MODLFODELAY;

    case CONN_DST_VIB_FREQUENCY:
        return GEN_VIBLFOFREQ;

    case CONN_DST_VIB_STARTDELAY:
        return GEN_VIBLFODELAY;

    case CONN_DST_EG1_ATTACKTIME:
        return GEN_VOLENVATTACK;

    case CONN_DST_EG1_DECAYTIME:
        return GEN_VOLENVDECAY;

    case CONN_DST_EG1_RELEASETIME:
        return GEN_VOLENVRELEASE;

    case CONN_DST_EG1_DELAYTIME:
        return GEN_VOLENVDELAY;

    case CONN_DST_EG1_HOLDTIME:
        return GEN_VOLENVHOLD;

    case CONN_DST_EG1_SHUTDOWNTIME:
        return std::nullopt;

    case CONN_DST_EG2_ATTACKTIME:
        return GEN_MODENVATTACK;

    case CONN_DST_EG2_DECAYTIME:
        return GEN_MODENVDECAY;

    case CONN_DST_EG2_RELEASETIME:
        return GEN_MODENVRELEASE;

    case CONN_DST_EG2_DELAYTIME:
        return GEN_MODENVDELAY;

    case CONN_DST_EG2_HOLDTIME:
        return GEN_MODENVHOLD;

    case CONN_DST_FILTER_CUTOFF:
        return GEN_FILTERFC;

    case CONN_DST_FILTER_Q:
        return GEN_FILTERQ;

    default:
        return std::nullopt;
    }
}

// gsrc: general source
constexpr std::optional<uint16_t> convert_dls_mod_gsrc(uint16_t source)
{
    switch(source)
    {
    case CONN_SRC_NONE:
        return FLUID_MOD_NONE;

    case CONN_SRC_KEYONVELOCITY:
        return FLUID_MOD_VELOCITY;

    case CONN_SRC_KEYNUMBER:
        return FLUID_MOD_KEY;

    case CONN_SRC_POLYPRESSURE:
        return FLUID_MOD_KEYPRESSURE;

    case CONN_SRC_CHANNELPRESSURE:
        return FLUID_MOD_CHANNELPRESSURE;

    case CONN_SRC_PITCHWHEEL:
        return FLUID_MOD_PITCHWHEEL;

    case CONN_SRC_RPN0:
        return FLUID_MOD_PITCHWHEELSENS;

    default:
        return std::nullopt;
    }
}

// csrc: CC source
constexpr std::optional<uint16_t> convert_dls_mod_csrc(uint16_t source)
{
    switch(source)
    {
    case CONN_SRC_CC1:
        return 1;

    case CONN_SRC_CC7:
        return 7;

    case CONN_SRC_CC10:
        return 10;

    case CONN_SRC_CC11:
        return 11;

    case CONN_SRC_CC91:
        return 91;

    case CONN_SRC_CC93:
        return 93;

    default:
        return std::nullopt;
    }
}

void add_dls_connectionblock_to_art(fluid_dls_articulation &art,
                                    uint16_t dest_gen,
                                    uint16_t src_dls,
                                    fluid_real_t scale,
                                    DLSTransform transform,
                                    uint16_t control_dls)
{
    if(src_dls == CONN_SRC_NONE && control_dls == CONN_SRC_NONE)
    {
        art.gens[dest_gen] = art.gens[dest_gen].value_or(0) + scale;
        return;
    }

    // DLS-2 1.6.5.5 Output Transforms
    // There are no output transforms defined in the DLS Level 2. The bits in the output transform
    // field must all be set to zero for compatibility with future versions of DLS.

    // if output transform is set to something else, but input transform is none, and the connection
    // block does not have a control source, apply the output transform to the input instead.
    if(transform.out_trans != CONN_TRN_NONE && transform.src_trans == CONN_TRN_NONE && control_dls == CONN_SRC_NONE)
    {
        transform.src_trans = transform.out_trans;
        transform.out_trans = CONN_TRN_NONE;
    }

    if(transform.out_trans != CONN_TRN_NONE)
    {
        FLUID_LOG(FLUID_WARN, "Output transform in connection block is not supported, set to to linear");
    }

    fluid_mod_t mod{};
    auto src = convert_dls_mod_gsrc(src_dls);

    if(src.has_value())
    {
        mod.flags1 = FLUID_MOD_GC;
    }
    else
    {
        src = convert_dls_mod_csrc(src_dls);

        if(src.has_value())
        {
            mod.flags1 = FLUID_MOD_CC;
        }
        else
        {
            FLUID_LOG(FLUID_WARN,
                      "Ignoring connection block with unsupported source for srcOper: %u",
                      static_cast<unsigned>(src_dls));
            return;
        }
    }

    mod.src1 = src.value();

    auto ctl = convert_dls_mod_gsrc(control_dls);

    if(ctl.has_value())
    {
        mod.flags2 = FLUID_MOD_GC;
    }
    else
    {
        ctl = convert_dls_mod_csrc(control_dls);

        if(ctl.has_value())
        {
            mod.flags2 = FLUID_MOD_CC;
        }
        else
        {
            FLUID_LOG(FLUID_WARN,
                      "Ignoring connection block with unsupported source for amtSrcOper: %u",
                      static_cast<unsigned>(control_dls));
            return;
        }
    }

    mod.src2 = ctl.value();

    if(mod.src1 != FLUID_MOD_NONE)
    {
        mod.flags1 |= transform.src_bip ? FLUID_MOD_BIPOLAR : FLUID_MOD_UNIPOLAR;
        mod.flags1 |= transform.src_inv ? FLUID_MOD_NEGATIVE : FLUID_MOD_POSITIVE;
        auto transform_flag = convert_dls_transform_to_fluid(transform.src_trans);

        if(!transform_flag.has_value())
        {
            FLUID_LOG(FLUID_WARN, "Invalid DLS transform enum, ignoring connection block");
            return;
        }

        mod.flags1 |= transform_flag.value();
    }

    if(mod.src2 != FLUID_MOD_NONE)
    {
        mod.flags2 |= transform.ctl_bip ? FLUID_MOD_BIPOLAR : FLUID_MOD_UNIPOLAR;
        mod.flags2 |= transform.ctl_inv ? FLUID_MOD_NEGATIVE : FLUID_MOD_POSITIVE;
        auto transform_flag = convert_dls_transform_to_fluid(transform.ctl_trans);

        if(!transform_flag.has_value())
        {
            FLUID_LOG(FLUID_WARN, "Invalid DLS transform enum, ignoring connection block");
            return;
        }

        mod.flags2 |= transform_flag.value();
    }

    mod.amount = scale;
    mod.dest = dest_gen;

    // transform.out_trans is ignored because unsupported
    mod.trans = FLUID_MOD_TRANSFORM_LINEAR;

    if(mod.src1 == FLUID_MOD_VELOCITY && mod.dest == GEN_ATTENUATION && (mod.flags1 & FLUID_MOD_CONCAVE) != 0)
    {
        // confirmed as vel2att modulator
        // fix when non-inverted (positive), this is seen in Fury.dls from https://www.ronimusic.com/smp_ios_dls_files.htm
        if((mod.flags1 & FLUID_MOD_NEGATIVE) == 0)
        {
            FLUID_LOG(FLUID_DBG, "Fixing non-inverted vel2att modulator to inverted");
            mod.flags1 |= FLUID_MOD_NEGATIVE;
        }
    }

    art.add_mod(mod);
}

void add_dls_connectionblock_to_art(fluid_dls_articulation &art, uint16_t dest_gen, uint16_t src_dls, fluid_real_t scale, DLSTransform transform)
{
    add_dls_connectionblock_to_art(art, dest_gen, src_dls, scale, transform, CONN_SRC_NONE);
}

void convert_dls_connectionblock_to_art(fluid_dls_articulation &art,
                                        uint16_t source,
                                        uint16_t control,
                                        uint16_t destination,
                                        uint16_t transform,
                                        int32_t scale_16)
{
    switch(destination)
    {
    case CONN_DST_NONE:

    // these reserved dest are used in gm.dls, interesting
    case CONN_DST_RESERVED:
    case CONN_DST_EG1_RESERVED:
    case CONN_DST_EG2_RESERVED:
        return;

    default:
        break;
    }

    if(source == CONN_SRC_NONE && control != CONN_SRC_NONE)
    {
        source = std::exchange(control, CONN_SRC_NONE);
        transform = dls_transform_ctl_to_src(transform);
    }

    if(source == CONN_SRC_RPN1)
    {
        FLUID_LOG(FLUID_WARN, "Ignoring connection block with RPN 1 source");
        return;
    }

    if(source == CONN_SRC_RPN2)
    {
        FLUID_LOG(FLUID_WARN, "Ignoring connection block with RPN 2 source");
        return;
    }

    if(source == CONN_SRC_EG1)
    {
        FLUID_LOG(FLUID_WARN, "Ignoring connection block with EG1 source");
        return;
    }

    auto trans = DLSTransform{ transform };
    fluid_real_t scale = static_cast<fluid_real_t>(scale_16) / 65536;

    // keynum * scale -> keynum, linear
    if(source == CONN_SRC_KEYNUMBER && control == CONN_SRC_NONE &&
            destination == CONN_DST_KEYNUMBER && transform == 0)
    {
        art.keynum_scale = scale / 12800;
        return;
    }

    // LFO or EG -> something
    // they are not available as modulator source in SF2
    // note: keynumToVol/ModEnvHold/Decay is implemented through modulator. why they exist?
    if(source == CONN_SRC_LFO)
    {
        if(!trans.src_bip)
        {
            FLUID_LOG(FLUID_DBG, "Non bipolar LFO source is not supported, set to bipolar");
        }

        if(trans.src_inv || trans.src_trans != CONN_TRN_NONE)
        {
            FLUID_LOG(FLUID_WARN, "LFO source is inverted or transformed, ignoring transform");
        }

        switch(destination)
        {
        case CONN_DST_PITCH:
            add_dls_connectionblock_to_art(
                art, GEN_MODLFOTOPITCH, control, scale, DLSTransform{ dls_transform_ctl_to_src(transform) });
            return;

        case CONN_DST_FILTER_CUTOFF:
            add_dls_connectionblock_to_art(art,
                                           GEN_MODLFOTOFILTERFC,
                                           control,
                                           scale,
                                           DLSTransform{ dls_transform_ctl_to_src(transform) });
            return;

        case CONN_DST_GAIN:
            add_dls_connectionblock_to_art(
                art, GEN_MODLFOTOVOL, control, scale, DLSTransform{ dls_transform_ctl_to_src(transform) });
            return;

        default:
            break;
        }
    }
    else if(source == CONN_SRC_VIBRATO)
    {
        if(!trans.src_bip)
        {
            FLUID_LOG(FLUID_DBG, "Non bipolar vibrato LFO source is not supported, set to bipolar");
        }

        if(trans.src_inv || trans.src_trans != CONN_TRN_NONE)
        {
            FLUID_LOG(FLUID_WARN, "Inverted or transformed vibrato LFO source is not supported, ignoring transform");
        }

        switch(destination)
        {
        case CONN_DST_PITCH:
            add_dls_connectionblock_to_art(
                art, GEN_VIBLFOTOPITCH, control, scale, DLSTransform{ dls_transform_ctl_to_src(transform) });
            return;

        default:
            break;
        }
    }
    else if(source == CONN_SRC_EG2)
    {
        if(trans.src_inv || trans.src_bip || trans.src_trans != CONN_TRN_NONE)
        {
            FLUID_LOG(FLUID_WARN, "EG2 source is transformed, ignoring transform");
        }

        switch(destination)
        {
        case CONN_DST_PITCH:
            add_dls_connectionblock_to_art(
                art, GEN_MODENVTOPITCH, control, scale, DLSTransform{ dls_transform_ctl_to_src(transform) });
            return;

        case CONN_DST_FILTER_CUTOFF:
            add_dls_connectionblock_to_art(art,
                                           GEN_MODENVTOFILTERFC,
                                           control,
                                           scale,
                                           DLSTransform{ dls_transform_ctl_to_src(transform) });
            return;

        default:
            break;
        }
    }

    if(destination == CONN_DST_GAIN)
    {
        scale = -scale;
    }

    if(destination == CONN_DST_EG1_SUSTAINLEVEL)
    {
        art.gens[GEN_VOLENVSUSTAIN] = art.gens[GEN_VOLENVSUSTAIN].value_or(0) + 960;
        scale = -scale * 960 / 1000;
    }

    if(destination == CONN_DST_EG2_SUSTAINLEVEL)
    {
        art.gens[GEN_MODENVSUSTAIN] = art.gens[GEN_MODENVSUSTAIN].value_or(0) + 1000;
        scale = -scale;
    }

    auto dest_gen = convert_dls_mod_dest_to_gen(destination);

    if(!dest_gen.has_value())
    {
        FLUID_LOG(FLUID_DBG,
                  "Ignoring connection block with unsupported destination: %u",
                  static_cast<unsigned>(destination));
        return;
    }

    add_dls_connectionblock_to_art(art, dest_gen.value(), source, scale, trans, control);
}

// Parse the DLS structure
fluid_dls_font::fluid_dls_font(fluid_synth_t *synth,
                               fluid_sfont_t *sfont,
                               const fluid_file_callbacks_t *fcbs_in,
                               const char *filename,
                               uint32_t output_sample_rate,
                               bool try_mlock)
    : synth(synth), sfont(sfont), fcbs(*fcbs_in), output_sample_rate(output_sample_rate), filename(filename)
{
    // Get basic file information

    file = fcbs.fopen(filename); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    if(file == nullptr)
    {
        throw std::runtime_error{ string_format("Unable to open file '%s'", filename) };
    }

    if(fcbs.fseek(file, 0L, SEEK_END) == FLUID_FAILED)
    {
        throw std::runtime_error{ "Seek to end of file failed" };
    }

    fluid_long_long_t filesize = fcbs.ftell(file);

    if(filesize == FLUID_FAILED)
    {
        throw std::runtime_error{ "Get end of file position failed" };
    }

    if(fcbs.fseek(file, 0, SEEK_SET) == FLUID_FAILED)
    {
        throw std::runtime_error{ "Rewind to start of file failed" };
    }

    if(filesize < 12)
    {
        throw std::runtime_error{ "File too short! (< 12 bytes)" };
    }

    // Parse DLS
    // chunk: RIFF[DLS ]
    // subchunk: ...

    fluid_dls_input input(fcbs, file);
    fluid_dls_reader file_reader(input, 0, filesize);
    std::optional<fluid_dls_chunk> lins;
    std::optional<fluid_dls_chunk> wvpl;
    std::optional<fluid_dls_reader> pgal;

    auto id = file_reader.read_fourcc();
    const auto riff_size = file_reader.read_u32();

    if(id != RIFF_FCC)
    {
        throw std::runtime_error{ "Not a RIFF file" };
    }

    id = file_reader.read_fourcc(); // load file ID

    if(id != DLS_FCC)
    {
        auto* p = (unsigned char *)&id;
        throw std::runtime_error{ string_format("Not a DLS file. Expected header identifier 'DLS ' but found '%c%c%c%c'", p[0], p[1], p[2], p[3]) };
    }

    if(riff_size + 8LL > filesize)
    {
        uint8_t from[4] = {
            static_cast<uint8_t>(riff_size >>  0),
            static_cast<uint8_t>(riff_size >>  8),
            static_cast<uint8_t>(riff_size >> 16),
            static_cast<uint8_t>(riff_size >> 24),
        };
        auto maxSize = filesize - 8LL;
        uint8_t to[4] = {
            static_cast<uint8_t>(maxSize >>  0),
            static_cast<uint8_t>(maxSize >>  8),
            static_cast<uint8_t>(maxSize >> 16),
            static_cast<uint8_t>(maxSize >> 24),
        };
        auto Hint = string_format(" (Hint: Try changing the hexadecimal little-endian size of the RIFF chunk from %02X%02X%02X%02X to %02X%02X%02X%02X using a hex editor.)", from[0], from[1], from[2], from[3], to[0], to[1], to[2], to[3]);
        throw std::runtime_error{ string_format("DLS file is too short! Outermost RIFF chunk reports a payload size of %u bytes, but there are only %lld bytes left in the file.%s", riff_size, maxSize, riff_size == filesize ? Hint.c_str() : "") };
    }

    if(riff_size + 8LL < filesize)
    {
        FLUID_LOG(FLUID_WARN, "DLS file has extra data after RIFF chunk, ignoring.");
    }

    // we don't care about real file size after this point
    filesize = riff_size + 8LL;

    file_reader.seek(0);
    auto root = fluid_dls_read_chunk(file_reader);

    // iterate over chunks in the RIFF form
    try
    {
        visit_subchunks(root, DLS_FCC, [&](fluid_dls_chunk subchunk)
        {
            const auto pos = subchunk.offset;
            const auto headersize = subchunk.headersize;
            auto reader = subchunk.data;
            switch(subchunk.id)  // toplevel chunk
            {
            case DLID_FCC:
            case VERS_FCC:
                break;

            case CDL_FCC:
                if(!execute_cdls(reader))
                {
                    throw std::runtime_error{ "DLS toplevel CDL bypasses the sound library" };
                }

                break;

            case COLH_FCC:
            {
                // read it now to preserve the instrument vector
                if(subchunk.data.size() != 4)
                {
                    throw std::runtime_error{ "DLS colh chunk size is not 4 bytes" };
                }

                uint32_t colh;
                colh = reader.read_u32();

                if(colh > filesize / 32) // A LIST[INS] with its required INSH chunk needs at least 32 bytes
                {
                    throw std::runtime_error{ "DLS instrument count exceeds file size" };
                }

                instruments.reserve(colh);
                break;
            }

            case PTBL_FCC:
            {
                // read ptbl now
                uint32_t cbsize;
                cbsize = reader.read_u32();

                if(cbsize < 8u)
                {
                    throw std::runtime_error{ "DLS ptbl chunk has invalid cbSize" };
                }

                uint32_t cues; // sample count
                cues = reader.read_u32();

                // RIFF payload sizes are uint32 and can be at max 0xFFFF'FFFF, therefore the max value of cues will be 0x3FFF'FFFF, as bigger
                // values would require an uint64 chunksize, contrary to the RIFF spec. Catch possible overflow here.
                if(cues > (std::numeric_limits<uint32_t>::max() - cbsize) / 4u)
                {
                    throw std::runtime_error{ "Too many poolcue records are contained in the ptbl chunk." };
                }

                if(cues * 4u + cbsize != subchunk.data.size())
                {
                    throw std::runtime_error{ "DLS ptbl chunk has corrupted size" };
                }

                reader.skip(cbsize - 8); // usually cbsize == 8

                poolcues.resize(cues);

                for(uint32_t i = 0; i < cues; i++)
                {
                    poolcues[i] = reader.read_u32();
                }

                break;
            }

            case INFO_FCC:
                read_name_from_info_entries(reader);
                break;

            case LINS_FCC:
                lins = subchunk;
                break;

            case WVPL_FCC:
                wvpl = subchunk;
                break;

            case PGAL_FCC:
                pgal = reader;
                break;

            default:
                // clang-format off
                FLUID_LOG(FLUID_WARN,
                          "Ignoring unknown top-level DLS chunk %s'" FMT_4CC_SPEC "' ofs=0x%llx",
                          headersize == 12 ? "LIST " : "",
                          FMT_4CC_ARG(subchunk.id),
                          pos
                         );
                // clang-format on
                break;
            }
        });
    }
    catch(...)
    {
        std::throw_with_nested(std::runtime_error{ "Exception thrown while reading RIFF form" });
    }

    // Parse samples (LIST[wvpl])
    try
    {
        if(!wvpl)
        {
            throw std::runtime_error{ "DLS does not contain a LIST[wvpl] chunk" };
        }

        parse_wvpl(*wvpl);
    }
    catch(...)
    {
        std::throw_with_nested(std::runtime_error{ "Exception thrown while parsing samples" });
    }

    // reading sample data is now completed

    // append_sample_frames guarantees this byte count fits runtime consumers.
    const auto sample_bytes = static_cast<unsigned int>(sampledata.size() * sizeof(int16_t));
    FLUID_LOG(FLUID_DBG, "DLS %zu samples read, %u bytes", samples.size(), sample_bytes);

    sampledata_mlock = mlock_guard{ sampledata.data(), sample_bytes };

    if(try_mlock && !sampledata.empty())
    {
        if(sampledata_mlock.lock() != 0)
        {
            FLUID_LOG(FLUID_WARN, "Failed to pin the sample data to RAM; swapping is possible.");
        }
        else
        {
            FLUID_LOG(FLUID_DBG, "Sample data pinned to RAM, %u bytes", sample_bytes);
        }
    }

    // Parse LIST[lins]
    try
    {
        if(!lins)
        {
            throw std::runtime_error{ "DLS does not contain a LIST[lins] chunk" };
        }

        parse_lins(*lins);
    }
    catch(...)
    {
        std::throw_with_nested(std::runtime_error{ "Exception thrown while parsing instruments" });
    }

    FLUID_LOG(FLUID_DBG, "DLS %zu instruments read", instruments.size());

    // Parse 'pgal'
    total_presets = instruments.size();

    if(pgal && pgal->size() != 0)
    {
        try
        {
            parse_pgal(*pgal);
        }
        catch(...)
        {
            std::throw_with_nested(std::runtime_error{ "Exception thrown while parsing pgal" });
        }
    }

    // convert dls samples to fluid samples

    try
    {
        samples_fluid.reserve(samples.size());
    }
    catch(...)
    {
        std::throw_with_nested(
            std::runtime_error{ "Exception thrown while allocating fluid_sample_t" });
    }

    bool invalid_loops_were_sanitized = false;
    for(auto &sample : samples)
    {
        auto& fluid = samples_fluid.emplace_back();
        fluid.start = sample.start;
        fluid.end = sample.end > sample.start ? sample.end - 1 : sample.start;
        fluid.samplerate = sample.samplerate;
        std::strncpy(fluid.name, sample.name.c_str(), sizeof(fluid.name) - 1);
        fluid.name[sizeof(fluid.name) - 1] = '\0';

        if(sample.wsmp.has_value())
        {
            const auto &wsmp = sample.wsmp.value();
            fluid.loopstart = sample.start + wsmp.loop_start;
            fluid.loopend = sample.start + wsmp.loop_start + wsmp.loop_length;
            fluid.origpitch = wsmp.unity_note;
            fluid.pitchadj = wsmp.fine_tune;
            invalid_loops_were_sanitized |= fluid_sample_sanitize_loop(&fluid, sample_bytes);
        }
        else
        {
            fluid.loopstart = 0;
            fluid.loopend = 0;
            fluid.origpitch = 0;
            fluid.pitchadj = 0;
        }

        fluid.data = sampledata.data();
        fluid.sampletype = FLUID_SAMPLETYPE_MONO;
        fluid.default_modulators = &this->sfont->default_mod_list;

        if(fluid_sample_validate(&fluid, sample_bytes) != FLUID_OK)
        {
            fluid = {};
        }
    }

    if(invalid_loops_were_sanitized)
    {
        FLUID_LOG(FLUID_WARN,
                  "Some invalid DLS sample loops were sanitized! If you experience audible glitches, "
                  "start fluidsynth in verbose mode for detailed information.");
    }

    // put info in dls_sample into region
    for(auto &instrument : instruments)
    {
        for(auto &region : instrument.regions)
        {
            auto &sample = samples.at(region.sampleindex);

            if(sample.wsmp.has_value())
            {
                const auto &wsmp = sample.wsmp.value();

                if(wsmp.loop_length == 0)
                {
                    region.samplemode_inherited = 0;
                }
                else
                {
                    if(wsmp.loop_type == 0)
                    {
                        region.samplemode_inherited = 1;
                    }
                    else if(wsmp.loop_type == 1)
                    {
                        region.samplemode_inherited = 3;
                    }
                    else
                    {
                        FLUID_LOG(FLUID_WARN,
                                  "Invalid loop type %u, defaulting to loop and release",
                                  static_cast<unsigned int>(wsmp.loop_type));
                        region.samplemode_inherited = 3;
                    }
                }

                region.gain_inherited = static_cast<fluid_real_t>(wsmp.gain) / 65536;
            }
            else
            {
                region.samplemode_inherited = 0;
                region.gain_inherited = 0;
            }
        }
    }

    // now samples list can be cleared
    samples.clear();

    std::sort(instruments.begin(),
              instruments.end(),
              [](const fluid_dls_instrument & lhs, const fluid_dls_instrument & rhs)
    {
        if(lhs.bankmsb * 128 + lhs.banklsb == rhs.bankmsb * 128 + rhs.banklsb)
        {
            return lhs.pcnum < rhs.pcnum;
        }

        return lhs.bankmsb * 128 + lhs.banklsb < rhs.bankmsb * 128 + rhs.banklsb;
    });

    std::sort(instruments_fluid_data.begin(),
              instruments_fluid_data.end(),
              [](const fluid_dls_instrument_fluid_data & lhs, const fluid_dls_instrument_fluid_data & rhs)
    {
        if(lhs.bankmsb * 128 + lhs.banklsb == rhs.bankmsb * 128 + rhs.banklsb)
        {
            return lhs.pcnum < rhs.pcnum;
        }

        return lhs.bankmsb * 128 + lhs.banklsb < rhs.bankmsb * 128 + rhs.banklsb;
    });

    // convert instruments to fluid presets
    instruments_fluid_data.reserve(total_presets);

    for(auto &instrument : instruments)
    {
        instrument.samples_fluid = samples_fluid.data();
        instrument.articulations = articulations.data();
        instrument.synth = synth;

        if(drum_note_aliasing.has_value())
        {
            instrument.drum_note_aliasing = drum_note_aliasing.value().data();
        }
        else
        {
            instrument.drum_note_aliasing = nullptr;
        }

        auto &self_data = instruments_fluid_data.emplace_back();
        self_data.banklsb = instrument.banklsb;
        self_data.bankmsb = instrument.bankmsb;
        self_data.pcnum = instrument.pcnum;
        self_data.is_drums = instrument.is_drums;
        self_data.instrument = &instrument;
        self_data.fluid.sfont = sfont;
        self_data.fluid.get_name = fluid_dls_preset_get_name;
        self_data.fluid.get_banknum = fluid_dls_preset_get_banknum;
        self_data.fluid.get_num = fluid_dls_preset_get_num;
        self_data.fluid.noteon = fluid_dls_preset_noteon;
        self_data.fluid.free = fluid_dls_preset_free;
        self_data.fluid.data = &self_data;
        self_data.fluid.notify = nullptr;

        for(auto &alias : instrument.aliases)
        {
            auto &alias_data = instruments_fluid_data.emplace_back();
            alias_data.banklsb = alias.banklsb;
            alias_data.bankmsb = alias.bankmsb;
            alias_data.pcnum = alias.pcnum;
            alias_data.is_drums = false;
            alias_data.instrument = &instrument;
            alias_data.fluid.sfont = sfont;
            alias_data.fluid.get_name = fluid_dls_preset_get_name;
            alias_data.fluid.get_banknum = fluid_dls_preset_get_banknum;
            alias_data.fluid.get_num = fluid_dls_preset_get_num;
            alias_data.fluid.noteon = fluid_dls_preset_noteon;
            alias_data.fluid.free = fluid_dls_preset_free;
            alias_data.fluid.data = &alias_data;
            alias_data.fluid.notify = nullptr;
        }

        // instrument.aliases is not used anymore, free it
        instrument.aliases.clear();
        instrument.aliases.shrink_to_fit();
    }
}

// cdl
inline std::optional<uint32_t> fluid_dls_font::eval_dlsid_query(const DLSID &dlsid)
{
    if(dlsid == DLSID_GMInHardware)
    {
        return std::numeric_limits<uint32_t>::max();
    }

    if(dlsid == DLSID_GSInHardware)
    {
        return std::numeric_limits<uint32_t>::max();
    }

    if(dlsid == DLSID_XGInHardware)
    {
        return std::numeric_limits<uint32_t>::max();
    }

    if(dlsid == DLSID_SupportsDLS1)
    {
        return std::numeric_limits<uint32_t>::max();
    }

    if(dlsid == DLSID_SupportsDLS2)
    {
        return std::numeric_limits<uint32_t>::max();
    }

    if(dlsid == DLSID_SampleMemorySize)
    {
        return std::numeric_limits<uint32_t>::max() / 4 * 3; // ~1.5GiB
    }

    if(dlsid == DLSID_ManufacturersID)
    {
        // See https://www.amei.or.jp/report/System_ID_e.html
        // Who are we?
        return 0x00004000; // a random number, no manufacturer have owned it
    }

    if(dlsid == DLSID_ProductID)
    {
        return 0x0d000721; // a random number
    }

    if(dlsid == DLSID_SamplePlaybackRate)
    {
        return output_sample_rate;
    }

    FLUID_LOG(FLUID_WARN,
              "Unknown CDL query {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
              dlsid.Data1,
              dlsid.Data2,
              dlsid.Data3,
              dlsid.Data4[0],
              dlsid.Data4[1],
              dlsid.Data4[2],
              dlsid.Data4[3],
              dlsid.Data4[4],
              dlsid.Data4[5],
              dlsid.Data4[6],
              dlsid.Data4[7]);
    return std::nullopt;
}

inline bool fluid_dls_font::execute_cdls(fluid_dls_reader reader)
{
    uint32_t stack[64];
    unsigned sp = 0;

    auto push = [&](uint32_t value)
    {
        if(sp >= sizeof(stack) / sizeof(stack[0]))
        {
            throw std::runtime_error{ "CDL stack overflow" };
        }

        stack[sp++] = value;
    };

    // DLS 2.2 section 2.6 represents logical TRUE as all 32 bits set.
    auto push_bool = [&](bool value)
    {
        push(value ? std::numeric_limits<uint32_t>::max() : 0);
    };

    auto pop = [&]() -> uint32_t
    {
        if(sp == 0)
        {
            throw std::runtime_error{ "CDL stack underflow" };
        }

        return stack[--sp];
    };

    uint16_t opcode;
    uint32_t rax; // these two are just temp vars
    uint32_t rbx;

    while(reader.remaining() != 0)
    {
        opcode = reader.read_u16();

        switch(opcode)
        {
        // Assign
        case 0x0010: // DLS_CDL_CONST
            rax = reader.read_u32();
            push(rax);
            break;

        // Unary operators
        case 0x000F: // DLS_CDL_NOT (logical)
            push_bool(!pop());
            break;

        // Binary operators
        case 0x0001: // DLS_CDL_AND
            rax = pop();
            rbx = pop();
            push(rax & rbx);
            break;

        case 0x0002: // DLS_CDL_OR
            rax = pop();
            rbx = pop();
            push(rax | rbx);
            break;

        case 0x0003: // DLS_CDL_XOR
            rax = pop();
            rbx = pop();
            push(rax ^ rbx);
            break;

        case 0x0004: // DLS_CDL_ADD
            rax = pop();
            rbx = pop();
            push(rax + rbx);
            break;

        case 0x0005:     // DLS_CDL_SUBTRACT
            rax = pop(); // X, top
            rbx = pop(); // Y, prev
            push(rax - rbx);
            break;

        case 0x0006: // DLS_CDL_MULTIPLY
            rax = pop();
            rbx = pop();
            push(rax * rbx);
            break;

        case 0x0007: // DLS_CDL_DIVIDE
            rax = pop();
            rbx = pop();

            if(rbx == 0)
            {
                throw std::runtime_error{ "CDL division by zero" };
            }

            push(rax / rbx);
            break;

        case 0x0008: // DLS_CDL_LOGICAL_AND
            rax = pop();
            rbx = pop();
            push_bool(rax && rbx);
            break;

        case 0x0009: // DLS_CDL_LOGICAL_OR
            rax = pop();
            rbx = pop();
            push_bool(rax || rbx);
            break;

        case 0x000A: // DLS_CDL_LT
            rax = pop();
            rbx = pop();
            push_bool(rax < rbx);
            break;

        case 0x000B: // DLS_CDL_LE
            rax = pop();
            rbx = pop();
            push_bool(rax <= rbx);
            break;

        case 0x000C: // DLS_CDL_GT
            rax = pop();
            rbx = pop();
            push_bool(rax > rbx);
            break;

        case 0x000D: // DLS_CDL_GE
            rax = pop();
            rbx = pop();
            push_bool(rax >= rbx);
            break;

        case 0x000E: // DLS_CDL_EQ
            rax = pop();
            rbx = pop();
            push_bool(rax == rbx);
            break;

        // Query
        case 0x0011: // DLS_CDL_QUERY
        case 0x0012: // DLS_CDL_QUERY_SUPPORTED
        {
            DLSID dlsid{};
            fluid_dls_read_guid(reader, dlsid);
            auto result = eval_dlsid_query(dlsid);

            if(opcode == 0x0011)  // Query
            {
                // DLS 2.2 section 2.6: unsupported queries evaluate to FALSE.
                push(result.value_or(0));
            }
            else
            {
                push_bool(result.has_value()); // Query_Supported
            }

            break;
        }

        default:
            // SIGILL lol
            throw std::runtime_error{ string_format("Unknown CDL opcode 0x%04x", opcode) };
        } // switch(opcode) end

    } // while

    return pop() != 0;
}

// wave
// Runtime sample indices are unsigned, and sample validation/sanitation takes
// an unsigned byte count. This is a representation bound, not a resource budget.
inline int16_t *fluid_dls_font::append_sample_frames(fluid_dls_sample &sample,
                                                   fluid_long_long_t frames)
{
    const auto max_frames = std::min(sampledata.max_size(),
        static_cast<size_t>(std::numeric_limits<unsigned int>::max()) / sizeof(int16_t));
    const auto start = sampledata.size();
    if(frames < 0 || static_cast<unsigned long long>(frames) > max_frames - start)
    {
        throw std::runtime_error{ "Decoded DLS samples exceed runtime buffer representation" };
    }

    const auto end = start + static_cast<size_t>(frames);
    sampledata.resize(end);
    sample.start = static_cast<unsigned int>(start);
    sample.end = static_cast<unsigned int>(end);
    return sampledata.empty() ? nullptr : sampledata.data() + start;
}

inline void fluid_dls_font::parse_wvpl(fluid_dls_chunk chunk)
{
    const auto offset = chunk.offset;
    const auto headersize = chunk.headersize;
    if(headersize != 12)
    {
        FLUID_LOG(FLUID_WARN, "Nonstandard 'wvpl' chunk; 'LIST[wvpl]' is legal");
    }

    // iterate ptbl
    for(unsigned i = 0; i < poolcues.size(); i++)
    {
        auto pos = poolcues[i];

        try
        {
            auto reader = chunk.data;
            reader.seek(pos);
            auto wave = fluid_dls_read_chunk(reader);
            if(wave.headersize != 12 || wave.id != WAVE_FCC)
            {
                throw std::runtime_error{ "DLS pool cue does not reference a wave LIST" };
            }

            auto &sample = samples.emplace_back();
            parse_wave(wave, sample);
        }
        catch(...)
        {
            std::throw_with_nested(std::runtime_error{ string_format(
                                       "Exception thrown while parsing LIST[wave] at offset 0x%llx, ptbl index %u", offset + headersize + pos, i) });
        }
    }
}

inline void fluid_dls_font::parse_wave(fluid_dls_chunk chunk, fluid_dls_sample &sample)
{
    bool contains_data = false;
    uint16_t fmtTag{};
    uint16_t bitsPerSample{};

    visit_subchunks(chunk, WAVE_FCC, [&](fluid_dls_chunk subchunk)
    {
        auto reader = subchunk.data;
        switch(subchunk.id)
        {
        case INFO_FCC:
            sample.name = read_name_from_info_entries(reader);
            break;

        case DLID_FCC:
        case GUID_FCC:
        case FACT_FCC:
        case CUE_FCC:
            break;

        case FMT_FCC:   // See WAVEFORMAT struct (mmreg.h)
        {
            if(subchunk.data.size() < 16)
            {
                throw std::runtime_error{ "DLS fmt chunk is too short" };
            }

            uint16_t temp16;
            uint32_t temp32;
            temp16 = reader.read_u16(); // read formatTag
            fmtTag = temp16;

            if(fmtTag != WAVE_FORMAT_PCM)
            {
#if !LIBSNDFILE_SUPPORT
                throw std::runtime_error
                {
                    string_format("Unsupported wave format %u (without libsndfile)", fmtTag)
                };
#endif
            }

            temp16 = reader.read_u16(); // read channels

            if(temp16 != 1)
            {
                throw std::runtime_error{ string_format("Unsupported wave channel count %u", temp16) };
            }

            temp32 = reader.read_u32(); // read sampleRate
            sample.samplerate = temp32;
            temp32 = reader.read_u32(); // read avgBytesPerSec
            temp32 = reader.read_u16(); // read blockAlign
            temp16 = reader.read_u16(); // read bitsPerSample

            if(temp16 != 8 && temp16 != 16)
            {
                throw std::runtime_error{ string_format("Unsupported wave bits per sample %u", temp16) };
            }

            bitsPerSample = temp16;
            // probably a cbSize field for WAVEFORMATEX
            break;
        }

        case WSMP_FCC:
            if(parse_wsmp(reader, sample.wsmp.emplace()) != subchunk.data.size())
            {
                throw std::runtime_error{ "DLS wsmp chunk in wave chunk has corrupted size" };
            }

            break;

        case DATA_FCC:
        {
            contains_data = true;

            if(fmtTag == 0 || bitsPerSample == 0)
            {
                throw std::runtime_error{ "DLS fmt chunk must exist in wave chunk and be prior "
                                          "to data chunk" };
            }

            if(fmtTag != WAVE_FORMAT_PCM)
            {
                break; // leave data to libsndfile
            }

            if(subchunk.data.size() % (bitsPerSample / 8) != 0)
            {
                throw std::runtime_error{ "DLS data chunk not align to bitsPerSample" };
            }

            auto samplelen = subchunk.data.size() / (bitsPerSample / 8);
            auto *destination = append_sample_frames(sample, samplelen);

            char buffer[4096];
            uint32_t remaining = subchunk.data.size();

            while(remaining > 0)
            {
                uint32_t c = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;

                reader.read(buffer, c);

                read_data_lpcm(destination, buffer, c, bitsPerSample);

                destination += c / (bitsPerSample / 8);
                remaining -= c;
            }

            break;
        }

        default:
            FLUID_LOG(FLUID_WARN,
                      "Unknown DLS chunk in LIST[wave] '" FMT_4CC_SPEC "'",
                      FMT_4CC_ARG(subchunk.id));
        }
    });


    if(!contains_data)
    {
        throw std::runtime_error{ "DLS data chunk must exist in wave chunk" };
    }

    if(fmtTag == WAVE_FORMAT_PCM)
    {
        return;
    }

#if LIBSNDFILE_SUPPORT

    try
    {
        parse_wave_sndfile(chunk, sample);
    }
    catch(...)
    {
        std::throw_with_nested(
            std::runtime_error{ "Exception thrown while parsing LIST[wave] using libsndfile" });
    }

#endif
}

inline uint32_t fluid_dls_font::parse_wsmp(fluid_dls_reader reader,
                                          fluid_dls_wsmp &wsmp)
{
    const auto size = reader.size();

    if(size < 4)
    {
        throw std::runtime_error{ "DLS wsmp chunk is too short to contain cbSize" };
    }

    uint32_t cbsize;
    cbsize = reader.read_u32();

    if(cbsize < 20)
    {
        // This is seen in Crystal's DLS. See CRS1_FCC comment.
        FLUID_LOG(FLUID_WARN, "DLS wsmp chunk cbSize < 20. The file is probably corrupted.");
        cbsize = 20;
    }

    if(cbsize > size)
    {
        throw std::runtime_error{ "DLS wsmp cbSize exceeds its chunk" };
    }

    wsmp.unity_note = reader.read_u16();
    if(wsmp.unity_note > 127)
    {
        throw std::runtime_error{ "DLS wsmp unity note is outside the MIDI note range" };
    }

    wsmp.fine_tune = reader.read_u16();
    wsmp.gain = reader.read_u32();
    reader.skip(4); // fulOptions
    uint32_t loops;
    loops = reader.read_u32();

    if(loops > 1)
    {
        throw std::runtime_error{ "DLS wsmp chunk has more than one loop" };
    }

    if(cbsize > 20)
    {
        reader.skip(cbsize - 20);
    }

    if(loops == 0)
    {
        return cbsize;
    }

    if(size - cbsize < 4)
    {
        throw std::runtime_error{ "DLS wsmp loop header exceeds its chunk" };
    }

    uint32_t loopcbsize;
    loopcbsize = reader.read_u32();

    if(loopcbsize < 16)
    {
        // This is also seen in Crystal's DLS. See CRS1_FCC comment.
        FLUID_LOG(FLUID_WARN, "DLS wsmp chunk loop cbSize < 16. The file is probably corrupted.");
        loopcbsize = 16;
    }

    if(loopcbsize > size - cbsize)
    {
        throw std::runtime_error{ "DLS wsmp loop exceeds its chunk" };
    }

    wsmp.loop_type = reader.read_u32();
    wsmp.loop_start = reader.read_u32();
    wsmp.loop_length = reader.read_u32();
    return cbsize + loopcbsize;
}

// lins

inline void fluid_dls_font::parse_lins(fluid_dls_chunk chunk)
{
    // clang-format off
    visit_subchunks(chunk, LINS_FCC,
                    [this](fluid_dls_chunk subchunk)
    {
        const auto pos = subchunk.offset;
        if(subchunk.id == CRS1_FCC || subchunk.id == CRS2_FCC)
        {
            return;
        }

        if(subchunk.id != INS_FCC)
        {
            FLUID_LOG(FLUID_WARN,
                      "Unknown DLS chunk '" FMT_4CC_SPEC
                      "' ofs=0x%llx in LIST[lins]",
                      FMT_4CC_ARG(subchunk.id),
                      pos);
            return;
        }

        auto &instrument = instruments.emplace_back();
        parse_ins(subchunk, instrument);
    }
                   );
    // clang-format on
}

inline void fluid_dls_font::parse_ins(fluid_dls_chunk chunk, fluid_dls_instrument &instrument)
{
    size_t articulation_index = -1; // in C++26 we can do std::optional<T&>
    visit_subchunks(chunk, INS_FCC, [&](fluid_dls_chunk subchunk)
    {
        const auto pos = subchunk.offset;
        auto reader = subchunk.data;
        switch(subchunk.id)
        {
        case DLID_FCC:
        case CRS1_FCC:
        case CRS2_FCC:
            break;

        case INFO_FCC:
            instrument.name = read_name_from_info_entries(reader);
            break;

        case INSH_FCC:
        {
            if(subchunk.data.size() != 12)
            {
                throw std::runtime_error{ "DLS insh chunk size != 12" };
            }

            uint32_t temp;
            temp = reader.read_u32(); // cRegions

            if(temp > chunk.data.size() / 52) // Minimum size of a region LIST, RGNH and WLNK chunks
            {
                throw std::runtime_error{ "DLS region count exceeds instrument chunk size" };
            }

            instrument.regions.reserve(temp);
            temp = reader.read_u32(); // bank
            instrument.is_drums = (temp & 0x80000000) != 0;
            instrument.bankmsb = (temp >> 8) & 0x7F;
            instrument.banklsb = temp & 0x7F;
            temp = reader.read_u32();
            instrument.pcnum = temp & 0x7F;
            break;
        }

        case LART_FCC:
        case LAR2_FCC:
            if(articulation_index == static_cast<size_t>(-1))
            {
                articulation_index = articulations.size();

                if(!parse_lart(subchunk, articulations.emplace_back()))  // bypassed by cdl
                {
                    FLUID_LOG(FLUID_DBG, "A instrument lart chunk is bypassed by cdl");
                    articulations.pop_back();
                    articulation_index = static_cast<size_t>(-1);
                }

                break;
            }

            parse_lart(subchunk, articulations[articulation_index]);
            instrument.keynum_scale = articulations[articulation_index].keynum_scale;
            break;

        case LRGN_FCC:
            parse_lrgn(subchunk, instrument);
            break;

        default:
            FLUID_LOG(FLUID_WARN,
                      "Unknown DLS chunk '" FMT_4CC_SPEC "' ofs=0x%llx in LIST[ins]",
                      FMT_4CC_ARG(subchunk.id),
                      pos);
        }
    });

    for(auto &region : instrument.regions)
    {
        if(region.artindex == static_cast<uint32_t>(-1))
        {
            region.artindex = articulation_index;
        }
    }
}

inline bool fluid_dls_font::parse_lart(fluid_dls_chunk chunk, fluid_dls_articulation &articulation)
{
    bool applicable = true;

    // DLS 2.2 section 2.6 allows CDL after ART. Check conditions before
    // modifying an articulation shared with earlier lists at this level.
    visit_subchunks(chunk, 0, [&](fluid_dls_chunk subchunk)
    {
        auto reader = subchunk.data;
        if(applicable && subchunk.id == CDL_FCC)
        {
            applicable = execute_cdls(reader);
        }
    });

    if(!applicable)
    {
        return false;
    }

    visit_subchunks(chunk, 0, [&](fluid_dls_chunk subchunk)
    {
        const auto pos = subchunk.offset;
        if(subchunk.id == CDL_FCC)
        {
            return;
        }

        if(subchunk.id != ART1_FCC && subchunk.id != ART2_FCC)
        {
            // clang-format off
            FLUID_LOG(FLUID_WARN,
                      "Unknown DLS chunk '" FMT_4CC_SPEC "' ofs=0x%llx in LIST[lart]",
                      FMT_4CC_ARG(subchunk.id), pos);
            return;
            // clang-format on
        }

        parse_art(subchunk, articulation);
    });

    return true;
}

inline void fluid_dls_font::parse_art(fluid_dls_chunk chunk, fluid_dls_articulation &articulation)
{
    auto reader = chunk.data;
    const auto offset = chunk.offset;
    const auto headersize = chunk.headersize;

    if(chunk.id != ART1_FCC && chunk.id != ART2_FCC)
    {
        // this should never happen!
        throw std::runtime_error{ "Expected 'art1' or 'art2' chunk" };
    }

    if(headersize != 8)
    {
        FLUID_LOG(FLUID_WARN, "Nonstandard LIST[art1/2] chunk; 'art1/2' is legal ofs=0x%llx", offset);
    }

    bool isArt1 = chunk.id == ART1_FCC;

    uint32_t cbsize;
    cbsize = reader.read_u32();

    if(cbsize < 8)
    {
        throw std::runtime_error{ "art chunk cbSize < 8" };
    }

    uint32_t connblocks;
    connblocks = reader.read_u32();

    if(connblocks > (std::numeric_limits<uint32_t>::max() - cbsize) / 12u)
    {
        throw std::runtime_error{ "Too many ConnectionBlocks are contained in the articulator chunk." };
    }

    if(cbsize + connblocks * 12u != chunk.data.size())
    {
        throw std::runtime_error{ "art chunk has corrupted size" };
    }

    reader.skip(cbsize - 8);

    for(uint32_t i = 0; i < connblocks; i++)
    {
        uint16_t source, control, dest, trans;
        int32_t scale;
        source = reader.read_u16();
        control = reader.read_u16();
        dest = reader.read_u16();
        trans = reader.read_u16();

        if(isArt1)
        {
            // DLS-1 uses concave transform for CC 7 and 11
            // in DLS-2 the transform is applied to source

            // DLS-2 transform: 0b00nnnn'00nnnn'0000
            trans = (trans << 10) | (trans << 4);
            auto dlsv2trans = DLSTransform{ trans };

            if(source == CONN_SRC_LFO || source == CONN_SRC_VIBRATO)
            {
                dlsv2trans.src_bip = true;
            }

            if(control == CONN_SRC_LFO || control == CONN_SRC_VIBRATO)
            {
                dlsv2trans.ctl_bip = true;
            }

            trans = static_cast<uint16_t>(dlsv2trans);
        }

        scale = reader.read_u32();
        convert_dls_connectionblock_to_art(articulation, source, control, dest, trans, scale);
    }
}

inline void fluid_dls_font::parse_lrgn(fluid_dls_chunk chunk, fluid_dls_instrument &instrument)
{
    visit_subchunks(chunk, LRGN_FCC, [&](fluid_dls_chunk subchunk)
    {
        const auto pos = subchunk.offset;
        switch(subchunk.id)
        {
        case RGN_FCC:
        case RGN2_FCC:
        {
            auto &region = instrument.regions.emplace_back();

            if(!parse_rgn(subchunk, region))  // bypassed by cdl
            {
                FLUID_LOG(FLUID_DBG, "Region ofs=0x%llx is bypassed by cdl", pos);
                instrument.regions.pop_back();
            }

            break;
        }

        case CRS1_FCC:
        case CRS2_FCC:
            // see CRS1 and CRS2_FCC's comment
            break;

        default:
            FLUID_LOG(FLUID_WARN,
                      "Unknown DLS chunk '" FMT_4CC_SPEC "' ofs=0x%llx in LIST[lrgn]",
                      FMT_4CC_ARG(subchunk.id),
                      pos);
            break;
        }
    });
}

inline bool fluid_dls_font::parse_rgn(fluid_dls_chunk chunk, fluid_dls_region &region)
{
    static int self_exclusive_class [[maybe_unused]] = 65536; // exclusive number for self-exclusive regions
    size_t articulation_index = -1;

    bool bypassed{};

    try
    {
        visit_subchunks(chunk, 0, [&](fluid_dls_chunk subchunk)
        {
            const auto pos = subchunk.offset;
            auto reader = subchunk.data;
            switch(subchunk.id)
            {
            case INFO_FCC:
            case CRS1_FCC:
            case CRS2_FCC:
                break;

            case WLNK_FCC:
                if(subchunk.data.size() < 12)
                {
                    throw std::runtime_error{ "DLS wlnk chunk is too small" };
                }

                reader.skip(8); // fluidsynth does not implement phase-locking and multichannel output
                region.sampleindex = reader.read_u32();

                if(region.sampleindex >= samples.size())
                {
                    throw std::runtime_error{ string_format("Sample index %u is out of range",
                                                            static_cast<unsigned>(region.sampleindex)) };
                }

                break;

            case CDL_FCC:
                if(!execute_cdls(reader))
                {
                    bypassed = true;
                    throw std::exception{};
                }

                break;

            case LART_FCC:
            case LAR2_FCC:
                if(articulation_index == static_cast<size_t>(-1))
                {
                    articulation_index = articulations.size();

                    if(!parse_lart(subchunk, articulations.emplace_back()))
                    {
                        FLUID_LOG(FLUID_DBG, "A region lart chunk is bypassed by cdl");
                        articulations.pop_back();
                        articulation_index = static_cast<size_t>(-1);
                    }

                    break;
                }

                parse_lart(subchunk, articulations[articulation_index]);

                if(articulations[articulation_index].keynum_scale != 1.0f)
                {
                    FLUID_LOG(FLUID_WARN, "Key Number Generator is not allowed in region articulation, ignoring");
                }

                break;

            case RGNH_FCC:
            {
                if(subchunk.data.size() < 12)
                {
                    throw std::runtime_error{ "DLS rgnh chunk is too small" };
                }

                uint16_t temp;
                temp = reader.read_u16(); // key low
                region.range.keylo = temp;
                temp = reader.read_u16(); // key high
                region.range.keyhi = temp;
                temp = reader.read_u16(); // vel low
                region.range.vello = temp;
                temp = reader.read_u16(); // vel high
                region.range.velhi = temp;
                temp = reader.read_u16();                              // fusOptions

                if((temp & F_RGN_OPTION_SELFNONEXCLUSIVE) == 0)  // self-exclusive
                {
                    // implement this flag doesn't make sense
                    // region.exclusive_class = self_exclusive_class++;
                }

                temp = reader.read_u16(); // keyGroup

                if(temp != 0)
                {
                    region.exclusive_class = temp;
                }

                // usLayer is useless
                break;
            }

            // DLS-2 1.14.6 Each region contains at minimum a <rgnh-ck> region header chunk and a <wlnk-ck> wave link chunk.
            // It may also **optionally** contain a <wsmp-ck> wave sample chunk. ...
            // DLS-2 2.2 <rgn-list> -> ... <wsmp-ck> ...
            // DLS-2 2.2 "the structure tree" ... rgn -> ... wsmp (optional) ...
            // DLS-2 2.8 Other chunks at the same nesting level include a <wsmp-ck> wave sample chunk.
            case WSMP_FCC:
                if(parse_wsmp(reader, region.wsmp.emplace()) != subchunk.data.size())
                {
                    throw std::runtime_error{ "DLS wsmp chunk in region has corrupted size" };
                }

                break;

            default:
                FLUID_LOG(FLUID_WARN,
                          "Unknown DLS chunk '" FMT_4CC_SPEC "' ofs=0x%llx in LIST[rgn]",
                          FMT_4CC_ARG(subchunk.id),
                          pos);
                break;
            }
        });
    }
    catch(...)
    {
        if(bypassed)
        {
            return false;
        }

        throw;
    }

    region.artindex = articulation_index;

    return true;
}

inline void fluid_dls_font::parse_pgal(fluid_dls_reader reader)
{
    const auto size = reader.size();
    if(size < 4 + 128 + 4)
    {
        throw std::runtime_error{ string_format("DLS pgal chunk is too small: %lld bytes", static_cast<long long>(size)) };
    }

    // drum note aliasing table
    uint32_t version;
    version = reader.read_u32();

    if(version == 0x03020100)
    {
        // there is actually no version field, this is the drum note aliasing table
        reader.seek(0);
    }

    drum_note_aliasing.emplace();

    reader.read(drum_note_aliasing->data(), 128);

    // melodic instrument aliasing table
    uint32_t nAlias;
    nAlias = reader.read_u32();

    const uint32_t alias_header_size = version == 0x03020100 ? 132u : 136u;

    if(nAlias > static_cast<uint32_t>(size - alias_header_size) / 8u)
    {
        throw std::runtime_error{ "DLS pgal alias count exceeds its chunk" };
    }

    for(uint32_t i = 0; i < nAlias; i++)
    {
        uint16_t bank;
        uint8_t pc;
        uint16_t bank2;
        uint8_t pc2;

        bank2 = reader.read_u16();
        pc2 = reader.read_u8();
        reader.skip(1);

        bank = reader.read_u16();
        pc = reader.read_u8();
        reader.skip(1);

        for(auto &instrument : instruments)
        {
            if(instrument.banklsb == (bank & 0x7F) && instrument.bankmsb == ((bank >> 7) & 0x7F) &&
                    instrument.pcnum == pc && !instrument.is_drums)
            {
                instrument.aliases.push_back(fluid_dls_instrument::fluid_dls_instrument_alias
                {
                    pc2, (bank2 >> 7) & 0x7F, bank2 & 0x7F });
                total_presets++;
                goto found_inst;
            }
        }

        FLUID_LOG(FLUID_WARN,
                  "DLS pgal aliasing from non-existing instrument at entry %u: "
                  "bank %u/%u pc %u -> bank %u/%u pc %u",
                  i,
                  (bank >> 7) & 0x7F,
                  bank & 0x7F,
                  pc,
                  (bank2 >> 7) & 0x7F,
                  bank2 & 0x7F,
                  pc2);
found_inst:
        ;
    }
}

// libsndfile
#if LIBSNDFILE_SUPPORT
static sf_count_t sfvio_get_filelen(void *user_data) noexcept
{
    auto *data = static_cast<fluid_dls_reader *>(user_data);
    return data->size();
}

static sf_count_t sfvio_seek(sf_count_t offset, int whence, void *user_data) noexcept
{
    auto *data = static_cast<fluid_dls_reader *>(user_data);

    const auto size = data->size();
    fluid_long_long_t base;
    switch(whence)
    {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = data->tell(); break;
    case SEEK_END: base = size; break;
    default: return data->tell();
    }
    // Clamp before adding, avoiding overflow for adversarial decoder seeks.
    const auto newpos = offset < -base ? 0 : offset > size - base ? size : base + offset;
    try
    {
        data->seek(newpos);
    }
    catch(...)
    {
        return data->tell();
    }
    return data->tell();
}

static sf_count_t sfvio_tell(void *user_data) noexcept
{
    auto *data = static_cast<fluid_dls_reader *>(user_data);
    return data->tell();
}

static sf_count_t sfvio_read(void *buffer, sf_count_t count, void *user_data) noexcept
{
    auto *data = static_cast<fluid_dls_reader *>(user_data);

    const auto pos = data->tell();
    count = std::min(count, static_cast<sf_count_t>(data->remaining()));
    if(count <= 0)
    {
        return 0;
    }
    try
    {
        data->read(buffer, count);
    }
    catch(...)
    {
        return 0;
    }

    // here is a hack to trick libsndfile to recognize DLS LIST[wave] as Microsoft WAVE (.wav)
    // the only difference is the header
    // origin: LISTnnnnwave...
    // wave  : RIFFnnnnWAVE...

    const char RIFF[4] = { 'R', 'I', 'F', 'F' };
    const char WAVE[4] = { 'W', 'A', 'V', 'E' };

    if(pos < 4)
    {
        memcpy(buffer, RIFF + pos, std::min(4LL - pos, static_cast<long long>(count)));
    }

    if(pos < 12 && pos + count > 8)
    {
        memcpy(static_cast<char *>(buffer) + std::max(0LL, 8 - pos),
               WAVE + std::max(0LL, pos - 8),
               std::min({ 4LL, count - std::max(0LL, 8 - pos), 12 - pos }));
    }

    return count;
}

static SF_VIRTUAL_IO sfvio = { sfvio_get_filelen, sfvio_seek, sfvio_read, nullptr, sfvio_tell };

inline void fluid_dls_font::parse_wave_sndfile(fluid_dls_chunk chunk, fluid_dls_sample &sample)
{
    auto data = chunk.encoded;

    SF_INFO sfinfo{};
    auto *sndfile = sf_open_virtual(&sfvio, SFM_READ, &sfinfo, &data);

    if(sndfile == nullptr)
    {
        throw std::runtime_error{ string_format("Failed to open 'wave' chunk using libsndfile: %s",
                                                sf_strerror(sndfile)) };
    }

    scope_guard close_sndfile{ [sndfile]() { sf_close(sndfile); } };
    sample.samplerate = sfinfo.samplerate;

    if(sfinfo.channels != 1)
    {
        throw std::runtime_error{ string_format("Unsupported wave channel count: %d (libsndfile)",
                                                sfinfo.channels) };
    }

    auto *destination = append_sample_frames(sample, sfinfo.frames);
    const auto count = sfinfo.frames == 0 ? 0 : sf_read_short(sndfile, destination, sfinfo.frames);

    if(count != sfinfo.frames)
    {
        FLUID_LOG(FLUID_WARN, "Read 'wave' using libsndfile reached unexpected EOF");
    }

}
#endif

// fluid sfloader interfaces implementation

struct fluid_dls_loader_data
{
    fluid_settings_t *settings;
    fluid_synth_t *synth;
};

fluid_sfloader_t *new_fluid_dls_loader(fluid_synth_t *synth, fluid_settings_t *settings)
{
    fluid_return_val_if_fail(settings != nullptr, nullptr);

    fluid_sfloader_t *loader = new_fluid_sfloader(fluid_dls_loader_load, fluid_dls_loader_delete);

    if(loader == nullptr)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        return nullptr;
    }

    auto *data = new fluid_dls_loader_data;
    data->settings = settings;
    data->synth = synth;
    fluid_sfloader_set_data(loader, data);

    return loader;
}

void fluid_dls_loader_delete(fluid_sfloader_t *loader) noexcept
{
    delete static_cast<fluid_dls_loader_data *>(fluid_sfloader_get_data(loader));
    delete_fluid_sfloader(loader);
}

static fluid_mod_t *fluid_dls_default_mod_list()
{
    std::array def_mods =   // skip default_vel2filter_mod
    {
        // skip default_at2viblfo_mod
        &default_vel2att_mod,
        &default_mod2viblfo_mod,
        &default_att_mod,
        &default_pan_mod,
        &default_expr_mod,
        &DLS_default_reverb_mod,
        &DLS_default_chorus_mod,
        &DLS_default_pitch_bend_mod,
        &custom_balance_mod
    };

    fluid_mod_t *list = nullptr;

    for(unsigned int i = 0; i < def_mods.size(); i++)
    {
        fluid_mod_t *mod = new_fluid_mod();

        if(mod == nullptr)
        {
            delete_fluid_list_mod(list);
            throw std::bad_alloc();
        }

        fluid_mod_clone(mod, def_mods[i]);
        mod->next = list;
        list = mod;
    }

    return list;
}

static fluid_sfont_t *fluid_dls_loader_load(fluid_sfloader_t *loader, const char *filename) noexcept
{
    auto *sfont = new_fluid_sfont(fluid_dls_sfont_get_name,
                                  fluid_dls_sfont_get_preset,
                                  fluid_dls_iteration_start,
                                  fluid_dls_iteration_next,
                                  fluid_dls_sfont_delete);

    if(sfont == nullptr)
    {
        return nullptr;
    }

    sfont->default_mod_list = fluid_dls_default_mod_list();

    uint32_t sample_rate = 44100;
    bool try_mlock = false;
    auto *sfloader_data = static_cast<fluid_dls_loader_data *>(fluid_sfloader_get_data(loader));
    auto *settings = sfloader_data->settings;

    if(settings != nullptr)
    {
        double rate{};

        if(fluid_settings_getnum(settings, "synth.sample-rate", &rate) == FLUID_OK)
        {
            sample_rate = static_cast<uint32_t>(rate);
        }

        int mlock{};

        if(fluid_settings_getint(settings, "synth.lock-memory", &mlock) == FLUID_OK)
        {
            try_mlock = mlock != 0;
        }
    }

    auto *dlsfont =
        new_fluid_dls_font(sfloader_data->synth, sfont, &loader->file_callbacks, filename, sample_rate, try_mlock);

    if(dlsfont == nullptr)
    {
        delete_fluid_sfont(sfont);
        return nullptr;
    }

    fluid_sfont_set_data(sfont, dlsfont);
    return sfont;
}

// fluid_sfont_t interface
static const char *fluid_dls_sfont_get_name(fluid_sfont_t *sfont) noexcept
{
    return static_cast<const fluid_dls_font *>(fluid_sfont_get_data(sfont))->filename.c_str();
}

static fluid_preset_t *fluid_dls_sfont_get_preset(fluid_sfont_t *sfont, int bank, int prenum) noexcept
{
    auto *dlsfont = static_cast<fluid_dls_font *>(fluid_sfont_get_data(sfont));

    for(auto &inst : dlsfont->instruments_fluid_data)
    {
        if(fluid_dls_preset_get_banknum(&inst.fluid) == bank && inst.pcnum == prenum)
        {
            return &inst.fluid;
        }
    }

    if(dlsfont->synth->bank_select == FLUID_BANK_STYLE_MMA)
    {
        for(auto &inst : dlsfont->instruments_fluid_data)
        {
            // Drum bank fallback for MMA bank style
            if(bank == DRUM_INST_BANK && inst.is_drums && inst.pcnum == prenum)
            {
                return &inst.fluid;
            }
        }
    }

    if(dlsfont->synth->bank_select == FLUID_BANK_STYLE_GM)
    {
        for(auto &inst : dlsfont->instruments_fluid_data)
        {
            if(bank == 0 && !inst.is_drums && inst.bankmsb == 0x79 && inst.pcnum == prenum)
            {
                return &inst.fluid;
            }
        }
    }

    return nullptr;
}

static void fluid_dls_iteration_start(fluid_sfont_t *sfont) noexcept
{
    auto *dlsfont = static_cast<fluid_dls_font *>(fluid_sfont_get_data(sfont));
    dlsfont->fluid_preset_iterator = dlsfont->instruments_fluid_data.begin();
}

static fluid_preset_t *fluid_dls_iteration_next(fluid_sfont_t *sfont) noexcept
{
    auto *dlsfont = static_cast<fluid_dls_font *>(fluid_sfont_get_data(sfont));

    if(dlsfont->fluid_preset_iterator == dlsfont->instruments_fluid_data.end())
    {
        return nullptr;
    }

    return &(*(dlsfont->fluid_preset_iterator++)).fluid;
}

static int fluid_dls_sfont_delete(fluid_sfont_t *sfont) noexcept
{
    auto *dlsfont = static_cast<fluid_dls_font *>(fluid_sfont_get_data(sfont));

    /* Check that no samples are currently used */
    for (const auto& sample : dlsfont->samples_fluid)
    {
        if (sample.refcount != 0)
        {
            return FLUID_FAILED;
        }
    }

    delete_fluid_dls_font(dlsfont);
    delete_fluid_sfont(sfont);

    return FLUID_OK;
}

static const char *fluid_dls_preset_get_name(fluid_preset_t *preset) noexcept
{
    return static_cast<const fluid_dls_instrument_fluid_data *>(fluid_preset_get_data(preset))
           ->instrument->name.c_str();
}

static int fluid_dls_preset_get_banknum(fluid_preset_t *preset) noexcept
{
    const auto *inst = static_cast<const fluid_dls_instrument_fluid_data *>(fluid_preset_get_data(preset));
    const auto *synth = inst->instrument->synth;

    // see fluid_synth.h enum fluid_midi_bank_select
    // see fluid_chan.c fluid_channel_set_bank_msb()
    if(synth->bank_select == FLUID_BANK_STYLE_GM)
    {
        if(inst->is_drums || inst->bankmsb == 0x78)  // GM2 rhythm bank
        {
            return DRUM_INST_BANK;
        }

        return inst->bankmsb; // to always match bank 0 for GM
    }

    if(synth->bank_select == FLUID_BANK_STYLE_GS)
    {
        return inst->bankmsb + (inst->is_drums ? DRUM_INST_BANK : 0);
    }

    if(synth->bank_select == FLUID_BANK_STYLE_XG ||
            synth->bank_select == FLUID_BANK_STYLE_GM2)
    {
        if(inst->is_drums)
        {
            // see https://github.com/FluidSynth/fluidsynth/issues/1524
            // see fluid_chan.c fluid_channel_set_bank_msb()
            return 128;
        }

        return inst->banklsb;
    }

    // if (synth->bank_select == FLUID_BANK_STYLE_MMA)
    if(inst->is_drums)
    {
        return DRUM_INST_BANK * 128;
    }

    return inst->banklsb + (128 * inst->bankmsb);
}

static int fluid_dls_preset_get_num(fluid_preset_t *preset) noexcept
{
    return static_cast<const fluid_dls_instrument_fluid_data *>(fluid_preset_get_data(preset))->pcnum;
}

static int fluid_dls_preset_noteon(fluid_preset_t *preset, fluid_synth_t *synth, int chan, int key, int vel) noexcept
{
    auto *dlspreset = static_cast<fluid_dls_instrument_fluid_data *>(fluid_preset_get_data(preset))->instrument;

    // Things affect region selection process:
    // 1. Subtonal tuning, see https://github.com/FluidSynth/fluidsynth/issues/926
    // 2. Key Number Generator
    fluid_real_t tuned_key_f = key;

    // this is copied from fluid_defsfont
    if(synth->channel[chan]->channel_type == CHANNEL_TYPE_MELODIC)
    {
        tuned_key_f = fluid_channel_get_key_pitch(synth->channel[chan], key) / 100.0f;
    }

    tuned_key_f *= dlspreset->keynum_scale;

    // key with subtonal tuning and key number generator applied
    int tuned_key;
    if(!std::isfinite(tuned_key_f) || tuned_key_f <= std::numeric_limits<int>::min())
    {
        tuned_key = std::numeric_limits<int>::min();
    }
    else if(tuned_key_f >= std::numeric_limits<int>::max())
    {
        tuned_key = std::numeric_limits<int>::max();
    }
    else
    {
        tuned_key = static_cast<int>(std::round(tuned_key_f));
    }
    // key with only key number generator applied
    int adjusted_key;
    if (synth->channel[chan]->channel_type == CHANNEL_TYPE_DRUM)
    {
        if(dlspreset->drum_note_aliasing != nullptr)
        {
            tuned_key = dlspreset->drum_note_aliasing[std::clamp(tuned_key, 0, 127)];
        }
        // drum channel has no subtonal tuning, so adjusted_key == tuned_Key
        adjusted_key = tuned_key;
    }
    else
    {
        adjusted_key = static_cast<int>(std::round(key * dlspreset->keynum_scale));
    }
    adjusted_key = std::clamp(adjusted_key, 0, 127);

    for(auto &region : dlspreset->regions)
    {
        auto *sample = dlspreset->samples_fluid + region.sampleindex;
        // Check for zero-length samples, typically caused by samples that failed fluid_sample_validate()
        if(!fluid_zone_inside_range(&region.range, tuned_key, vel) || sample->start == sample->end)
        {
            continue;
        }

        auto *voice = fluid_synth_alloc_voice_LOCAL(
                          synth, sample, chan, adjusted_key, vel, &region.range);

        if(voice == nullptr)
        {
            return FLUID_FAILED;
        }

        if(region.artindex != static_cast<uint32_t>(-1))
        {
            auto &art = dlspreset->articulations[region.artindex];

            for(int i = 0; i < GEN_LAST; i++)
            {
                if(art.gens[i].has_value())
                {
                    fluid_voice_gen_set(voice, i, art.gens[i].value());
                }
                fluid_real_t awe_val;
                /* ...unless the default value has been overridden by an AWE32 NRPN */
                if (FLUID_UNLIKELY(fluid_channel_get_override_gen_default(synth->channel[chan], i, &awe_val)))
                {
                    fluid_voice_gen_set(voice, i, awe_val);
                }
            }

            // this should be the count of default mods to be probably overwritten
            auto existing_mod_count = voice->mod_count;

            for(auto &mod : art.mods)
            {
                fluid_voice_add_mod_local(voice, &mod, FLUID_VOICE_OVERWRITE, existing_mod_count);
            }

            // See also https://github.com/FluidSynth/fluidsynth/pull/1626 conversation for "Key Number to Pitch" articulation implementation
            fluid_voice_gen_set(voice,
                                GEN_SCALETUNE,
                                dlspreset->articulations[region.artindex].keyToPitch.amount / 128.0);
        }

        if(region.wsmp.has_value())
        {
            const auto &wsmp = region.wsmp.value();
            const auto &sample = dlspreset->samples_fluid[region.sampleindex];

            fluid_voice_gen_set(voice, GEN_OVERRIDEROOTKEY, wsmp.unity_note);
            fluid_voice_gen_incr(voice, GEN_FINETUNE, wsmp.fine_tune - sample.pitchadj);
            fluid_voice_gen_incr(voice, GEN_ATTENUATION, -static_cast<fluid_real_t>(wsmp.gain) / 65536.0f);

            if(wsmp.loop_length != 0)
            {
                if(wsmp.loop_type == 0)
                {
                    fluid_voice_gen_set(voice, GEN_SAMPLEMODE, 1);
                }
                else if(wsmp.loop_type == 1)
                {
                    fluid_voice_gen_set(voice, GEN_SAMPLEMODE, 3);
                }
                else
                {
                    FLUID_LOG(FLUID_WARN, "invalid loop type of region wsmp, set to loop and release");
                    fluid_voice_gen_set(voice, GEN_SAMPLEMODE, 3);
                }
            }
            else
            {
                fluid_voice_gen_set(voice, GEN_SAMPLEMODE, 0);
            }

            fluid_voice_gen_set(voice,
                                GEN_STARTLOOPADDROFS,
                                static_cast<int>(sample.start + wsmp.loop_start) -
                                static_cast<int>(sample.loopstart));
            fluid_voice_gen_set(voice,
                                GEN_ENDLOOPADDROFS,
                                static_cast<int>(sample.start + wsmp.loop_start + wsmp.loop_length) -
                                static_cast<int>(sample.loopend));
        }
        else
        {
            fluid_voice_gen_incr(voice, GEN_ATTENUATION, -region.gain_inherited);
            fluid_voice_gen_set(voice, GEN_SAMPLEMODE, region.samplemode_inherited);
        }

        fluid_voice_gen_set(voice, GEN_EXCLUSIVECLASS, region.exclusive_class);

        fluid_synth_start_voice(synth, voice);
    }

    return FLUID_OK;
}

static void fluid_dls_preset_free(fluid_preset_t *preset [[maybe_unused]]) noexcept
{
    // do nothing. presets are under RAII of fluid_dls_font
}
