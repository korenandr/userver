#pragma once

#include <storages/postgres/exceptions.hpp>
#include <storages/postgres/io/buffer_io.hpp>
#include <storages/postgres/io/integral_types.hpp>
#include <storages/postgres/io/nullable_traits.hpp>

namespace storages::postgres::io {

inline constexpr FieldBuffer FieldBuffer::GetSubBuffer(
    std::size_t offset, std::size_t len, BufferCategory cat) const {
  auto new_buffer_start = buffer + offset;
  if (offset > length) {
    throw InvalidInputBufferSize(
        length, ". Offset requested " + std::to_string(offset));
  }
  len = len == npos ? length - offset : len;
  if (offset + len > length) {
    throw InvalidInputBufferSize(
        len, ". Buffer remaininig size is " + std::to_string(length - offset));
  }
  if (cat == BufferCategory::kKeepCategory) {
    cat = category;
  }
  return {is_null, format, cat, len, new_buffer_start};
}

template <typename T>
std::size_t FieldBuffer::Read(T&& value, BufferCategory cat, std::size_t len) {
  ReadBinary(GetSubBuffer(0, len, cat), std::forward<T>(value));
  length -= len;
  buffer += len;
  return len;
}

template <typename T>
std::size_t FieldBuffer::Read(T&& value, const TypeBufferCategory& categories,
                              std::size_t len, BufferCategory cat) {
  ReadBinary(GetSubBuffer(0, len, cat), std::forward<T>(value), categories);
  length -= len;
  buffer += len;
  return len;
}

template <typename T>
std::size_t FieldBuffer::ReadRaw(T&& value,
                                 const TypeBufferCategory& categories,
                                 BufferCategory cat) {
  using ValueType = std::decay_t<T>;
  Integer field_length{0};
  auto consumed = Read(field_length, BufferCategory::kPlainBuffer);
  if (field_length == kPgNullBufferSize) {
    // NULL value
    traits::GetSetNull<ValueType>::SetNull(std::forward<T>(value));
    return consumed;
  } else if (field_length < 0) {
    // invalid length value
    throw InvalidInputBufferSize(0, "Negative buffer size value");
  } else if (field_length == 0) {
    traits::GetSetNull<ValueType>::SetDefault(std::forward<T>(value));
    return consumed;
  } else {
    return consumed + Read(value, categories, field_length, cat);
  }
}

template <typename T>
std::size_t ReadRawBinary(FieldBuffer buffer, T& value,
                          const TypeBufferCategory& categories) {
  return buffer.ReadRaw(value, categories);
}

namespace detail {

template <typename T, typename Buffer, typename Enable = ::utils::void_t<>>
struct FormatterAcceptsReplacementOid : std::false_type {};

template <typename T, typename Buffer>
struct FormatterAcceptsReplacementOid<
    T, Buffer,
    ::utils::void_t<decltype(std::declval<T&>()(
        std::declval<const UserTypes&>(), std::declval<Buffer&>(),
        std::declval<Oid>()))>> : std::true_type {};

}  // namespace detail

template <typename T, typename Buffer>
void WriteRawBinary(const UserTypes& types, Buffer& buffer, const T& value,
                    Oid replace_oid = kInvalidOid) {
  static_assert(
      (traits::HasFormatter<T, DataFormat::kBinaryDataFormat>::value == true),
      "Type doesn't have a binary formatter");
  static constexpr auto size_len = sizeof(Integer);
  if (traits::GetSetNull<T>::IsNull(value)) {
    WriteBinary(types, buffer, kPgNullBufferSize);
  } else {
    using BufferFormatter =
        typename traits::IO<T, DataFormat::kBinaryDataFormat>::FormatterType;
    using AcceptsReplacementOid =
        detail::FormatterAcceptsReplacementOid<BufferFormatter, Buffer>;
    auto len_start = buffer.size();
    buffer.resize(buffer.size() + size_len);
    auto size_before = buffer.size();
    if constexpr (AcceptsReplacementOid{}) {
      BufferFormatter{value}(types, buffer, replace_oid);
    } else {
      WriteBuffer<DataFormat::kBinaryDataFormat>(types, buffer, value);
    }
    Integer bytes = buffer.size() - size_before;
    BufferWriter<DataFormat::kBinaryDataFormat>(bytes)(buffer.begin() +
                                                       len_start);
  }
}

}  // namespace storages::postgres::io
