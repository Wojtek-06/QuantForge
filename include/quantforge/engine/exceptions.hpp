#pragma once

#include <stdexcept>
#include <string>

namespace quantforge::engine {

class InvalidOrder final : public std::invalid_argument {
public:
    explicit InvalidOrder(const std::string& message)
        : std::invalid_argument(message)
    {
    }
};

class DuplicateOrderId final : public std::invalid_argument {
public:
    explicit DuplicateOrderId(const std::string& message)
        : std::invalid_argument(message)
    {
    }
};

} // namespace quantforge::engine
