#include "parquet_reader.h"
#include <parquet/arrow/reader.h>
#include <arrow/io/file.h>
#include <arrow/array.h>
#include <stdexcept>

std::unique_ptr<ParquetData> ParquetReader::read(const std::string& filename) {
    auto data = std::make_unique<ParquetData>();

    auto file_reader = parquet::arrow::FileReader::Make(
        arrow::io::ReadableFile::Open(filename).ValueOrDie().ValueOrDie(),
        arrow::default_memory_pool()
    ).ValueOrDie();

    std::shared_ptr<arrow::Table> table;
    file_reader->ReadTable(&table);

    auto get_double_column = [&table](const std::string& name) -> std::vector<double> {
        auto column = table->GetColumnByName(name);
        std::vector<double> result;
        for (int i = 0; i < column->num_chunks(); ++i) {
            auto chunk = std::dynamic_pointer_cast<arrow::DoubleArray>(column->chunk(i));
            if (!chunk) throw std::runtime_error("Column " + name + " is not double type");
            for (int64_t j = 0; j < chunk->length(); ++j) {
                result.push_back(chunk->Value(j));
            }
        }
        return result;
    };

    auto get_int_column = [&table](const std::string& name) -> std::vector<int> {
        auto column = table->GetColumnByName(name);
        std::vector<int> result;
        for (int i = 0; i < column->num_chunks(); ++i) {
            auto chunk = std::dynamic_pointer_cast<arrow::Int64Array>(column->chunk(i));
            if (!chunk) throw std::runtime_error("Column " + name + " is not int type");
            for (int64_t j = 0; j < chunk->length(); ++j) {
                result.push_back(static_cast<int>(chunk->Value(j)));
            }
        }
        return result;
    };

    data->raw_idx = get_double_column("raw_idx");
    data->ecg1_rotated = get_double_column("ecg1_rotated");
    data->puls_raw = get_double_column("puls_raw");
    data->target = get_int_column("target");

    return data;
}
