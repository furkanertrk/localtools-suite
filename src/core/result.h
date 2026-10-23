#ifndef LOCALTOOLS_CORE_RESULT_H
#define LOCALTOOLS_CORE_RESULT_H

#include "error.h"
#include <variant>
#include <type_traits>

namespace LocalTools::Core {

template <typename T>
class Result {
public:
    Result(const T& value) : m_data(value) {}
    Result(T&& value) : m_data(std::move(value)) {}
    Result(const PdfError& error) : m_data(error) {}
    Result(PdfError&& error) : m_data(std::move(error)) {}

    bool hasValue() const {
        return std::holds_alternative<T>(m_data);
    }

    bool hasError() const {
        return std::holds_alternative<PdfError>(m_data);
    }

    explicit operator bool() const {
        return hasValue();
    }

    const T& value() const {
        return std::get<T>(m_data);
    }

    T& value() {
        return std::get<T>(m_data);
    }

    T valueOr(const T& fallback) const {
        return hasValue() ? std::get<T>(m_data) : fallback;
    }

    const PdfError& error() const {
        return std::get<PdfError>(m_data);
    }

    static Result<T> ok(T value) {
        return Result<T>(std::move(value));
    }

    static Result<T> fail(PdfError error) {
        return Result<T>(std::move(error));
    }

    static Result<T> fail(PdfErrorCategory cat, const std::string& msg, const std::string& details = "") {
        return Result<T>(PdfError::create(cat, msg, details));
    }

private:
    std::variant<T, PdfError> m_data;
};

// Specialization for void
template <>
class Result<void> {
public:
    Result() : m_error(PdfError::none()) {}
    Result(const PdfError& error) : m_error(error) {}
    Result(PdfError&& error) : m_error(std::move(error)) {}

    bool hasValue() const { return m_error.isSuccess(); }
    bool hasError() const { return m_error.isError(); }
    explicit operator bool() const { return hasValue(); }

    const PdfError& error() const { return m_error; }

    static Result<void> ok() {
        return Result<void>();
    }

    static Result<void> fail(PdfError error) {
        return Result<void>(std::move(error));
    }

    static Result<void> fail(PdfErrorCategory cat, const std::string& msg, const std::string& details = "") {
        return Result<void>(PdfError::create(cat, msg, details));
    }

private:
    PdfError m_error;
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_RESULT_H
