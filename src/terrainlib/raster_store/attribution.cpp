#include "raster_store/attribution.h"

#include <array>
#include <limits>
#include <system_error>

#include <cpl_json.h>

#include "io/bytes.h"

namespace raster_store::attribution {

Expected<const Entity*> Table::at(const std::uint32_t index) const
{
    if (index >= index_limit) {
        return Error::fail(Error::Code::Unsupported, "attribution index must be less than 65535");
    }
    if (index >= entities.size()) {
        return Error::fail(Error::Code::InvalidInput, "attribution index is outside the source table");
    }
    return &entities[index];
}

Expected<Table> parse(const std::string_view json)
{
    if (json.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return Error::fail(Error::Code::ResourceExhausted, "attribution JSON exceeds parser capacity");
    }
    CPLJSONDocument document;
    if (!document.LoadMemory(reinterpret_cast<const GByte*>(json.data()), static_cast<int>(json.size()))) {
        return Error::fail(Error::Code::CorruptData, "invalid attribution JSON");
    }
    const auto root = document.GetRoot();
    if (root.GetType() != CPLJSONObject::Type::Array) {
        return Error::fail(Error::Code::CorruptData, "attribution table must be a JSON array");
    }
    const auto array = root.ToArray();
    if (array.Size() == 0) {
        return Error::fail(Error::Code::CorruptData, "attribution table must include slot 0");
    }
    if (static_cast<std::uint32_t>(array.Size()) > index_limit) {
        return Error::fail(Error::Code::Unsupported, "attribution table contains unsupported indices");
    }
    Table table;
    table.entities.reserve(static_cast<std::size_t>(array.Size()));
    for (int index = 0; index < array.Size(); ++index) {
        const auto object = array[index];
        if (object.GetType() != CPLJSONObject::Type::Object) {
            return Error::fail(Error::Code::CorruptData, "attribution slot " + std::to_string(index) + " must be an object");
        }
        const auto resolution_type = object["spatial_resolution"].GetType();
        if (resolution_type != CPLJSONObject::Type::Double && resolution_type != CPLJSONObject::Type::Integer
            && resolution_type != CPLJSONObject::Type::Long) {
            return Error::fail(Error::Code::CorruptData, "attribution spatial_resolution must be numeric");
        }
        constexpr std::array fields { "acquisition_date", "ingestion_date", "copyright", "copyright_link", "license" };
        for (const auto field : fields) {
            if (object[field].GetType() != CPLJSONObject::Type::String) {
                return Error::fail(Error::Code::CorruptData, "attribution field " + std::string(field) + " must be a string");
            }
        }
        table.entities.push_back({ object.GetDouble("spatial_resolution"), object.GetString("acquisition_date"),
            object.GetString("ingestion_date"), object.GetString("copyright"), object.GetString("copyright_link"), object.GetString("license") });
    }
    return table;
}

Expected<std::filesystem::path> find_table(const std::filesystem::path& index_path)
{
    std::error_code error;
    auto directory = std::filesystem::absolute(index_path, error).parent_path();
    if (error) {
        return Error::fail(Error::Code::Io, "resolve attribution index path", index_path, error);
    }
    for (int level = 0; level < 3; ++level) {
        const auto path = directory / file_name;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            return Error::fail(Error::Code::Io, "inspect attribution table", path, error);
        }
        if (std::filesystem::exists(status)) {
            return path;
        }
        directory = directory.parent_path();
    }
    return Error::fail(Error::Code::NotFound, "find attribution table beside index or in its two ancestor directories", index_path);
}

Expected<Table> read_table(const std::filesystem::path& index_path)
{
    auto path = find_table(index_path);
    if (!path) {
        return Error::propagate(std::move(path));
    }
    auto bytes = ::io::read_bytes_from_path(*path);
    if (!bytes) {
        return Error::propagate(std::move(bytes), "read source attribution table");
    }
    auto table = parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (!table) {
        return Error::propagate(std::move(table), "parse source attribution table \"" + path->string() + "\"");
    }
    return table;
}

Expected<void> copy_table(const std::filesystem::path& source_index_path, const std::filesystem::path& destination_directory)
{
    auto source = find_table(source_index_path);
    if (!source) {
        return Error::propagate(std::move(source));
    }
    const auto destination = destination_directory / file_name;
    std::error_code error;
    if (!std::filesystem::copy_file(*source, destination, std::filesystem::copy_options::none, error)) {
        if (error == std::errc::no_such_file_or_directory) {
            return Error::fail(Error::Code::NotFound, "copy source attribution table", *source, destination, error);
        }
        return Error::fail(error == std::errc::file_exists ? Error::Code::AlreadyExists : Error::Code::Io,
            "copy source attribution table", *source, destination, error);
    }
    return {};
}

} // namespace raster_store::attribution
