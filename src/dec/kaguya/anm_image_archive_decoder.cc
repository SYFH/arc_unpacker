#include "dec/kaguya/anm_image_archive_decoder.h"
#include "algo/range.h"
#include "enc/png/png_image_encoder.h"
#include "err.h"
#include "io/memory_stream.h"

using namespace au;
using namespace au::dec::kaguya;

static const bstr magic0 = "AN00"_b;
static const bstr magic1 = "AN10"_b;
static const bstr magic2 = "AN20"_b;

namespace
{
    struct ArchiveEntryImpl final : dec::ArchiveEntry
    {
        size_t offset;
        size_t size;
        size_t width;
        size_t height;
        size_t channels;
        std::unique_ptr<res::Image> mask;
    };
}

algo::NamingStrategy AnmImageArchiveDecoder::naming_strategy() const
{
    return algo::NamingStrategy::Sibling;
}

bool AnmImageArchiveDecoder::is_recognized_impl(io::File &input_file) const
{
    return input_file.stream.seek(0).read(magic0.size()) == magic0
        || input_file.stream.seek(0).read(magic1.size()) == magic1
        || input_file.stream.seek(0).read(magic2.size()) == magic2;
}

std::unique_ptr<dec::ArchiveMeta> AnmImageArchiveDecoder::read_meta_impl(
    const Logger &logger, io::File &input_file) const
{
    input_file.stream.seek(2);
    const auto major_version = input_file.stream.read<u8>() - '0';
    const auto minor_version = input_file.stream.read<u8>() - '0';

    if (major_version == 0 || major_version == 1)
    {
        const auto x = input_file.stream.read_le<u32>();
        const auto y = input_file.stream.read_le<u32>();
        const auto width = input_file.stream.read_le<u32>();
        const auto height = input_file.stream.read_le<u32>();
        const auto frame_count = input_file.stream.read_le<u16>();
        input_file.stream.skip(2);
        input_file.stream.skip(frame_count * 4);
        const auto file_count = input_file.stream.read_le<u16>();
        auto meta = std::make_unique<dec::ArchiveMeta>();
        for (const auto i : algo::range(file_count))
        {
            auto entry = std::make_unique<ArchiveEntryImpl>();
            input_file.stream.skip(8);
            entry->width = input_file.stream.read_le<u32>();
            entry->height = input_file.stream.read_le<u32>();
            entry->channels = major_version == 0
                ? 4
                : input_file.stream.read_le<u32>();
            entry->offset = input_file.stream.tell();
            entry->size = entry->channels * entry->width * entry->height;
            input_file.stream.skip(entry->size);
            meta->entries.push_back(std::move(entry));
        }
        return meta;
    }

    else if (major_version == 2)
    {
        const auto unk_count = input_file.stream.read_le<u16>();
        input_file.stream.skip(2);
        for (const auto i : algo::range(unk_count))
        {
            const auto control = input_file.stream.read<u8>();
            if (control == 0) continue;
            else if (control == 1) input_file.stream.skip(8);
            else if (control == 2) input_file.stream.skip(4);
            else if (control == 3) input_file.stream.skip(4);
            else if (control == 4) input_file.stream.skip(4);
            else if (control == 5) input_file.stream.skip(4);
            else throw err::NotSupportedError("Unsupported control");
        }

        input_file.stream.skip(2);
        auto meta = std::make_unique<dec::ArchiveMeta>();
        const auto file_count = input_file.stream.read_le<u16>();
        if (!file_count)
            return meta;
        const auto x = input_file.stream.read_le<u32>();
        const auto y = input_file.stream.read_le<u32>();
        const auto width = input_file.stream.read_le<u32>();
        const auto height = input_file.stream.read_le<u32>();
        for (const auto i : algo::range(file_count))
        {
            auto entry = std::make_unique<ArchiveEntryImpl>();
            input_file.stream.skip(8);
            entry->width = input_file.stream.read_le<u32>();
            entry->height = input_file.stream.read_le<u32>();
            entry->channels = input_file.stream.read_le<u32>();
            entry->offset = input_file.stream.tell();
            entry->size = entry->channels * entry->width * entry->height;
            input_file.stream.skip(entry->size);
            meta->entries.push_back(std::move(entry));
        }
        return meta;
    }

    throw err::UnsupportedVersionError(major_version);
}

std::unique_ptr<io::File> AnmImageArchiveDecoder::read_file_impl(
    const Logger &logger,
    io::File &input_file,
    const dec::ArchiveMeta &m,
    const dec::ArchiveEntry &e) const
{
    const auto entry = static_cast<const ArchiveEntryImpl*>(&e);
    res::Image image(
        entry->width,
        entry->height,
        input_file.stream.seek(entry->offset).read(entry->size),
        entry->channels == 3
            ? res::PixelFormat::BGR888
            : res::PixelFormat::BGRA8888);
    image.flip_vertically();
    return enc::png::PngImageEncoder().encode(logger, image, entry->path);
}

static auto _ = dec::register_decoder<AnmImageArchiveDecoder>("kaguya/anm");
