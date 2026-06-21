#include "parquet_reader.h"
#include <parquet/arrow/reader.h>
#include <arrow/io/file.h>
#include <arrow/array.h>
#include <arrow/table.h>
#include <stdexcept>

static std::vector<double> get_double_column(const std::shared_ptr<arrow::Table>& table, const std::string& name) {
    auto column = table->GetColumnByName(name);
    if (!column) throw std::runtime_error("Column " + name + " not found");

    std::vector<double> result;
    for (int i = 0; i < column->num_chunks(); ++i) {
        auto chunk = column->chunk(i);

        // Try different numeric types
        if (auto arr = std::dynamic_pointer_cast<arrow::DoubleArray>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(arr->Value(j));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::FloatArray>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<double>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::Int64Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<double>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::Int32Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<double>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::Int16Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<double>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::UInt64Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<double>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::UInt32Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<double>(arr->Value(j)));
            }
        } else {
            throw std::runtime_error("Column " + name + " has unsupported numeric type");
        }
    }
    return result;
}

static std::vector<int> get_int_column(const std::shared_ptr<arrow::Table>& table, const std::string& name) {
    auto column = table->GetColumnByName(name);
    if (!column) throw std::runtime_error("Column " + name + " not found");

    std::vector<int> result;
    for (int i = 0; i < column->num_chunks(); ++i) {
        auto chunk = column->chunk(i);

        // Try different integer types
        if (auto arr = std::dynamic_pointer_cast<arrow::Int64Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<int>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::Int32Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<int>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::Int16Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<int>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::UInt32Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<int>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::UInt16Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<int>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::UInt8Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<int>(arr->Value(j)));
            }
        } else if (auto arr = std::dynamic_pointer_cast<arrow::Int8Array>(chunk)) {
            for (int64_t j = 0; j < arr->length(); ++j) {
                result.push_back(static_cast<int>(arr->Value(j)));
            }
        } else {
            throw std::runtime_error("Column " + name + " has unsupported integer type");
        }
    }
    return result;
}

std::unique_ptr<ParquetData> ParquetReader::read(const std::string& filename) {
    auto data = std::make_unique<ParquetData>();

    auto infile_result = arrow::io::ReadableFile::Open(filename);
    if (!infile_result.ok()) {
        throw std::runtime_error("Failed to open file: " + infile_result.status().ToString());
    }
    auto infile = infile_result.ValueOrDie();

    auto reader_result = parquet::arrow::OpenFile(infile, arrow::default_memory_pool());
    if (!reader_result.ok()) {
        throw std::runtime_error("Failed to open parquet file: " + reader_result.status().ToString());
    }
    auto& reader = reader_result.ValueOrDie();

    std::shared_ptr<arrow::Table> table;
    auto status = reader->ReadTable(&table);
    if (!status.ok()) {
        throw std::runtime_error("Failed to read table: " + status.ToString());
    }

    data->raw_idx = get_double_column(table, "raw_idx");
    data->ecg1_rotated = get_double_column(table, "ecg1_rotated");
    data->puls_raw = get_double_column(table, "puls_raw");
    data->target = get_int_column(table, "target");

    return data;
}
