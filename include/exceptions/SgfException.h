#pragma once

#include "SgfReturnCode.h"

#include <exception>
#include <string>

namespace sgf {

/**
 * @brief Abstract base for all subgraph_filter_suite exceptions.
 *
 * Every exception thrown by this package inherits from SgfException.
 * The message is retrievable via what(). CLI tools cast return_code()
 * to int and pass it to exit(). The C++ API guarantees it never
 * propagates anything that does not derive from SgfException.
 */
class SgfException : public std::exception {
public:
    /**
     * @brief Constructs an SgfException with a message.
     * @param message Human-readable description of the error.
     */
    explicit SgfException(std::string message) : m_message(std::move(message))
    {
    }

    /**
     * @brief Returns the error message.
     *
     * The returned pointer is valid for the lifetime of this exception object.
     *
     * @return Null-terminated error description.
     */
    const char* what() const noexcept override
    {
        return m_message.c_str();
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     *
     * Each concrete subclass returns a unique SgfReturnCode value so that
     * CLI tools can report distinct failure modes via the process exit status.
     *
     * @return The SgfReturnCode identifying this exception type.
     */
    virtual SgfReturnCode return_code() const noexcept = 0;

    /**
     * @brief Default destructor.
     */
    ~SgfException() override = default;

private:
    std::string m_message;
};

}  // namespace sgf
