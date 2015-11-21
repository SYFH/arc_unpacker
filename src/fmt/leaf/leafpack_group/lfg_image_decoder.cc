#include "fmt/leaf/leafpack_group/lfg_image_decoder.h"
#include "fmt/leaf/common/custom_lzss.h"
#include "util/cyclic_buffer.h"
#include "err.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::leaf;

static const bstr magic = "LEAFCODE"_b;

static const bstr lame_permutation =
    "\x00\x01\x10\x11\x02\x03\x12\x13\x20\x21\x30\x31\x22\x23\x32\x33"
    "\x04\x05\x14\x15\x06\x07\x16\x17\x24\x25\x34\x35\x26\x27\x36\x37"
    "\x40\x41\x50\x51\x42\x43\x52\x53\x60\x61\x70\x71\x62\x63\x72\x73"
    "\x44\x45\x54\x55\x46\x47\x56\x57\x64\x65\x74\x75\x66\x67\x76\x77"
    "\x08\x09\x18\x19\x0A\x0B\x1A\x1B\x28\x29\x38\x39\x2A\x2B\x3A\x3B"
    "\x0C\x0D\x1C\x1D\x0E\x0F\x1E\x1F\x2C\x2D\x3C\x3D\x2E\x2F\x3E\x3F"
    "\x48\x49\x58\x59\x4A\x4B\x5A\x5B\x68\x69\x78\x79\x6A\x6B\x7A\x7B"
    "\x4C\x4D\x5C\x5D\x4E\x4F\x5E\x5F\x6C\x6D\x7C\x7D\x6E\x6F\x7E\x7F"
    "\x80\x81\x90\x91\x82\x83\x92\x93\xA0\xA1\xB0\xB1\xA2\xA3\xB2\xB3"
    "\x84\x85\x94\x95\x86\x87\x96\x97\xA4\xA5\xB4\xB5\xA6\xA7\xB6\xB7"
    "\xC0\xC1\xD0\xD1\xC2\xC3\xD2\xD3\xE0\xE1\xF0\xF1\xE2\xE3\xF2\xF3"
    "\xC4\xC5\xD4\xD5\xC6\xC7\xD6\xD7\xE4\xE5\xF4\xF5\xE6\xE7\xF6\xF7"
    "\x88\x89\x98\x99\x8A\x8B\x9A\x9B\xA8\xA9\xB8\xB9\xAA\xAB\xBA\xBB"
    "\x8C\x8D\x9C\x9D\x8E\x8F\x9E\x9F\xAC\xAD\xBC\xBD\xAE\xAF\xBE\xBF"
    "\xC8\xC9\xD8\xD9\xCA\xCB\xDA\xDB\xE8\xE9\xF8\xF9\xEA\xEB\xFA\xFB"
    "\xCC\xCD\xDC\xDD\xCE\xCF\xDE\xDF\xEC\xED\xFC\xFD\xEE\xEF\xFE\xFF"_b;

static void move_output_ptr(
    const bool horizontal,
    const size_t width,
    const size_t height,
    u8 *&output_ptr,
    size_t &x,
    size_t &y)
{
    if (horizontal)
    {
        output_ptr += 2;
        x -= 2;
        if (x <= 0)
        {
            output_ptr -= width * 2;
            y--;
            x = width;
        }
    }
    else
    {
        output_ptr -= width;
        y--;
        if (!y)
        {
            output_ptr += width * height + 2;
            x -= 2;
            y = height;
        }
    }
}

bool LfgImageDecoder::is_recognized_impl(File &input_file) const
{
    return input_file.stream.read(magic.size()) == magic;
}

pix::Grid LfgImageDecoder::decode_impl(File &input_file) const
{
    const auto width
        = (input_file.stream.seek(36).read_u16_le()
        - input_file.stream.seek(32).read_u16_le() + 1) * 8;

    const auto height
        = input_file.stream.seek(38).read_u16_le()
        - input_file.stream.seek(34).read_u16_le() + 1;

    const bool horizontal = input_file.stream.seek(40).read_u8() > 0;
    const auto base_color = input_file.stream.seek(41).read_u8();
    const auto size_orig = input_file.stream.seek(44).read_u32_le();
    const auto input = input_file.stream.seek(48).read_to_eof();

    bstr data(size_orig * 2, base_color);
    auto output_ptr = data.get<u8>() + width * (height - 1);
    auto input_ptr = input.get<const u8>();
    const auto output_start = data.get<const u8>();
    const auto output_end = data.end<const u8>();
    const auto input_end = input.end<const u8>();

    size_t x = width;
    size_t y = height;

    // heavily modified LZSS
    util::CyclicBuffer<0x1000> dict(0xFEE);
    u16 control = 0;
    while (output_ptr >= output_start
        && output_ptr < output_end
        && input_ptr < input_end)
    {
        control <<= 1;
        if (!(control & 0x80))
            control = (*input_ptr++ << 8) | 0xFF;

        if ((control >> 15) & 1)
        {
            const auto tmp = lame_permutation[*input_ptr++];
            dict << tmp;
            output_ptr[0] = tmp >> 4;
            output_ptr[1] = tmp & 0xF;
            move_output_ptr(horizontal, width, height, output_ptr, x, y);
        }
        else
        {
            if (input_ptr + 2 > input_end)
                break;
            const auto tmp = *reinterpret_cast<const u16*>(input_ptr);
            input_ptr += 2;
            u16 look_behind_pos = tmp >> 4;
            u16 repetitions = (tmp & 0xF) + 3;
            while (repetitions--
                && output_ptr >= output_start
                && output_ptr < output_end)
            {
                const auto tmp = dict[look_behind_pos++];
                dict << tmp;
                output_ptr[0] = tmp >> 4;
                output_ptr[1] = tmp & 0xF;
                move_output_ptr(horizontal, width, height, output_ptr, x, y);
            }
        }
    }

    input_file.stream.seek(magic.size());
    bstr palette_data;
    for (const auto i : util::range(24))
    {
        const auto tmp = input_file.stream.read_u8();
        palette_data += static_cast<u8>(tmp & 0xF0);
        palette_data += static_cast<u8>(tmp << 4);
    }
    pix::Palette palette(16, palette_data, pix::Format::RGB888);
    pix::Grid image(width, height, data, palette);
    image.flip_vertically();
    return image;
}

static auto dummy = fmt::register_fmt<LfgImageDecoder>("leaf/lfg");
